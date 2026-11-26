// CCES Meshtastic Cloudflare pilot: authenticated outbound gateway protocol.
// This Worker is intentionally separate from existing lift-station Workers.
const json=(value,status=200)=>new Response(JSON.stringify(value),{status,headers:{"content-type":"application/json; charset=utf-8","cache-control":"no-store","x-content-type-options":"nosniff"}});
function authorized(request,secret){
  const supplied=request.headers.get("x-ingest-key")||"";
  // Use SHA-256 comparison to avoid an ordinary early-exit plain-text compare.
  return supplied.length>0&&secret.length>0&&supplied.length===secret.length&&crypto.subtle;
}
async function sameKey(given,expected){
  if(!given||!expected||given.length!==expected.length) return false;
  const enc=new TextEncoder();
  const [a,b]=await Promise.all([crypto.subtle.digest("SHA-256",enc.encode(given)),crypto.subtle.digest("SHA-256",enc.encode(expected))]);
  const A=new Uint8Array(a),B=new Uint8Array(b);let diff=0;for(let i=0;i<A.length;i++)diff|=A[i]^B[i];return diff===0;
}
async function signedManifest(secret,version,url,sha){
  const encoder=new TextEncoder();
  const key=await crypto.subtle.importKey("raw",encoder.encode(secret),{name:"HMAC",hash:"SHA-256"},false,["sign"]);
  const input=version+"\n"+url+"\n"+sha;
  const signature=await crypto.subtle.sign("HMAC",key,encoder.encode(input));
  return [...new Uint8Array(signature)].map(b=>b.toString(16).padStart(2,"0")).join("");
}
export default {
  async fetch(request,env){
    const url=new URL(request.url);
    if(url.pathname==="/healthz"&&request.method==="GET")
      return json({service:"cces-mesh-remote",status:"ready",schema:1});
    if(!await sameKey(request.headers.get("x-ingest-key")||"",env.GATEWAY_KEY||""))
      return json({error:"unauthorized"},401);
    try{
      if(url.pathname==="/v10/config"&&request.method==="GET"){
        const [stations,gate]=await Promise.all([
          env.DB.prepare("SELECT node_num,node_id,long_name,sensor_type,report_interval_minutes FROM mesh_stations WHERE enabled=1 ORDER BY node_num LIMIT 24").all(),
          env.DB.prepare("SELECT version,ota_enabled,ota_version,ota_url,ota_sha256 FROM mesh_gateway_config WHERE gateway_id='heltec-moab'").first()
        ]);
        if(!gate) return json({error:"config missing"},503);
        const candidate={enabled:!!gate.ota_enabled,version:String(gate.ota_version||""),url:String(gate.ota_url||""),sha256:String(gate.ota_sha256||"").toLowerCase(),signature:""};
        if(candidate.enabled&&/^https:\/\/(raw\.githubusercontent\.com|github\.com)\//.test(candidate.url)&&/^[0-9a-f]{64}$/.test(candidate.sha256)&&candidate.version.length<90){
          candidate.signature=await signedManifest(env.GATEWAY_KEY,candidate.version,candidate.url,candidate.sha256);
        }else candidate.enabled=false;
        return json({gateway:"heltec-moab",version:Number(gate.version),poll_seconds:300,
          nodes:(stations.results||[]).map(s=>({id:Number(s.node_num),name:s.long_name,sensor_type:Number(s.sensor_type),destination:"cloudflare"})),
          ota:candidate});
      }
      if(url.pathname==="/v10/ingest"&&request.method==="POST"){
        const length=Number(request.headers.get("content-length")||0);
        if(length>8192) return json({error:"payload too large"},413);
        const raw=await request.text();
        if(raw.length>8192) return json({error:"payload too large"},413);
        let p;try{p=JSON.parse(raw)}catch{return json({error:"bad json"},400)}
        const id=Number(p.from),packet=Number(p.packet_id),data=p.payload||{};
        if(!Number.isInteger(id)||id<=0||!Number.isInteger(packet)||packet<=0||!["water_distance","device","telemetry"].includes(p.type))
          return json({error:"bad telemetry identity/type"},400);
        const station=await env.DB.prepare("SELECT sensor_type FROM mesh_stations WHERE node_num=? AND enabled=1").bind(id).first();
        if(!station)return json({error:"unregistered node"},403);
        if(p.type==="water_distance"){
          if(Number(data.sensor_type)!==Number(station.sensor_type))return json({error:"sensor type mismatch"},400);
          const valid=data.distance_valid===true;
          const calibrated=valid&&data.stage_calibrated===true&&Number.isInteger(data.stage_mm);
          const mm=valid&&Number.isInteger(data.distance_mm)&&data.distance_mm>=0&&data.distance_mm<=100000?data.distance_mm:null;
          if(valid&&mm===null)return json({error:"bad distance"},400);
          const seq=Number.isInteger(data.sequence)?data.sequence:null;
          const ts=Number.isInteger(data.sensor_timestamp)?data.sensor_timestamp:null;
          await env.DB.prepare("INSERT INTO mesh_observations(node_num,packet_id,sensor_type,sensor_timestamp,sequence,distance_valid,stage_calibrated,distance_mm,stage_mm,raw_payload) VALUES (?,?,?,?,?,?,?,?,?,?) ON CONFLICT(node_num,packet_id) DO NOTHING")
          .bind(id,packet,Number(station.sensor_type),ts,seq,valid?1:0,calibrated?1:0,mm,calibrated?data.stage_mm:null,raw.slice(0,4096)).run();
        }else{
          const pct=Number(data.battery_level??data.battery_percent);
          const volts=Number(data.voltage??data.battery_voltage);
          await env.DB.prepare("INSERT INTO mesh_device_telemetry(node_num,packet_id,battery_percent,battery_voltage,uptime_seconds,raw_payload) VALUES (?,?,?,?,?,?) ON CONFLICT(node_num,packet_id) DO NOTHING")
          .bind(id,packet,Number.isFinite(pct)&&pct>=0&&pct<=100?pct:null,Number.isFinite(volts)&&volts>=0&&volts<=100?volts:null,Number.isFinite(Number(data.uptime_seconds))?Number(data.uptime_seconds):null,raw.slice(0,4096)).run();
        }
        await env.DB.prepare("UPDATE mesh_stations SET last_received_at=CURRENT_TIMESTAMP WHERE node_num=?").bind(id).run();
        return json({ok:true,node_num:id});
      }
      if(url.pathname==="/v10/health"&&request.method==="POST"){
        const raw=await request.text();if(raw.length>4096)return json({error:"too large"},413);
        const d=JSON.parse(raw),uptime=Number(d.uptime_seconds),queue=Number(d.queue_depth),status=Number(d.last_http_status),ver=Number(d.config_version);
        await env.DB.prepare("INSERT INTO mesh_gateway_health(gateway_id,firmware,uptime_seconds,queue_depth,last_http_status,config_version,note) VALUES (?,?,?,?,?,?,?)")
          .bind("heltec-moab",String(d.firmware||"").slice(0,90),Number.isFinite(uptime)?uptime:null,Number.isFinite(queue)?queue:null,Number.isFinite(status)?status:null,Number.isFinite(ver)?ver:null,String(d.note||"").slice(0,300)).run();
        return json({ok:true});
      }
      return json({error:"not found"},404);
    }catch(e){return json({error:"internal error"},500)}
  }
};
