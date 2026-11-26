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
    // Immutable opaque download link; the URL token is 256 bits of entropy.
    // Only signed, SHA-pinned firmware manifests point gateways to this file.
    const fwMatch=url.pathname.match(/^\/v10\/firmware\/([a-f0-9]{64})$/);
    if(fwMatch&&request.method==="GET"){
      if(!env.FW_BIN)return json({error:"firmware storage unavailable"},503);
      const bytes=await env.FW_BIN.get("heltec-v10/"+fwMatch[1],"arrayBuffer");
      if(!bytes)return json({error:"firmware not found"},404);
      if(bytes.byteLength<4096||new Uint8Array(bytes)[0]!==0xe9)return json({error:"invalid firmware object"},500);
      return new Response(bytes,{status:200,headers:{"content-type":"application/octet-stream","content-length":String(bytes.byteLength),"cache-control":"no-store","x-content-type-options":"nosniff"}});
    }
    if(!await sameKey(request.headers.get("x-ingest-key")||"",env.GATEWAY_KEY||""))
      return json({error:"unauthorized"},401);
    try{
      if(url.pathname==="/v10/config"&&request.method==="GET"){
        const [stations,gate,dm]=await Promise.all([
          env.DB.prepare("SELECT node_num,node_id,long_name,sensor_type,report_interval_minutes FROM mesh_stations WHERE enabled=1 ORDER BY node_num LIMIT 24").all(),
          env.DB.prepare("SELECT version,ota_enabled,ota_version,ota_url,ota_sha256 FROM mesh_gateway_config WHERE gateway_id='heltec-moab'").first(),
          env.DB.prepare("SELECT node_num,enabled,interval_minutes FROM mesh_gateway_dm_overrides ORDER BY node_num LIMIT 16").all()
        ]);
        if(!gate) return json({error:"config missing"},503);
        const candidate={enabled:!!gate.ota_enabled,version:String(gate.ota_version||""),url:String(gate.ota_url||""),sha256:String(gate.ota_sha256||"").toLowerCase(),signature:""};
        if(candidate.enabled&&(/^https:\/\/(raw\.githubusercontent\.com|github\.com)\//.test(candidate.url)||/^https:\/\/cces-mesh-remote\.raspy-lake-a7f4\.workers\.dev\/v10\/firmware\/[a-f0-9]{64}$/.test(candidate.url))&&/^[0-9a-f]{64}$/.test(candidate.sha256)&&candidate.version.length<90){
          candidate.signature=await signedManifest(env.GATEWAY_KEY,candidate.version,candidate.url,candidate.sha256);
        }else candidate.enabled=false;
        return json({gateway:"heltec-moab",version:Number(gate.version),poll_seconds:300,
          nodes:(stations.results||[]).map(s=>({id:Number(s.node_num),name:s.long_name,sensor_type:Number(s.sensor_type),destination:"cloudflare"})),
          auto_dm:(dm.results||[]).map(s=>({id:Number(s.node_num),enabled:!!s.enabled,interval_minutes:Number(s.interval_minutes)})),
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
        const note=String(d.note||"").slice(0,300);
        await env.DB.prepare("INSERT INTO mesh_gateway_health(gateway_id,firmware,uptime_seconds,queue_depth,last_http_status,config_version,note) VALUES (?,?,?,?,?,?,?)")
          .bind("heltec-moab",String(d.firmware||"").slice(0,90),Number.isFinite(uptime)?uptime:null,Number.isFinite(queue)?queue:null,Number.isFinite(status)?status:null,Number.isFinite(ver)?ver:null,note).run();
        // Optional V12+ effective auto-DM snapshot, authenticated with the same
        // gateway ingest secret as health. V11 continues posting health unchanged.
        // Incomplete/malformed snapshots never block remote-OTA boot confirmation.
        // Retain last boot reset reason from ESP32 diagnostics; V11 omits these.
        if(typeof d.reset_diagnostic==="string"&&d.reset_diagnostic.startsWith("HELTEC V4 DIAG")&&d.reset_diagnostic.length<=650&&
           Number.isInteger(d.reset_reason_code)&&d.reset_reason_code>=0&&d.reset_reason_code<256){
          try{
            await env.DB.prepare("INSERT INTO mesh_gateway_reset_diag(gateway_id,firmware,uptime_seconds,reset_reason,detail) VALUES ('heltec-moab',?,?,?,?)")
              .bind(String(d.firmware||"").slice(0,90),Number.isFinite(uptime)?uptime:null,d.reset_reason_code,d.reset_diagnostic).run();
          }catch(_ignored){console.warn("Reset diagnostics storage unavailable")}
        }
        const dm=d.auto_dm_state;
        if(dm&&typeof dm==="object"){
          const allowed=new Set([1949224949,1695962694,1683604927,2004386937,2425602407,3044869407,1577197109,647393146,2650172798,4241345683]);
          const cadence=new Set([15,30,45,60,360,720,1440]);
          const valid=dm.schema===1&&typeof dm.global_enabled==="boolean"&&Number(dm.minimum_missing_minutes)===60&&
            typeof dm.clock_synchronized==="boolean"&&Array.isArray(dm.watches)&&dm.watches.length===11&&
            Array.isArray(dm.configured_polls)&&dm.configured_polls.length<=4&&
            dm.watches.every(w=>allowed.has(w.node)&&Number.isInteger(w.kind)&&w.kind>=0&&w.kind<=2&&
              typeof w.enabled==="boolean"&&cadence.has(w.interval_minutes)&&typeof w.custom_interval==="boolean"&&
              (w.last_auto_age_seconds===null||(Number.isInteger(w.last_auto_age_seconds)&&w.last_auto_age_seconds>=0))&&
              (w.last_request_age_seconds===null||(Number.isInteger(w.last_request_age_seconds)&&w.last_request_age_seconds>=0)))&&
            dm.configured_polls.every(x=>Number.isInteger(x.node)&&x.node>=0&&x.node<=4294967295&&
              Number.isInteger(x.interval_minutes)&&x.interval_minutes>=0&&x.interval_minutes<=1440&&
              Number.isInteger(x.mode)&&x.mode>=0&&x.mode<=2);
          const snapshot=valid?JSON.stringify(dm):"";
          if(valid&&snapshot.length<=6000){
            try {
              await env.DB.prepare("INSERT INTO mesh_gateway_dm_state(gateway_id,reported_at,firmware,config_version,snapshot_json) VALUES ('heltec-moab',CURRENT_TIMESTAMP,?,?,?) ON CONFLICT(gateway_id) DO UPDATE SET reported_at=CURRENT_TIMESTAMP,firmware=excluded.firmware,config_version=excluded.config_version,snapshot_json=excluded.snapshot_json")
                .bind(String(d.firmware||"").slice(0,90),Number.isInteger(ver)?ver:0,snapshot).run();
            }catch(_ignored){console.warn("Optional auto-DM snapshot storage failed")}
          }
        }
        // Close the release authorization only after a matching installed image
        // boots far enough to authenticate and post its first cloud heartbeat.
        const verified=note.match(/^remote-ota: loader_ready=1,phase=(3|4),attempts=0,applied=([a-zA-Z0-9._-]{3,63})(?:,|$)/);
        if(verified){
          const done=await env.DB.prepare("UPDATE mesh_gateway_config SET ota_enabled=0,version=version+1,updated_at=CURRENT_TIMESTAMP WHERE gateway_id='heltec-moab' AND ota_enabled=1 AND ota_version=? RETURNING version").bind(verified[2]).first();
          if(done){
            const release=await env.DB.prepare("SELECT id FROM mesh_firmware_releases WHERE gateway_id='heltec-moab' AND version=?").bind(verified[2]).first();
            await env.DB.prepare("INSERT INTO mesh_firmware_actions(id,gateway_id,action,release_id,config_version,actor_email,note) VALUES (?,'heltec-moab','verified-boot',?,?,?,?)")
              .bind(crypto.randomUUID(),release?.id||null,done.version,'gateway','Gateway boot confirmed; OTA authorization disabled').run();
          }
        }
        return json({ok:true});
      }
      return json({error:"not found"},404);
    }catch(e){return json({error:"internal error"},500)}
  }
};
