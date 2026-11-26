const dashboard = `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>CCES Monitor</title>
<style>
:root{font-family:Inter,ui-sans-serif,system-ui,Arial,sans-serif;color:#eaf2f1;background:#071519;--panel:#102329;--line:#274047;--muted:#92aaa9;--aqua:#6ee1c7;--red:#ff786e;--amber:#f3c57c}*{box-sizing:border-box}body{margin:0}button,input,select{font:inherit}button{cursor:pointer}.shell{max-width:1320px;margin:auto;padding:24px 30px 80px}.top{display:flex;align-items:center;justify-content:space-between;gap:20px;padding:15px 0 38px}.brand{font-weight:800;letter-spacing:.11em;font-size:15px}.brand b{color:var(--aqua)}.top small{color:var(--muted)}.eyebrow{color:var(--aqua);text-transform:uppercase;letter-spacing:.14em;font-size:11px;font-weight:800}h1{font-size:clamp(32px,4vw,56px);line-height:1.04;margin:12px 0 12px;letter-spacing:-.05em}h2{font-size:20px;margin:0}p{color:var(--muted);line-height:1.6}.hero{display:flex;align-items:end;justify-content:space-between;gap:20px;margin-bottom:28px}.hero p{margin:0}.refresh{background:var(--aqua);border:0;color:#06211c;padding:12px 16px;border-radius:9px;font-weight:800}.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;margin:28px 0 38px}.stat,.card,.admin-panel,.history{background:var(--panel);border:1px solid var(--line);border-radius:14px}.stat{padding:20px}.stat span{color:var(--muted);font-size:12px}.stat strong{display:block;font-size:32px;margin-top:8px;letter-spacing:-.04em}.sectionhead{display:flex;justify-content:space-between;align-items:center;margin:0 0 16px}.sectionhead span{color:var(--muted);font-size:12px}.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px}.card{padding:19px;transition:border-color .2s}.card:hover{border-color:#5a8984}.card h3{margin:15px 0 6px;font-size:20px}.card small{color:var(--muted)}.cardtop{display:flex;justify-content:space-between;align-items:center}.pill{display:inline-flex;align-items:center;border-radius:30px;padding:6px 10px;font-size:11px;font-weight:800;text-transform:uppercase;letter-spacing:.05em;background:#163a33;color:var(--aqua)}.pill.bad{background:#482824;color:var(--red)}.pill.warn{background:#433621;color:var(--amber)}.metrics{display:flex;gap:22px;border-top:1px solid var(--line);margin-top:20px;padding-top:17px}.metrics div{flex:1}.metrics span{display:block;color:var(--muted);font-size:11px;margin-bottom:6px}.metrics b{font-size:14px}.link{color:var(--aqua);background:none;border:0;padding:0;font-weight:700}.notice{padding:13px 16px;margin:18px 0;border:1px solid #806738;border-radius:10px;color:var(--amber);background:#30291c}.empty{border:1px dashed var(--line);border-radius:14px;padding:42px;text-align:center;color:var(--muted)}.admin-panel{margin-top:40px;padding:24px}.admin-panel p{margin:8px 0 22px}.admin-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}.formbox{padding:16px;border:1px solid var(--line);border-radius:11px}.formbox h3{margin:0 0 15px}.fields{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}.fields label{display:block;color:var(--muted);font-size:12px}.fields input,.fields select{display:block;margin-top:6px;width:100%;background:#091a1e;color:#eaf2f1;border:1px solid #3c5559;border-radius:7px;padding:10px}.formbox button[type=submit]{margin-top:16px;border:0;border-radius:7px;background:var(--aqua);color:#06211c;font-weight:800;padding:9px 15px}.key{word-break:break-all;background:#071519;border:1px solid var(--aqua);padding:14px;border-radius:8px;margin-top:16px}.history{margin-top:22px;padding:22px;display:none}.history.show{display:block}.history table{border-collapse:collapse;width:100%;margin-top:15px;font-size:13px}td,th{text-align:left;padding:10px;border-bottom:1px solid var(--line)}th{color:var(--muted)}.footer{color:var(--muted);font-size:12px;margin-top:35px}#message{color:var(--aqua);min-height:22px}@media(max-width:950px){.grid{grid-template-columns:repeat(2,1fr)}.stats{grid-template-columns:repeat(2,1fr)}}@media(max-width:650px){.shell{padding:18px 16px 60px}.top{padding-bottom:25px}.hero{display:block}.hero button{margin-top:18px}.grid,.admin-grid{grid-template-columns:1fr}.stats{gap:8px}.stat{padding:14px}.stat strong{font-size:26px}.fields{grid-template-columns:1fr}.history{overflow-x:auto}}
</style><style>
.nav{display:flex;gap:8px;border-bottom:1px solid var(--line);margin:-14px 0 45px;padding-bottom:13px}.nav button{border:0;border-radius:8px;background:transparent;color:var(--muted);font-weight:700;padding:10px 14px}.nav button[aria-current=page]{background:#17342f;color:var(--aqua)}[hidden]{display:none!important}.hub-intro{max-width:760px;margin:8px 0 30px}.hub-intro p{font-size:17px}.project-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}.project{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:26px;min-height:235px;display:flex;flex-direction:column;align-items:flex-start;text-align:left;color:inherit}.project.action:hover{border-color:var(--aqua)}.project h2{font-size:26px;margin:25px 0 7px}.project p{margin:0}.project .project-foot{margin-top:auto;padding-top:24px;font-size:13px;color:var(--aqua);font-weight:800}.project .project-foot.muted{color:var(--muted)}.hub-note{margin-top:24px;color:var(--muted);font-size:13px}.back{background:none;border:0;color:var(--aqua);padding:0;margin-bottom:18px;font-weight:800}.lift-intro{max-width:720px;margin-bottom:26px}.admin-panel details{border-top:1px solid var(--line);padding-top:18px}.admin-panel summary{cursor:pointer;font-weight:800;color:var(--aqua)}.admin-panel details[open] summary{margin-bottom:18px}.tools{display:flex;gap:12px;margin-top:15px}.tools button,.access-row button{background:none;border:0;color:var(--aqua);padding:0;font-weight:700}.access-row{display:flex;justify-content:space-between;gap:15px;border-top:1px solid var(--line);padding:12px 0}.access-row small{display:block;color:var(--muted)}#access-list{margin-top:20px}.key{white-space:pre-wrap}
.filters{display:flex;gap:8px;align-items:center}.filters input,.filters select,.range select{background:#0a1d21;border:1px solid var(--line);color:inherit;padding:9px 11px;border-radius:9px}.filters input{width:170px}.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}.metrics.three{gap:12px}.metrics.three b{font-size:13px}.history{display:none}.history.show{display:block}.history-head{display:flex;align-items:center;justify-content:space-between;gap:15px;flex-wrap:wrap}.history-actions{display:flex;align-items:center;gap:12px;flex-wrap:wrap}.history-actions button{border:1px solid var(--line);background:#17342f;color:var(--aqua);border-radius:8px;padding:8px 12px;font-weight:700}.history-actions .link{border:0;background:none}.trend-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px;margin-top:20px}.trend{border:1px solid var(--line);background:#0b1e23;border-radius:12px;padding:16px;min-width:0}.trend h3{margin:0 0 4px;font-size:16px}.trend small,.chart-foot{color:var(--muted);font-size:12px}.trend svg{display:block;width:100%;height:165px;margin-top:12px}.chart-foot{display:flex;justify-content:space-between;gap:10px}.reading-summary{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:18px}.reading-summary div{background:#0b1e23;border:1px solid var(--line);border-radius:10px;padding:13px}.reading-summary small{color:var(--muted);display:block}.reading-summary strong{display:block;font-size:19px;margin-top:6px}.event-list{margin:17px 0 0;padding:0;list-style:none}.event-list li{padding:10px 0;border-bottom:1px solid var(--line);font-size:13px;color:var(--muted)}.event-list b{color:var(--amber)}.readings-table{overflow:auto;max-height:400px;margin-top:15px}.readings-table table{margin-top:0;min-width:620px}.readings-table caption{text-align:left;color:var(--muted);margin:12px 0}.readings-table th{position:sticky;top:0;background:#102329}.trend-empty{display:grid;place-items:center;height:165px;color:var(--muted);font-size:13px}
@media(max-width:650px){.nav{margin:0 0 30px}.project-grid{grid-template-columns:1fr}.project{min-height:200px}.filters{width:100%;flex-wrap:wrap}.filters input{flex:1;min-width:130px}.sectionhead{flex-wrap:wrap;gap:10px}.trend-grid{grid-template-columns:1fr}.reading-summary{grid-template-columns:1fr 1fr}}
.diagnostic-toolbar{display:flex;align-items:end;gap:12px;flex-wrap:wrap;margin:26px 0}.diagnostic-toolbar label{color:var(--muted);font-size:12px;font-weight:700}.diagnostic-toolbar select{display:block;margin-top:6px;min-width:170px;background:#0a1d21;border:1px solid var(--line);color:var(--aqua);padding:11px;border-radius:9px}.diagnostic-toolbar button{border:0;background:var(--aqua);color:#06211c;border-radius:9px;padding:11px 15px;font-weight:800}.diagnostic-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px;margin:20px 0}.diagnostic-card,.diagnostic-guide,.answer-card{background:var(--panel);border:1px solid var(--line);border-radius:13px;padding:20px}.diagnostic-card h3{font-size:20px;margin:3px 0 16px}.diagnostic-card dl{display:grid;grid-template-columns:1fr auto;gap:10px;margin:0}.diagnostic-card dt{color:var(--muted)}.diagnostic-card dd{margin:0;text-align:right;font-weight:800}.diagnostic-guide{margin:25px 0}.diagnostic-guide ol{padding-left:22px;color:var(--muted);line-height:1.7}.diagnostic-guide li{margin:8px 0}.diagnostic-guide b{color:#eaf2f1}.diagnostic-guide p{margin-bottom:0}.answer-card{margin-top:16px}.answer-card h2{margin-bottom:15px}.answer-row{padding:12px 0;border-top:1px solid var(--line)}.answer-row b{display:block}.answer-row span{display:block;color:var(--muted);margin-top:4px;line-height:1.5}.diagnostic-caveat{color:var(--muted);font-size:13px;margin-top:14px}@media(max-width:650px){.diagnostic-grid{grid-template-columns:1fr}.diagnostic-toolbar label,.diagnostic-toolbar select{width:100%}}
.mesh-toolbar{display:flex;align-items:center;gap:12px;margin:12px 0 24px;flex-wrap:wrap}.mesh-toolbar label,.mesh-toolbar small{color:var(--muted)}.mesh-toolbar select{background:var(--panel);color:#fff;border:1px solid var(--line);border-radius:8px;padding:10px}.mesh-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.mesh-grid canvas{width:100%;height:160px;display:block;background:#091b20;border:1px solid var(--line);border-radius:9px;margin-top:8px}.mesh-grid .metrics{flex-wrap:wrap}.mesh-grid .metrics div{min-width:100px}.mesh-chart-label{font-size:12px;color:var(--muted);margin-top:16px}.mesh-grid .card h3{margin:10px 0}.mesh-grid .card p{margin:7px 0}@media(max-width:800px){.mesh-grid{grid-template-columns:1fr}}
body.mesh-active{background:#f5f7f8;color:#202c35;font-family:Arial,Helvetica,sans-serif}body.mesh-active .shell{max-width:1208px;padding:0 24px 60px}body.mesh-active .top{background:#fff;border-bottom:1px solid #d6dde2;margin:0 -24px;padding:18px 24px}body.mesh-active .brand b{color:#285e78}body.mesh-active .top small{color:#62717c}body.mesh-active .nav{margin:0 -24px;padding:9px 24px;background:#fff;border-bottom:1px solid #d6dde2;flex-wrap:wrap}body.mesh-active .nav button{color:#526775;border-radius:3px}body.mesh-active .nav button[aria-current=page]{background:#285e78;color:#fff}
#mesh-view{--mesh-muted:#62717c;--mesh-line:#d6dde2;--mesh-blue:#285e78;padding:24px 0 55px}#mesh-view .back{color:var(--mesh-blue);font-size:12px;margin:0 0 20px}.mesh-hero{padding:14px 0 28px}.mesh-eyebrow{margin:0 0 7px;color:var(--mesh-muted);font-size:11px;letter-spacing:.045em;font-weight:600}.mesh-hero h1{font-size:32px;font-weight:600;letter-spacing:0;margin:0 0 7px}.mesh-hero>p:not(.mesh-eyebrow){margin:0;color:var(--mesh-muted);font-size:14px}.mesh-picker{display:flex;align-items:center;gap:12px;margin-top:22px;font-size:12px}.mesh-picker select{min-width:220px;max-width:100%;padding:9px 12px;background:#fff;color:#202c35;border:1px solid #bdc9d1;border-radius:3px}.mesh-section{border-top:1px solid var(--mesh-line);padding:25px 0 30px}.mesh-heading,.mesh-statusline,.mesh-chart-head,.mesh-table-head{display:flex;align-items:center;justify-content:space-between;gap:16px}.mesh-heading h2{font-size:20px;font-weight:600}.mesh-heading>div>p:last-child:not(.mesh-eyebrow){margin:5px 0 0;color:var(--mesh-muted);font-size:12px}.mesh-heading>button,.mesh-ranges button{background:#fff;border:1px solid #bdc9d1;border-radius:3px;color:#202c35;padding:8px 12px;font-size:12px}.mesh-ranges{display:flex;gap:5px;flex-wrap:wrap}.mesh-ranges button[aria-pressed=true]{background:var(--mesh-blue);border-color:var(--mesh-blue);color:#fff}.mesh-statusline{font-size:11px;color:var(--mesh-muted);margin:16px 0 12px}.mesh-readings{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}.mesh-reading{background:#fff;border:1px solid var(--mesh-line);border-top:3px solid #668599;border-radius:3px;padding:20px;min-width:0;min-height:175px;display:flex;flex-direction:column}.mesh-reading:first-child{border-top-color:var(--mesh-blue)}.mesh-reading .label{font-size:12px;color:#202c35}.mesh-reading .value{display:flex;align-items:baseline;gap:8px;margin:8px 0 3px;font:400 38px ui-monospace,Consolas,monospace;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}.mesh-reading .value span{font:14px Arial,sans-serif;color:var(--mesh-muted)}.mesh-reading .time{font-size:11px;color:var(--mesh-muted);margin:0 0 15px}.mesh-reading .foot{font-size:11px;color:var(--mesh-muted);border-top:1px solid #e4e8eb;padding-top:11px;margin-top:auto}.mesh-chart-panel,.mesh-table-panel,.mesh-note{background:#fff;border:1px solid var(--mesh-line);border-radius:3px}.mesh-chart-panel{margin-top:16px}.mesh-chart-head{padding:17px 20px}.mesh-chart-head>span,.mesh-table-head>span{font-size:11px;color:var(--mesh-muted)}#mesh-plot{display:block;width:100%;height:320px;background:#fff}#mesh-chart-note{font-size:11px;color:var(--mesh-muted);margin:0;padding:12px 20px;border-top:1px solid var(--mesh-line)}.mesh-bottom{display:grid;grid-template-columns:1.9fr 1fr;gap:18px;margin:18px 0 0}.mesh-table-panel,.mesh-note{padding:20px;min-width:0}.mesh-table-head{margin-bottom:12px}.mesh-table-head h3,.mesh-note h3{font-size:16px;margin:0}.mesh-table-scroll{max-height:350px;overflow:auto}.mesh-table-panel table{width:100%;border-collapse:collapse;white-space:nowrap;font-size:11px}.mesh-table-panel th,.mesh-table-panel td{padding:10px 8px;border-bottom:1px solid #e3e8ec;text-align:right}.mesh-table-panel th:first-child,.mesh-table-panel td:first-child{text-align:left}.mesh-table-panel th{color:#495d6a;background:#f5f7f9;position:sticky;top:0}.mesh-table-panel td{font-family:ui-monospace,Consolas,monospace}.mesh-note dl{margin:12px 0;font-size:12px}.mesh-note dt{font-weight:600;margin:12px 0 3px}.mesh-note dd,.mesh-note p{margin:0;color:var(--mesh-muted);font-size:12px;line-height:1.5}.mesh-note p{border-top:1px solid var(--mesh-line);padding-top:12px;margin-top:16px}@media(max-width:800px){.mesh-bottom{grid-template-columns:1fr}.mesh-reading .value{font-size:31px}}@media(max-width:600px){body.mesh-active .shell{padding:0 16px 50px}body.mesh-active .top,body.mesh-active .nav{margin:0 -16px;padding-left:16px;padding-right:16px}.mesh-readings{grid-template-columns:1fr}.mesh-reading{min-height:150px;padding:16px}.mesh-heading,.mesh-statusline{align-items:flex-start;flex-wrap:wrap}.mesh-picker{align-items:flex-start;flex-direction:column;gap:6px}.mesh-picker select{width:100%}#mesh-plot{height:260px}.mesh-chart-head{align-items:flex-start;flex-direction:column}}
.mesh-remote-stats{display:flex;flex-wrap:wrap;gap:12px;margin:16px 0}.mesh-remote-chip{background:var(--panel);border:1px solid var(--line);border-radius:9px;padding:10px 14px;flex:1;min-width:140px}.mesh-remote-chip small{display:block;color:var(--muted);font-size:11px}.mesh-remote-chip strong{display:block;margin-top:4px;font-size:15px}.mesh-remote-forms{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}.mesh-remote-forms form{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:17px;display:flex;flex-direction:column;gap:10px}.mesh-remote-forms form h3{margin:0 0 8px}.mesh-remote-forms form label{font-size:12px;color:var(--muted);display:flex;flex-direction:column;gap:5px}.mesh-remote-forms form label.mesh-check{flex-direction:row;align-items:center}.mesh-remote-forms input:not([type=checkbox]),.mesh-remote-forms select{background:#071519;border:1px solid var(--line);border-radius:7px;color:#eaf2f1;padding:10px;max-width:100%}.mesh-remote-forms button,#mesh-remote-refresh{border:1px solid var(--line);border-radius:9px;background:var(--aqua);color:#06211c;font-weight:750;padding:11px;cursor:pointer}.mesh-remote-forms button:disabled{opacity:.45;cursor:not-allowed}.mesh-remote-forms small{color:var(--muted);font-size:11px;line-height:1.4}#mesh-remote-message{min-height:20px;color:var(--aqua)}@media(max-width:920px){.mesh-remote-forms{grid-template-columns:1fr}}
/* Unified navigation and quiet administrative tools. */
body.mesh-active .nav{gap:6px}body.mesh-active .nav .nav-setup{font-size:10px;opacity:.75}
#mesh-view .mesh-admin{display:block;background:#fff;border:1px solid #d6dde2;border-radius:3px;padding:0;margin-top:10px}
#mesh-view .mesh-admin>summary{cursor:pointer;list-style:none;padding:18px 20px;font-weight:600;color:#285e78;font-size:14px}
#mesh-view .mesh-admin>summary::-webkit-details-marker{display:none}
#mesh-view .mesh-admin>summary:before{content:'▸';margin-right:9px}#mesh-view .mesh-admin[open]>summary:before{content:'▾'}
#mesh-view .mesh-admin>summary span{display:inline-block;color:#62717c;font-weight:400;font-size:11px;margin-left:10px}
#mesh-view .mesh-admin[open]>summary{border-bottom:1px solid #d6dde2}
#mesh-view .mesh-admin>.mesh-heading,#mesh-view .mesh-admin>.mesh-remote-stats,#mesh-view .mesh-admin>.mesh-remote-forms,#mesh-view .mesh-admin>#mesh-remote-message{margin-left:20px;margin-right:20px}
#mesh-view .mesh-admin>.mesh-heading{padding-top:18px}#mesh-view .mesh-admin>.mesh-remote-forms{margin-bottom:20px}
#mesh-view .mesh-remote-chip,#mesh-view .mesh-remote-forms form{background:#fff;border:1px solid #d6dde2;border-radius:3px;color:#202c35}
#mesh-view .mesh-remote-chip small,#mesh-view .mesh-remote-forms form label,#mesh-view .mesh-remote-forms small{color:#62717c}
#mesh-view .mesh-remote-forms input:not([type=checkbox]),#mesh-view .mesh-remote-forms select{background:#fff;border:1px solid #bdc9d1;color:#202c35;border-radius:3px;width:100%}
#mesh-view .mesh-remote-forms button,#mesh-view #mesh-remote-refresh{background:#285e78;border-color:#285e78;color:#fff;border-radius:3px}
#mesh-view #mesh-remote-message{color:#285e78;padding-bottom:12px}
@media(max-width:600px){#mesh-view .mesh-admin>summary span{display:block;margin:4px 0 0 22px}#mesh-view .mesh-admin>.mesh-heading{flex-wrap:wrap}}
/* One visual system across mesh, lift stations, and pump diagnostics. */
:root{color:#202c35;background:#f5f7f8;--panel:#fff;--line:#d6dde2;--muted:#62717c;--aqua:#285e78;--red:#a53f35;--amber:#80621e}
body{background:#f5f7f8;color:#202c35;font-family:Arial,Helvetica,sans-serif}
.shell{max-width:1208px;padding:0 24px 60px}.top{background:#fff;border-bottom:1px solid var(--line);margin:0 -24px;padding:18px 24px 17px}.brand b{color:var(--aqua)}
.nav{margin:0 -24px 20px;padding:9px 24px;background:#fff;border-bottom:1px solid var(--line);flex-wrap:wrap}.nav button{color:#526775;border-radius:3px}.nav button[aria-current=page]{background:var(--aqua);color:#fff}
.eyebrow{color:var(--muted)}h1{font-size:32px;letter-spacing:0;font-weight:600}h2{font-weight:600}p{color:var(--muted)}
.stat,.card,.admin-panel,.history,.diagnostic-card,.diagnostic-guide,.answer-card,.formbox,.trend,.reading-summary div{background:#fff;border-color:var(--line);border-radius:3px}
.card:hover{border-color:#8ca7b7}.pill{background:#e6f1ed;color:#285e78}.pill.warn{background:#fff5df;color:#80621e}.pill.bad{background:#fff0eb;color:#a53f35}
.refresh,.formbox button[type=submit],.diagnostic-toolbar button{background:var(--aqua);color:#fff;border-radius:3px}
.link,.back,.tools button,.access-row button{color:var(--aqua)}.notice{background:#fff8e8;border-color:#dbbf80;color:#70531d;border-radius:3px}
.fields input,.fields select,.filters input,.filters select,.range select,.diagnostic-toolbar select{background:#fff;color:#202c35;border:1px solid #bdc9d1;border-radius:3px}
.fields label,.diagnostic-toolbar label{color:var(--muted)}.diagnostic-toolbar select{color:#202c35}.diagnostic-card dd,.diagnostic-guide b{color:#202c35}
.history-actions button{background:#fff;border-color:#bdc9d1;color:var(--aqua)}.trend{background:#fff}.trend svg{background:#fff}.readings-table th{background:#f5f7f9}
#key .key{background:#f5f7f8;color:#202c35;border-color:#bdc9d1}.nav-setup{font-size:10px;opacity:.75}.admin-panel summary{color:var(--aqua)}
.footer{border-top:1px solid var(--line);padding-top:18px}#lift-view,#pumps-view{padding-top:20px}.hero{margin-bottom:24px}.hero h1{margin-top:8px}
@media(max-width:600px){.shell{padding:0 16px 50px}.top,.nav{margin-left:-16px;margin-right:-16px;padding-left:16px;padding-right:16px}}
</style></head><body><main class="shell"><header class="top"><div class="brand">CANYON COUNTRY <b>·</b> ECOLOGICAL SERVICES</div><small id="signedin">Loading account…</small></header>
<nav class="nav" aria-label="Monitoring sections"><button type="button" data-view="mesh" aria-current="page">Water monitoring</button><button type="button" data-view="lift">Lift stations <span class="nav-setup">· setup</span></button><button type="button" data-view="pumps">Pump diagnostics</button></nav>
<section id="mesh-view" hidden>
<section class="mesh-hero"><p class="mesh-eyebrow">CANYON COUNTRY ECOLOGICAL SERVICES · MESHTASTIC</p><h1>Water monitoring</h1><p>Live water-distance observations from the southeastern Utah mesh network.</p><label class="mesh-picker">Station <select id="mesh-station"></select></label></section>
<section class="mesh-section"><div class="mesh-heading"><div><p class="mesh-eyebrow">LATEST OBSERVATIONS</p><h2 id="mesh-title">Station</h2><p id="mesh-subtitle">Waiting for station details</p></div><button id="mesh-refresh" type="button">↻ Refresh readings</button></div><div class="mesh-statusline"><span id="mesh-status" role="status">Loading…</span><span id="mesh-updated">Times shown in Mountain Time</span></div><div id="mesh-cards" class="mesh-readings"></div></section>
<section class="mesh-section"><div class="mesh-heading"><div><p class="mesh-eyebrow">TIME SERIES</p><h2>Measurement history</h2></div><div class="mesh-ranges" role="group" aria-label="Time range"><button data-mesh-hours="24">24 hours</button><button data-mesh-hours="72">3 days</button><button data-mesh-hours="168" aria-pressed="true">7 days</button><button data-mesh-hours="720">30 days</button></div></div><div class="mesh-chart-panel"><div class="mesh-chart-head"><div class="mesh-ranges" role="group" aria-label="Measurement"><button data-mesh-metric="distance" aria-pressed="true">Distance</button><button data-mesh-metric="battery">Battery</button></div><span id="mesh-count">—</span></div><canvas id="mesh-plot" role="img" aria-label="Station measurement history"></canvas><p id="mesh-chart-note">Select a station to view its readings.</p></div><div class="mesh-bottom"><div class="mesh-table-panel"><div class="mesh-table-head"><h3>Recent measurements</h3><span id="mesh-table-count"></span></div><div class="mesh-table-scroll"><table><thead><tr><th>Received · MT</th><th>Distance · mm</th><th>Stage · ft</th><th>Battery · %</th><th>Battery · V</th></tr></thead><tbody id="mesh-rows"></tbody></table></div></div><aside class="mesh-note"><h3>Instrumentation</h3><dl><dt>Sensor</dt><dd id="mesh-model">—</dd><dt>Water distance</dt><dd>Measured from the sensor to the water surface. Smaller distance generally means rising water if the sensor is fixed.</dd><dt>Stage</dt><dd>Shown only after the station datum is calibrated.</dd><dt>Battery</dt><dd>Radio telemetry may arrive separately from water readings.</dd></dl><p id="mesh-gateway">Gateway status unavailable</p></aside></div></section><details class="mesh-section mesh-admin" id="mesh-remote-panel"><summary>Gateway &amp; node administration <span>Station registration, DM rules, firmware policy</span></summary>
<div class="mesh-heading"><div><p class="mesh-eyebrow">CLOUDFLARE CONTROL</p><h2>Remote management</h2><p>Station registration, existing V9 auto-DM overrides, and signed Heltec firmware policy.</p></div><button type="button" id="mesh-remote-refresh">Refresh controls</button></div>
<div class="mesh-remote-stats" id="mesh-remote-stats">Reading gateway configuration…</div>
<div class="mesh-remote-forms">
<form id="mesh-remote-node"><h3>Add mesh station</h3>
<label>Node ID<input required name="node_id" maxlength="9" placeholder="!1234abcd"></label>
<label>Station name<input required name="long_name" maxlength="80" placeholder="DYP station"></label>
<label>Reading interval (minutes)<select name="minutes"><option value="15">15</option><option value="30">30</option><option value="60">60</option><option value="180">180</option></select></label>
<button type="submit">Register node</button><small>Authorized DYP-A08 nodes synchronize without a firmware flash.</small></form>
<form id="mesh-remote-dm"><h3>Existing auto-DM setting</h3>
<label>V9 watched node number<input required type="number" name="node_num" min="1" max="4294967295"></label>
<label>Missing-data threshold<select name="minutes"><option value="15">15 minutes</option><option value="30">30 minutes</option><option value="45">45 minutes</option><option value="60" selected>60 minutes</option><option value="360">6 hours</option><option value="720">12 hours</option><option value="1440">24 hours</option></select></label>
<label class="mesh-check"><input name="enabled" type="checkbox" checked>Enabled</label>
<button type="submit">Save DM rule</button><small>Only affects nodes already watched by the V9 recovery scheduler.</small></form>
<form id="mesh-remote-ota"><h3>Heltec firmware policy</h3>
<label>Release name<input name="version" maxlength="63" placeholder="v10-2026-11-25"></label>
<label>Approved GitHub BIN URL<input name="url" placeholder="https://raw.githubusercontent.com/…"></label>
<label>SHA-256 checksum<input name="sha" maxlength="64"></label>
<label class="mesh-check"><input id="mesh-ota-enable" name="enabled" type="checkbox">Allow unattended install</label>
<button type="submit">Save firmware policy</button><small id="mesh-ota-note">Locked until matching loader reports ready.</small></form>
</div><p id="mesh-remote-message" role="status"></p>
</details></section><section id="lift-view" hidden>
<section class="hero"><div><div class="eyebrow">Lift station monitoring</div><h1>Station overview<span style="color:var(--aqua)">.</span></h1><p class="lift-intro">Configured stations appear below. Status becomes live after each gateway sends its first report.</p></div><button class="refresh" id="refresh">Refresh status</button></section>
<div id="banner"></div><section class="stats"><div class="stat"><span>Stations</span><strong id="total">—</strong></div><div class="stat"><span>Normal</span><strong id="normal">—</strong></div><div class="stat"><span>Active alarms</span><strong id="alarms" style="color:var(--red)">—</strong></div><div class="stat"><span>Offline</span><strong id="offline" style="color:var(--amber)">—</strong></div></section>
<section><div class="sectionhead"><div><h2>Sites & stations</h2><span id="updated"></span></div><div class="filters"><label><span class="sr-only">Search stations</span><input id="station-search" type="search" placeholder="Search stations" aria-label="Search stations"></label><select id="site-filter" aria-label="Filter by site"><option value="">All sites</option></select></div></div><div class="grid" id="stations"></div></section>
<section id="history" class="history" aria-live="polite"><div class="history-head"><div><div class="eyebrow">Station details</div><h2 id="history-title">Recent readings</h2></div><div class="history-actions"><label class="range">Period <select id="history-range"><option value="24">24 hours</option><option value="168">7 days</option><option value="720">30 days</option></select></label><button type="button" id="export-readings">Download CSV</button><button type="button" class="link" id="close-history">Close</button></div></div><div id="history-content"></div></section>
<section class="admin-panel" id="admin" hidden><div class="sectionhead"><h2>Station setup</h2><span>Administrator</span></div><p>When you are ready to connect a lift station, create its owner, facility and station in that order. You can give the client access later.</p><details><summary>Open setup forms</summary><div class="admin-grid">
<form class="formbox" data-route="clients"><h3>1 · New client</h3><div class="fields"><label>Client name<input name="name" required maxlength="120"></label><label>Alert recipient email<input name="alert_email" type="email" required></label></div><button type="submit">Create client</button></form>
<form class="formbox" data-route="sites"><h3>2 · New site</h3><div class="fields"><label>Client<select name="client_id" data-clients required></select></label><label>Site name<input name="name" required maxlength="120"></label><label>Details<input name="description" maxlength="240"></label></div><button type="submit">Create site</button></form>
<form class="formbox" data-route="stations"><h3>3 · New station</h3><div class="fields"><label>Site<select name="site_id" data-sites required></select></label><label>Station name<input name="name" required maxlength="120"></label><label>Report interval, minutes<input name="report_interval_minutes" type="number" min="1" max="60" value="5"></label><label>Offline after, minutes<input name="offline_after_minutes" type="number" min="10" max="240" value="15"></label></div><button type="submit">Create station</button></form>
<form class="formbox" data-route="users"><h3>4 · Client contact</h3><p>Save their name and email here. Add the same email to the Cloudflare Access policy separately before they can sign in.</p><div class="fields"><label>Client<select name="client_id" data-clients required></select></label><label>Person's name<input name="name" required maxlength="120"></label><label>Email<input name="email" type="email" required></label></div><button type="submit">Save client contact</button></form></div><div id="access-list"></div><div id="key"></div><div id="message" role="status"></div></details></section>
<div class="footer">Data reflects reports received by the gateway. Loss of radio, power or internet can delay alarms. Keep the facility's local alarm and operating procedures in place.</div></section>
<section id="pumps-view" hidden><button type="button" class="back" data-view="lift">← Lift stations</button><div class="eyebrow">Lift station diagnostics</div><h1>Which pump is failing<span style="color:var(--aqua)">?</span></h1><p class="lift-intro">Compare the two motors using run feedback, accumulated runtime, starts, faults and measured phase currents. The existing alarm contact does not identify which motor ran or why it stopped.</p><div class="diagnostic-toolbar"><label>Station <select id="pump-station" aria-label="Station for pump diagnostics"></select></label><label>Period <select id="pump-range"><option value="24">24 hours</option><option value="168" selected>7 days</option><option value="720">30 days</option></select></label><button type="button" id="pump-refresh">Refresh diagnostics</button></div><div id="pump-results" aria-live="polite"></div><section class="diagnostic-guide"><h2>Signals needed to solve this</h2><ol><li><b>Run feedback for each pump</b> from its contactor auxiliary or controller output: proves whether Pump 2 is commanded and actually energized.</li><li><b>Persistent runtime and start counters for each pump</b> from the controller or gateway: compares duty accurately even when a cycle is shorter than the reporting interval.</li><li><b>Individual fault or overload contacts</b>: ties a trip to the motor instead of the shared high-level alarm.</li><li><b>Phase A/B/C current under load</b> for each three-phase motor, if applicable: shows an imbalance or loss of current. Voltage, overload history and onsite electrical testing are still needed to determine a cause.</li></ol><p>Do not infer motor runtime from five-minute status snapshots. A pump can start and stop between reports.</p></section></section></main>
<script>
let state={clients:[],sites:[],stations:[],alerts:[]},me=null,assigned=[],selectedStation=null,readingCache=[];
const $=s=>document.querySelector(s), el=(tag,cls,text)=>{const n=document.createElement(tag);if(cls)n.className=cls;if(text!=null)n.textContent=text;return n};
const stamp=t=>t?new Date(t).toLocaleString(): 'Never';
const minutes=t=>t?Math.floor((Date.now()-Date.parse(t))/60000):Infinity;
async function request(path,options={}){const r=await fetch(path,{credentials:'same-origin',...options});const v=await r.json();if(!r.ok)throw Error(v.error||'Request failed');return v}
function selectOptions(selector,values,label){document.querySelectorAll(selector).forEach(s=>{const previous=s.value;s.replaceChildren(el('option','',label));for(const item of values){const o=el('option','',item.name);o.value=item.id;s.append(o)}if(values.some(x=>x.id===previous))s.value=previous})}
function draw(){let alarm=0,offline=0,normal=0,shown=0;const wrap=$('#stations'),siteFilter=$('#site-filter').value,q=$('#station-search').value.trim().toLowerCase();wrap.replaceChildren();for(const station of state.stations){const site=state.sites.find(x=>x.id===station.site_id),client=state.clients.find(x=>x.id===station.client_id);const age=minutes(station.last_seen_at);const down=!!station.commissioned&&age>=station.offline_after_minutes;const active=state.alerts.some(x=>x.station_id===station.id&&x.kind==='alarm');let status=!station.commissioned?'Awaiting first report':down?'Offline':active?'Alarm active':'Normal';if(down)offline++;else if(active)alarm++;else if(station.commissioned)normal++;if((siteFilter&&station.site_id!==siteFilter)||(q&&!(station.name+' '+(site?.name||'')+' '+(client?.name||'')).toLowerCase().includes(q)))continue;shown++;
const card=el('article','card'),top=el('div','cardtop');top.append(el('span','eyebrow',client?.name||'Client'),el('span','pill'+(down?' warn':active?' bad':''),status));card.append(top,el('h3','',station.name),el('small','',site?.name||'Site'));
const metrics=el('div','metrics three');for(const [title,value] of [['Last report',stamp(station.last_seen_at)],['Level',station.latest_level??'—'],['Battery',station.latest_battery??'—']]){const block=el('div');block.append(el('span','',title),el('b','',String(value)));metrics.append(block)}card.append(metrics);const tools=el('div','tools'),more=el('button','link','Explore data →');more.onclick=()=>showReadings(station);tools.append(more);if(me?.role==='admin'){const reveal=el('button','','Reveal token'),rotate=el('button','','Replace token');reveal.onclick=()=>stationToken(station,'reveal');rotate.onclick=()=>stationToken(station,'rotate');tools.append(reveal,rotate)}card.append(tools);wrap.append(card)}
if(!shown)wrap.append(el('div','empty',state.stations.length?'No stations match these filters.':'No stations yet. Add a client, site and station below.'));
$('#total').textContent=state.stations.length;$('#normal').textContent=normal;$('#alarms').textContent=alarm;$('#offline').textContent=offline;$('#updated').textContent='Updated '+new Date().toLocaleTimeString();
const banner=$('#banner');banner.replaceChildren();if(alarm||offline)banner.append(el('div','notice',alarm+' active alarm(s), '+offline+' station(s) offline. Confirm conditions onsite.'));else if(state.stations.length&&!state.stations.some(s=>s.commissioned))banner.append(el('div','notice',state.stations.length+' lift stations configured · awaiting first gateway reports. No live lift telemetry yet.'));
selectOptions('[data-clients]',state.clients,'Select a client');selectOptions('[data-sites]',state.sites.map(x=>({...x,name:(state.clients.find(c=>c.id===x.client_id)?.name||'Client')+' · '+x.name})),'Select a site');selectOptions('#site-filter',state.sites,'All sites')}


function meshTime(v){if(!v)return NaN;const s=String(v).replace(' ','T');return Date.parse(s.endsWith('Z')?s:s+'Z')}
function meshFormat(v){const t=meshTime(v);return Number.isFinite(t)?new Intl.DateTimeFormat('en-US',{timeZone:'America/Denver',month:'short',day:'numeric',hour:'numeric',minute:'2-digit',timeZoneName:'short'}).format(t):'No report'}
let meshData=null,meshSelection=null,meshMetric='distance',meshHours=168;
const meshNode=()=>meshData?.stations.find(s=>String(s.node_num)===String(meshSelection));
function meshCard(label,value,unit,time,foot){const card=el('article','mesh-reading');card.append(el('div','label',label));const val=el('div','value',value);if(unit)val.append(el('span','',unit));card.append(val,el('p','time',time),el('div','foot',foot));return card}
function renderMesh(){const data=meshData,node=meshNode();if(!data||!node)return;const obs=data.observations.filter(r=>r.node_num===node.node_num).sort((a,b)=>meshTime(a.received_at)-meshTime(b.received_at)),bat=data.batteries.filter(r=>r.node_num===node.node_num).sort((a,b)=>meshTime(a.received_at)-meshTime(b.received_at)),last=obs.at(-1),battery=bat.at(-1),waterAge=meshTime(last?.received_at),age=Number.isFinite(waterAge)?Math.round((Date.now()-waterAge)/60000):Infinity;
const status=age===Infinity?(node.last_received_at?'No water reports in selected period':'Awaiting first water report'):age>Math.max(60,node.report_interval_minutes*3)?'Water reporting stale':'Receiving water reports';$('#mesh-title').textContent=node.long_name;$('#mesh-subtitle').textContent=node.node_id+' · '+node.sensor_model+' · expected interval '+node.report_interval_minutes+' minutes';$('#mesh-status').textContent=status+' · '+(last?'last water reading '+meshFormat(last.received_at):(node.last_received_at?'last received '+meshFormat(node.last_received_at):'no water observations received'));$('#mesh-updated').textContent='Checked '+meshFormat(new Date().toISOString());
$('#mesh-cards').replaceChildren(meshCard('Water distance',last?.distance_mm!=null?Number(last.distance_mm).toLocaleString():'—','mm',last?meshFormat(last.received_at):'Awaiting water reading','Sensor-to-water distance · smaller means water is closer to the sensor'),meshCard('Stage',last?.stage_calibrated===1&&last.stage_mm!=null?(last.stage_mm/304.8).toFixed(2):'—',last?.stage_calibrated===1&&last.stage_mm!=null?'ft':'',last?.stage_calibrated===1?meshFormat(last.received_at):'Datum not calibrated','Stage appears after a station-specific reference is set'),meshCard('Radio battery',battery?.battery_percent!=null?Number(battery.battery_percent).toFixed(0):battery?.battery_voltage!=null?Number(battery.battery_voltage).toFixed(2):'—',battery?.battery_percent!=null?'%':battery?.battery_voltage!=null?'V':'',battery?meshFormat(battery.received_at):'Awaiting device telemetry',battery?.battery_voltage!=null?Number(battery.battery_voltage).toFixed(2)+' V reported':'Battery voltage not reported'));
$('#mesh-model').textContent=node.sensor_model;$('#mesh-gateway').textContent=data.gateway?'Gateway last checked in '+meshFormat(data.gateway.received_at)+(data.gateway.firmware?' · '+data.gateway.firmware:''):'No gateway check-in recorded';const hasPercent=bat.some(r=>r.battery_percent!=null),samples=meshMetric==='distance'?obs.filter(r=>r.distance_mm!=null).map(r=>({time:meshTime(r.received_at),value:Number(r.distance_mm)})):bat.filter(r=>hasPercent?r.battery_percent!=null:r.battery_voltage!=null).map(r=>({time:meshTime(r.received_at),value:Number(hasPercent?r.battery_percent:r.battery_voltage)})),units=meshMetric==='distance'?'mm':hasPercent?'%':'V';$('#mesh-count').textContent=samples.length+' '+(meshMetric==='distance'?'water readings':'battery packets')+' in selected range';drawMeshPlot(samples,units,node.report_interval_minutes,meshHours);$('#mesh-chart-note').textContent=meshMetric==='distance'?'Distance from sensor to water in millimeters. Lines stop across reporting gaps; stage requires calibration.':'Radio battery telemetry in '+(units==='%'?'percent':'volts')+'. It may arrive separately from water readings.';
const rows=$('#mesh-rows');rows.replaceChildren();const recent=obs.slice(-40).reverse();$('#mesh-table-count').textContent=obs.length+' water reports · '+bat.length+' device packets';if(!recent.length){const tr=el('tr'),td=el('td','','No water observations in this range');td.colSpan=5;tr.append(td);rows.append(tr)}for(const r of recent){const tr=el('tr'),prior=bat.filter(b=>meshTime(b.received_at)<=meshTime(r.received_at)).at(-1);for(const value of [meshFormat(r.received_at),r.distance_mm??'—',r.stage_calibrated===1&&r.stage_mm!=null?(r.stage_mm/304.8).toFixed(2):'—',prior?.battery_percent!=null?Number(prior.battery_percent).toFixed(0):'—',prior?.battery_voltage!=null?Number(prior.battery_voltage).toFixed(2):'—'])tr.append(el('td','',String(value)));rows.append(tr)}}
function drawMeshPlot(points,units,interval,hours){const canvas=$('#mesh-plot'),rect=canvas.getBoundingClientRect(),dpr=Math.min(devicePixelRatio||1,2),width=Math.max(rect.width,320),height=Math.max(rect.height,240);canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);const c=canvas.getContext('2d');c.scale(dpr,dpr);c.fillStyle='#fff';c.fillRect(0,0,width,height);const left=58,right=18,top=18,bottom=36,w=width-left-right,h=height-top-bottom;c.font='11px Arial';c.textAlign='right';c.fillStyle='#62717c';c.strokeStyle='#e3e8ec';c.lineWidth=1;const valid=points.filter(p=>Number.isFinite(p.time)&&Number.isFinite(p.value)).sort((a,b)=>a.time-b.time),now=Date.now(),minTime=now-hours*3600000,values=valid.map(p=>p.value),vmin=values.length?Math.min(...values):0,vmax=values.length?Math.max(...values):1,pad=Math.max((vmax-vmin)*.15,units==='mm'?5:units==='%'?2:.05),lo=vmin-pad,hi=vmax+pad;for(let i=0;i<4;i++){const yy=top+i*h/3;c.beginPath();c.moveTo(left,yy);c.lineTo(width-right,yy);c.stroke();c.fillText((hi-(hi-lo)*i/3).toFixed(units==='V'?2:0),left-9,yy+4)}const fx=t=>left+(t-minTime)/(hours*3600000)*w,fy=v=>top+(hi-v)/(hi-lo)*h;c.textAlign='left';c.fillText(new Intl.DateTimeFormat('en-US',{timeZone:'America/Denver',month:'short',day:'numeric'}).format(minTime),left,height-10);c.textAlign='right';c.fillText(new Intl.DateTimeFormat('en-US',{timeZone:'America/Denver',month:'short',day:'numeric'}).format(now),width-right,height-10);if(!valid.length){c.textAlign='center';c.font='13px Arial';c.fillText('No measurements in this range',width/2,height/2);return}c.strokeStyle='#285e78';c.lineWidth=2.5;c.beginPath();let prior=null;for(const p of valid){const px=fx(p.time),py=fy(p.value);if(prior&&p.time-prior.time<=Math.max(20,interval*3)*60000)c.lineTo(px,py);else c.moveTo(px,py);prior=p}c.stroke();c.fillStyle='#285e78';for(const p of valid.slice(-200)){c.beginPath();c.arc(fx(p.time),fy(p.value),3,0,Math.PI*2);c.fill()}}
async function loadMesh(){const selector=$('#mesh-station');$('#mesh-status').textContent='Loading water telemetry…';try{const data=await request('/api/mesh?hours='+meshHours);meshData=data;const previous=meshSelection;selector.replaceChildren();for(const n of data.stations){const option=document.createElement('option');option.value=n.node_num;option.textContent=n.long_name;selector.append(option)}meshSelection=data.stations.some(n=>String(n.node_num)===String(previous))?previous:data.stations[0]?.node_num;if(meshSelection==null){$('#mesh-status').textContent='No mesh stations registered';$('#mesh-cards').replaceChildren();return}selector.value=meshSelection;renderMesh()}catch(error){$('#mesh-status').textContent='Could not load mesh data: '+error.message;$('#mesh-cards').replaceChildren()}}

let meshRemoteSnapshot=null;
async function loadMeshRemote(){
 const panel=$('#mesh-remote-panel');if(!panel)return;
 if(me?.role!=='admin')return;
 try{
  const r=await request('/api/mesh/remote');meshRemoteSnapshot=r;
  const stats=$('#mesh-remote-stats');stats.replaceChildren();
  const stamp=r.health?.received_at?Date.parse(r.health.received_at.replace(' ','T')+'Z'):0;
  const ready=!!r.health?.note?.includes('loader_ready=1')&&(Date.now()-stamp)<1800000;
  for(const [label,value] of [['Cloudflare version',r.gateway?.version??'—'],['Registered nodes',r.stations.length],['Gateway heartbeat',stamp?meshFormat(r.health.received_at):'Never'],['Recovery loader',ready?'READY':'NOT VERIFIED'],['Remote OTA',r.gateway?.ota_enabled?'ENABLED':'OFF']]){
   const chip=el('div','mesh-remote-chip');chip.append(el('small','',label),el('strong','',String(value)));stats.append(chip)
  }
  const f=$('#mesh-remote-ota');
  if(!f.contains(document.activeElement)){
   f.elements.version.value=r.gateway?.ota_version||'';
   f.elements.url.value=r.gateway?.ota_url||'';
   f.elements.sha.value=r.gateway?.ota_sha256||'';
   f.elements.enabled.checked=!!r.gateway?.ota_enabled;
  }
  $('#mesh-ota-enable').disabled=!ready&&!r.gateway?.ota_enabled;
  $('#mesh-ota-note').textContent=ready?'Loader authenticated by gateway heartbeat. Only enable after hardware recovery test.':'OTA locked: matching CCA loader has not checked in during the past 30 minutes.';
 }catch(e){$('#mesh-remote-stats').textContent='Cannot load remote controls: '+e.message}
}
async function saveMeshRemote(payload){
 const status=$('#mesh-remote-message');status.textContent='Saving…';
 try{
  const r=await request('/api/mesh/remote',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(payload)});
  status.textContent='Saved as Cloudflare configuration '+r.version+'. Heltec polls within five minutes.';
  await loadMeshRemote();await loadMesh();
 }catch(e){status.textContent='Not saved: '+e.message}
}
$('#mesh-remote-panel').addEventListener('toggle',e=>{if(e.target.open)loadMeshRemote()});
$('#mesh-remote-refresh').onclick=loadMeshRemote;
$('#mesh-remote-node').onsubmit=e=>{e.preventDefault();const f=e.currentTarget;saveMeshRemote({action:'add_node',node_id:f.elements.node_id.value,long_name:f.elements.long_name.value,report_interval_minutes:Number(f.elements.minutes.value)})};
$('#mesh-remote-dm').onsubmit=e=>{e.preventDefault();const f=e.currentTarget;saveMeshRemote({action:'set_dm',node_num:Number(f.elements.node_num.value),interval_minutes:Number(f.elements.minutes.value),enabled:f.elements.enabled.checked})};
$('#mesh-remote-ota').onsubmit=e=>{e.preventDefault();const f=e.currentTarget;const enabled=f.elements.enabled.checked;if(enabled&&!confirm('Authorize unattended installation only after the CCA loader and recovery procedure have been tested?'))return;saveMeshRemote({action:'set_firmware',ota_enabled:enabled,ota_version:f.elements.version.value,ota_url:f.elements.url.value,ota_sha256:f.elements.sha.value})};
function showView(view){if(!['mesh','lift','pumps'].includes(view))view=me?.role==='admin'?'mesh':'lift';if(view==='mesh'&&me?.role!=='admin')view='lift';document.body.classList.toggle('mesh-active',view==='mesh');$('#lift-view').hidden=view!=='lift';$('#pumps-view').hidden=view!=='pumps';$('#mesh-view').hidden=view!=='mesh';if(view==='mesh')loadMesh();document.querySelectorAll('.nav [data-view]').forEach(b=>{if(b.dataset.view===view)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current')});if(location.hash!=='#'+view)history.replaceState(null,'','#'+view);if(view==='pumps'&&me&&state.stations.length)loadPumpDiagnostics()}
document.querySelector('#mesh-station').addEventListener('change',e=>{meshSelection=e.target.value;renderMesh()});document.querySelector('#mesh-refresh').addEventListener('click',loadMesh);document.querySelectorAll('[data-mesh-hours]').forEach(b=>b.addEventListener('click',()=>{meshHours=Number(b.dataset.meshHours);document.querySelectorAll('[data-mesh-hours]').forEach(x=>x.setAttribute('aria-pressed',String(x===b)));loadMesh()}));document.querySelectorAll('[data-mesh-metric]').forEach(b=>b.addEventListener('click',()=>{meshMetric=b.dataset.meshMetric;document.querySelectorAll('[data-mesh-metric]').forEach(x=>x.setAttribute('aria-pressed',String(x===b)));renderMesh()}));addEventListener('resize',()=>{if(!document.querySelector('#mesh-view').hidden&&meshData)renderMesh()});
document.querySelectorAll('[data-view]').forEach(b=>b.addEventListener('click',()=>showView(b.dataset.view)));
addEventListener('hashchange',()=>showView(location.hash.slice(1)));
function showToken(label,token){const box=$('#key');box.closest('details').open=true;box.replaceChildren(el('strong','',label),el('div','key',token),el('p','','Keep this token in your gateway configuration. Anyone with it can submit readings.'));box.scrollIntoView({behavior:'smooth'})}
async function stationToken(station,action){if(action==='rotate'&&!confirm('Replace the token for '+station.name+'? The old gateway token will stop working immediately.'))return;try{const result=await request('/api/stations/'+station.id+'/'+action,{method:'POST',headers:{'content-type':'application/json'},body:'{}'});showToken(station.name+' · Station '+station.id+' · gateway token',result.token)}catch(e){$('#admin details').open=true;$('#message').textContent=e.message;$('#admin').scrollIntoView({behavior:'smooth'})}}
function drawAccess(){const box=$('#access-list');box.replaceChildren(el('h3','','Client contacts'),el('p','','Cloudflare sign-in permission is managed separately.'));if(!assigned.length){box.append(el('p','','No client contacts saved.'));return}for(const person of assigned){const row=el('div','access-row'),name=el('div');name.append(el('strong','',person.display_name||person.email),el('small','',person.email+' · '+(state.clients.find(c=>c.id===person.client_id)?.name||'Client')));const remove=el('button','','Remove contact');remove.onclick=async()=>{if(!confirm('Remove '+person.email+' from this client? Also remove their email from Cloudflare Access manually if they should no longer reach the login page.'))return;try{await request('/api/users',{method:'DELETE',headers:{'content-type':'application/json'},body:JSON.stringify({email:person.email,client_id:person.client_id})});$('#message').textContent='Contact removed from the monitor. Check Cloudflare Access separately.';await load()}catch(e){$('#message').textContent=e.message}};row.append(name,remove);box.append(row)}}
async function load(){try{if(!me){me=await request('/api/me');$('#signedin').textContent=me.email+' · '+(me.role==='admin'?'Administrator':'Client');$('#admin').hidden=me.role!=='admin';document.querySelectorAll('[data-view="mesh"]').forEach(b=>b.hidden=me.role!=='admin');showView(location.hash.slice(1)||(me.role==='admin'?'mesh':'lift'))}state=await request('/api/overview');if(me.role==='admin'){assigned=(await request('/api/users')).users;drawAccess()}draw();selectOptions('#pump-station',state.stations.map(s=>({...s,name:(state.sites.find(site=>site.id===s.site_id)?.name||'Site')+' · '+s.name})),'Select station');if(!$('#pump-station').value&&state.stations.length)$('#pump-station').value=state.stations[0].id;if(!$('#pumps-view').hidden)loadPumpDiagnostics()}catch(e){showView(me?.role==='admin'?'mesh':'lift');$('#banner').replaceChildren(el('div','notice',e.message))}}
const svgEl=(tag,attrs={})=>{const n=document.createElementNS('http://www.w3.org/2000/svg',tag);for(const [key,value] of Object.entries(attrs))n.setAttribute(key,String(value));return n};
function sampledPoints(points){if(points.length<=450)return points;const size=Math.ceil(points.length/180),selected=[];for(let i=0;i<points.length;i+=size){const group=points.slice(i,i+size),low=group.reduce((a,b)=>a.value<b.value?a:b),high=group.reduce((a,b)=>a.value>b.value?a:b);selected.push(...[group[0],low,high,group[group.length-1]].filter((v,j,a)=>a.indexOf(v)===j).sort((a,b)=>a.time-b.time))}return selected}
function trendChart(readings,field,title,color,station,hours){const panel=el('div','trend'),head=el('h3','',title);panel.append(head,el('small','','Reported values · units not specified'));const valid=readings.map(r=>({time:Date.parse(r.received_at),value:Number(r[field])})).filter((p,i)=>readings[i][field]!=null&&Number.isFinite(p.time)&&Number.isFinite(p.value));if(!valid.length){panel.append(el('div','trend-empty','No '+title.toLowerCase()+' readings in this period'));return panel}const min=Math.min(...valid.map(p=>p.value)),max=Math.max(...valid.map(p=>p.value)),spread=max-min||1,now=Date.now(),start=now-hours*3600000,svg=svgEl('svg',{viewBox:'0 0 600 165',role:'img','aria-label':title+' history from '+stamp(readings[0]?.received_at)+' to '+stamp(readings.at(-1)?.received_at)});for(const y of [16,82,148])svg.append(svgEl('line',{x1:0,y1:y,x2:600,y2:y,stroke:'#28444a','stroke-dasharray':'3 5'}));const points=sampledPoints(valid),gapMs=Math.max(station.report_interval_minutes*3,20)*60000;let part=[],last=null;function flush(){if(part.length){svg.append(svgEl('polyline',{points:part.join(' '),fill:'none',stroke:color,'stroke-width':2.5,'stroke-linecap':'round','stroke-linejoin':'round'}));part=[]}}for(const p of points){if(last&&p.time-last.time>gapMs)flush();const x=Math.max(0,Math.min(600,(p.time-start)/(hours*3600000)*600)),y=148-(p.value-min)/spread*132;part.push(x.toFixed(1)+','+y.toFixed(1));last=p}flush();panel.append(svg);const foot=el('div','chart-foot');foot.append(el('span','','Min '+min.toFixed(2)),el('span','','Max '+max.toFixed(2)));panel.append(foot);const times=el('div','chart-foot');times.append(el('span','',stamp(new Date(start).toISOString())),el('span','',stamp(new Date(now).toISOString())));panel.append(times);return panel}
function renderReadings(station,readings,hours,truncated){const content=$('#history-content');content.replaceChildren();const summary=el('div','reading-summary'),alarmCount=readings.filter(r=>r.alarm).length,gaps=[];for(let i=1;i<readings.length;i++){const interval=(Date.parse(readings[i].received_at)-Date.parse(readings[i-1].received_at))/60000;if(interval>Math.max(3*station.report_interval_minutes,20))gaps.push({at:readings[i].received_at,minutes:Math.round(interval)})}for(const [label,value] of [['Reports received',readings.length.toLocaleString()],['Alarm reports',alarmCount.toLocaleString()],['Reporting gaps',gaps.length.toLocaleString()]]){const card=el('div');card.append(el('small','',label),el('strong','',value));summary.append(card)}content.append(summary);if(truncated)content.append(el('div','notice','This period has more than 12,000 reports. Showing the latest 12,000; CSV has the same limit.'));if(!readings.length){content.append(el('div','empty','No reports received during this period. Check the station status and try a longer range.'));return}const charts=el('div','trend-grid');charts.append(trendChart(readings,'level','Level','#6ee1c7',station,hours),trendChart(readings,'battery','Battery','#f3c57c',station,hours));content.append(charts);const events=el('div');events.append(el('h3','','Notable reports'));const list=el('ul','event-list'),items=[],gapByTime=new Map(gaps.map(g=>[g.at,g]));for(let i=0;i<readings.length;i++){const r=readings[i];if(r.alarm&&(!i||!readings[i-1].alarm))items.push({at:r.received_at,label:'Alarm reported'});const gap=gapByTime.get(r.received_at);if(gap)items.push({at:r.received_at,label:'Report gap of '+gap.minutes+' minutes'})}items.sort((a,b)=>Date.parse(b.at)-Date.parse(a.at));for(const item of items.slice(0,8)){const li=el('li');li.append(el('b','',item.label+' · '),document.createTextNode(stamp(item.at)));list.append(li)}if(!items.length)list.append(el('li','','No alarm transitions or large report gaps in these received readings.'));events.append(list);content.append(events);const details=document.createElement('details');details.append(el('summary','','Recent raw reports (latest 250)'));details.append(el('p','','Download CSV for all '+readings.length.toLocaleString()+' reports in this selection.'));const wrap=el('div','readings-table'),table=el('table'),head=el('tr');for(const title of ['Received','Observed','Alarm','Level','Battery','Notes'])head.append(el('th','',title));table.append(head);for(const r of readings.slice(-250).reverse()){const row=el('tr');for(const value of [stamp(r.received_at),stamp(r.observed_at),r.alarm?'Active':'Clear',r.level??'—',r.battery??'—',r.note||'—'])row.append(el('td','',String(value)));table.append(row)}wrap.append(table);details.append(wrap);content.append(details)}
async function showReadings(station,scroll=true){selectedStation=station;readingCache=[];$('#export-readings').disabled=true;const box=$('#history'),content=$('#history-content'),hours=Number($('#history-range').value);$('#history-title').textContent=station.name+' · '+(state.sites.find(s=>s.id===station.site_id)?.name||'Site');box.classList.add('show');content.replaceChildren(el('p','','Loading reports…'));try{const data=await request('/api/readings?station='+encodeURIComponent(station.id)+'&hours='+hours);if(selectedStation?.id!==station.id||Number($('#history-range').value)!==hours)return;readingCache=data.readings;$('#export-readings').disabled=false;renderReadings(station,data.readings,hours,data.truncated)}catch(e){content.textContent=e.message}if(scroll)box.scrollIntoView({behavior:'smooth',block:'start'})}
function downloadReadings(){if(!selectedStation)return;const lines=[['received_at','observed_at','alarm','level','battery','note'],...readingCache.map(r=>[r.received_at,r.observed_at||'',r.alarm?'1':'0',r.level??'',r.battery??'',['=','+','@'].includes((r.note||'')[0])||[9,13].includes((r.note||'').charCodeAt(0))?"'"+r.note:r.note||''])],csv=lines.map(row=>row.map(value=>'"'+String(value).replaceAll('"','""')+'"').join(',')).join(String.fromCharCode(13,10)),blob=new Blob([csv],{type:'text/csv;charset=utf-8'}),url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download='cces-'+selectedStation.name.toLowerCase().replace(/[^a-z0-9]+/g,'-')+'-'+$('#history-range').value+'h.csv';document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000)}
function answer(box,question,value){const row=el('div','answer-row');row.append(el('b','',question),el('span','',value));box.append(row)}
function phaseText(pump){const values=pump.latest?.phase_amps;if(!values)return 'Not measured';const text=values.map((n,i)=>'ABC'[i]+': '+n.toFixed(1)+' A').join(' · '),mean=values.reduce((a,b)=>a+b,0)/3;return text+(mean>0&&pump.latest.running===true?' · spread '+((Math.max(...values)-Math.min(...values))/mean*100).toFixed(0)+'% of mean':' · not confirmed under load')}
function renderPumpDiagnostics(data,station){
  const box=$('#pump-results');box.replaceChildren();
  if(!data.sample_count){box.append(el('div','notice','Pump run, fault and phase signals are not connected yet. The shared alarm cannot reveal which motor ran or failed.'));return}
  const p1=data.pumps['1'],p2=data.pumps['2'];
  box.append(el('p','diagnostic-caveat',data.sample_count.toLocaleString()+' pump telemetry reports · '+stamp(data.first_at)+' to '+stamp(data.last_at)+(data.truncated?' · latest 12,000 reports only':'')));
  const grid=el('div','diagnostic-grid');
  for(const [name,pump] of [['Pump 1',p1],['Pump 2',p2]]){
    const card=el('article','diagnostic-card');card.append(el('div','eyebrow','Motor evidence'),el('h3','',name));
    const dl=el('dl'),fresh=pump.latest_at&&minutes(pump.latest_at)<station.offline_after_minutes;
    for(const [label,value] of [
      ['Run feedback',pump.latest?.running==null?'Not measured':!fresh?'Stale · last '+stamp(pump.latest_at):pump.latest.running?'Running':'Stopped'],
      ['Runtime counter increase',pump.runtime_intervals?(pump.runtime_seconds/3600).toFixed(2)+' hours':'Not measured'],
      ['Start counter increase',pump.starts_intervals?String(pump.starts_count):'Not measured'],
      ['Observed running reports',String(pump.running_samples)],
      ['Fault reports',pump.latest?.fault==null&&pump.fault_samples===0?'Not measured':String(pump.fault_samples)],
      ['Phase currents',phaseText(pump)]
    ])dl.append(el('dt','',label),el('dd','',value));
    card.append(dl);grid.append(card)
  }
  box.append(grid);
  const answers=el('section','answer-card');answers.append(el('h2','','What the data says'));
  answer(answers,'Does Pump 2 come on?',p2.running_samples>0||p2.starts_count>0?'Yes. Run feedback or the start counter recorded Pump 2 activity in this period.':p2.starts_intervals?'No Pump 2 starts were counted in the available counter interval. This does not prove it never runs.':'Unknown. Connect Pump 2 run feedback and a start counter.');
  answer(answers,'Is one pump running longer?',p1.runtime_intervals&&p2.runtime_intervals?'Recorded runtime increases: Pump 1 '+(p1.runtime_seconds/3600).toFixed(2)+' h; Pump 2 '+(p2.runtime_seconds/3600).toFixed(2)+' h. Verify that both counters cover the same interval before treating this as a duty comparison.':'Unknown. Five-minute run snapshots miss short cycles; persistent runtime counters are needed for both pumps.');
  answer(answers,'Are both pumps running together?',data.both_running_samples?'Yes, in '+data.both_running_samples+' report(s).':'No simultaneous running was observed. Short overlaps between reports can be missed.');
  answer(answers,'Which pump reports a fault?',p1.fault_samples||p2.fault_samples?'Pump 1: '+p1.fault_samples+' fault reports; Pump 2: '+p2.fault_samples+' fault reports. Check the panel fault history for the actual trip cause.':'No fault reported by the supplied contacts. If fault inputs are unwired, this result is inconclusive.');
  answer(answers,'Are the electrical phases balanced?',p1.latest?.phase_amps||p2.latest?.phase_amps?'Latest phase currents appear on the pump cards. Compare under-load readings and investigate any recurring spread with onsite voltage and overload measurements.':'Unknown. Phase A/B/C current has not been supplied.');
  box.append(answers);
  const timeline=el('section','answer-card');timeline.append(el('h2','','Observed motor transitions'));
  if(!data.events.length)timeline.append(el('p','','No run or fault transitions were captured. A motor can cycle between reports.'));
  else{const list=el('ul','event-list');for(const event of data.events.slice(0,20)){const item=el('li');item.append(el('b','','Pump '+event.pump+' · '+event.kind+' · '),document.createTextNode(stamp(event.at)+(event.phase_amps?' · A/B/C '+event.phase_amps.map(n=>n.toFixed(1)).join('/')+' A':'')));list.append(item)}timeline.append(list)}
  box.append(timeline);
  if(p1.counter_resets||p2.counter_resets)box.append(el('div','notice','One or more counters decreased. A controller reset, rollover or counter replacement may make runtime/start totals incomplete.'));
  box.append(el('p','diagnostic-caveat','A running feedback signal shows the contactor or controller state, not proof of flow or motor health. Fault diagnosis still needs panel logs and onsite electrical checks.'))
}
async function loadPumpDiagnostics(){
  const station=state.stations.find(s=>s.id===$('#pump-station').value),box=$('#pump-results');
  if(!station){box.replaceChildren(el('div','empty','No station is available for pump diagnostics.'));return}
  const hours=Number($('#pump-range').value);box.replaceChildren(el('p','','Loading motor evidence…'));
  try{const data=await request('/api/pump-diagnostics?station='+encodeURIComponent(station.id)+'&hours='+hours);if($('#pump-station').value===station.id&&Number($('#pump-range').value)===hours)renderPumpDiagnostics(data,station)}catch(e){box.replaceChildren(el('div','notice',e.message))}
}
document.querySelectorAll('form[data-route]').forEach(form=>form.addEventListener('submit',async event=>{event.preventDefault();const route=form.dataset.route,payload=Object.fromEntries(new FormData(form));$('#message').textContent='Saving…';try{const result=await request('/api/'+route,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(payload)});$('#message').textContent=route==='users'?'Client contact saved. Add this email to Cloudflare Access manually before they can sign in.':'Saved.';if(result.token)showToken('Station '+result.id+' · gateway token',result.token);form.reset();await load()}catch(e){$('#message').textContent=e.message}}));
$('#pump-station').onchange=loadPumpDiagnostics;$('#pump-range').onchange=loadPumpDiagnostics;$('#pump-refresh').onclick=loadPumpDiagnostics;$('#refresh').onclick=()=>{load();if(selectedStation&&$('#history').classList.contains('show'))showReadings(selectedStation,false)};$('#close-history').onclick=()=>$('#history').classList.remove('show');$('#history-range').onchange=()=>{if(selectedStation)showReadings(selectedStation,false)};$('#export-readings').onclick=downloadReadings;$('#mesh-refresh').onclick=loadMesh;$('#mesh-hours').onchange=loadMesh;setInterval(()=>{if(!$('#mesh-view').hidden)loadMesh()},60000);$('#station-search').oninput=draw;$('#site-filter').onchange=draw;load();setInterval(load,60_000);
</script></body></html>`;

function analyzePumps(rows) {
  const samples = rows.map(row => {
    let pumps = null;
    try { pumps = row.pump_telemetry ? JSON.parse(row.pump_telemetry) : null; } catch { /* Ignore malformed historical values. */ }
    return { at: row.observed_at || row.received_at, pumps };
  }).filter(row => row.pumps && Number.isFinite(Date.parse(row.at))).sort((a, b) => Date.parse(a.at) - Date.parse(b.at));
  const result = { sample_count: samples.length, first_at: samples[0]?.at || null, last_at: samples.at(-1)?.at || null, both_running_samples: 0, pumps: {}, events: [] };
  const previous = { '1': null, '2': null };
  for (const key of ['1', '2']) result.pumps[key] = {
    running_samples: 0, stopped_samples: 0, fault_samples: 0, observed_starts: 0,
    runtime_seconds: 0, runtime_intervals: 0, starts_count: 0, starts_intervals: 0,
    counter_resets: 0, latest: null, latest_at: null
  };
  for (const sample of samples) {
    if (sample.pumps['1']?.running === true && sample.pumps['2']?.running === true) result.both_running_samples++;
    for (const key of ['1', '2']) {
      const current = sample.pumps[key];
      if (!current) continue;
      const out = result.pumps[key], prior = previous[key];
      if (current.running === true) out.running_samples++;
      if (current.running === false) out.stopped_samples++;
      if (current.fault === true) out.fault_samples++;
      if (current.running === true && prior?.running === false) out.observed_starts++;
      if (prior && current.running != null && prior.running != null && current.running !== prior.running)
        result.events.push({ at: sample.at, pump: key, kind: current.running ? 'Run feedback on' : 'Run feedback off', phase_amps: current.phase_amps || null });
      if (current.fault === true && prior?.fault === false)
        result.events.push({ at: sample.at, pump: key, kind: 'Fault contact on', phase_amps: current.phase_amps || null });
      for (const [field, sum, intervals] of [
        ['runtime_seconds_total', 'runtime_seconds', 'runtime_intervals'],
        ['starts_total', 'starts_count', 'starts_intervals']
      ]) {
        if (current[field] != null && prior?.[field] != null) {
          const delta = current[field] - prior[field];
          if (delta >= 0) { out[sum] += delta; out[intervals]++; }
          else out.counter_resets++;
        }
      }
      out.latest = current;
      out.latest_at = sample.at;
      previous[key] = current;
    }
  }
  result.events = result.events.slice(-100).reverse();
  return result;
}


const json = (data, status = 200) => new Response(JSON.stringify(data), { status, headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', 'x-content-type-options': 'nosniff' } });
const fail = (message, status = 400) => json({ error: message }, status);
const clean = (value, max = 120) => typeof value === 'string' ? value.trim().slice(0, max) : '';
const email = value => /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value) && value.length <= 254;
const id = () => crypto.randomUUID();
const hash = async value => Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(value)))).map(b => b.toString(16).padStart(2, '0')).join('');
const base64url = str => Uint8Array.from(atob(str.replace(/-/g, '+').replace(/_/g, '/') + '='.repeat((4 - str.length % 4) % 4)), c => c.charCodeAt(0));
const b64 = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/g, '');
async function vaultKey(env) {
  if (!env.TOKEN_VAULT_KEY) throw Error('Token vault is not configured');
  const raw = Uint8Array.from(atob(env.TOKEN_VAULT_KEY), c => c.charCodeAt(0));
  if (raw.length !== 32) throw Error('Invalid token vault key');
  return crypto.subtle.importKey('raw', raw, 'AES-GCM', false, ['encrypt', 'decrypt']);
}
export async function sealToken(token, stationId, env) {
  const iv = crypto.getRandomValues(new Uint8Array(12));
  const ciphertext = await crypto.subtle.encrypt({ name: 'AES-GCM', iv, additionalData: new TextEncoder().encode(stationId) }, await vaultKey(env), new TextEncoder().encode(token));
  return `${b64(iv)}.${b64(new Uint8Array(ciphertext))}`;
}
export async function openToken(sealed, stationId, env) {
  const [iv, ciphertext] = sealed.split('.');
  if (!iv || !ciphertext) throw Error('Invalid stored token');
  const raw = await crypto.subtle.decrypt({ name: 'AES-GCM', iv: base64url(iv), additionalData: new TextEncoder().encode(stationId) }, await vaultKey(env), base64url(ciphertext));
  return new TextDecoder().decode(raw);
}
const newToken = () => Array.from(crypto.getRandomValues(new Uint8Array(32))).map(b => b.toString(16).padStart(2, '0')).join('');
function pumpTelemetry(value) {
  if (value == null) return null;
  if (!value || typeof value !== 'object' || Array.isArray(value) || Object.keys(value).some(key => !['1','2'].includes(key))) throw Error('Invalid pump telemetry');
  const result = {};
  for (const [key, pump] of Object.entries(value)) {
    if (!pump || typeof pump !== 'object' || Array.isArray(pump) || Object.keys(pump).some(field => !['running','fault','runtime_seconds_total','starts_total','current_a','phase_amps'].includes(field))) throw Error('Invalid pump telemetry');
    const out = {};
    for (const field of ['running','fault']) if (pump[field] != null) {
      if (typeof pump[field] !== 'boolean') throw Error('Invalid pump '+field);
      out[field] = pump[field];
    }
    for (const field of ['runtime_seconds_total','starts_total','current_a']) if (pump[field] != null) {
      const n = pump[field];
      if (typeof n !== 'number' || !Number.isFinite(n) || n < 0 || (field === 'starts_total' && !Number.isSafeInteger(n)) || (field === 'current_a' && n > 1000)) throw Error('Invalid pump '+field);
      out[field] = n;
    }
    if (pump.phase_amps != null) {
      if (!Array.isArray(pump.phase_amps) || pump.phase_amps.length !== 3 || pump.phase_amps.some(n => typeof n !== 'number' || !Number.isFinite(n) || n < 0 || n > 1000)) throw Error('Invalid pump phase currents');
      out.phase_amps = pump.phase_amps;
    }
    result[key] = out;
  }
  return JSON.stringify(result);
}
let cachedKeys = null;
let keysUntil = 0;

export async function verifiedIdentity(request, env) {
  if (!env.ACCESS_ISSUER?.startsWith('https://') || !env.ACCESS_ISSUER.endsWith('.cloudflareaccess.com') || !env.ACCESS_AUD || env.ACCESS_AUD.startsWith('REPLACE_')) return null;
  const token = request.headers.get('Cf-Access-Jwt-Assertion');
  if (!token) return null;
  const parts = token.split('.');
  if (parts.length !== 3) return null;
  try {
    const header = JSON.parse(new TextDecoder().decode(base64url(parts[0])));
    const payload = JSON.parse(new TextDecoder().decode(base64url(parts[1])));
    if (header.alg !== 'RS256' || !header.kid || payload.iss !== env.ACCESS_ISSUER ||
        !(Array.isArray(payload.aud) ? payload.aud.includes(env.ACCESS_AUD) : payload.aud === env.ACCESS_AUD) ||
        !Number.isFinite(payload.exp) || payload.exp <= Date.now() / 1000 ||
        (payload.nbf && payload.nbf > Date.now() / 1000) || !email(payload.email)) return null;
    if (!cachedKeys || Date.now() > keysUntil) {
      const response = await fetch(`${env.ACCESS_ISSUER}/cdn-cgi/access/certs`);
      if (!response.ok) return null;
      cachedKeys = (await response.json()).keys;
      keysUntil = Date.now() + 300_000;
    }
    let keyData = cachedKeys.find(k => k.kid === header.kid);
    if (!keyData) {
      cachedKeys = null; keysUntil = 0;
      return null; // Next request refreshes after signing-key rotation.
    }
    const key = await crypto.subtle.importKey('jwk', keyData, { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' }, false, ['verify']);
    const signature = base64url(parts[2]);
    const valid = await crypto.subtle.verify('RSASSA-PKCS1-v1_5', key, signature, new TextEncoder().encode(`${parts[0]}.${parts[1]}`));
    return valid ? payload.email.toLowerCase() : null;
  } catch { return null; }
}

async function body(request) {
  if (!request.headers.get('content-type')?.toLowerCase().startsWith('application/json')) throw Error('Expected JSON');
  if (Number(request.headers.get('content-length') || 0) > 4096) throw Error('Request too large');
  const text = await request.text();
  if (text.length > 4096) throw Error('Request too large');
  const parsed = JSON.parse(text);
  if (!parsed || Array.isArray(parsed) || typeof parsed !== 'object') throw Error('Expected object');
  return parsed;
}

async function sendEmail(env, clientId, stationId, kind, recovery = false) {
  if (!env.RESEND_API_KEY || !email(env.ALERT_FROM?.match(/<([^>]+)>/)?.[1] || env.ALERT_FROM)) return false;
  const row = await env.DB.prepare(`SELECT c.alert_email, s.name station, p.name site, c.name client
    FROM stations s JOIN sites p ON p.id=s.site_id JOIN clients c ON c.id=s.client_id
    WHERE s.id=? AND c.id=?`).bind(stationId, clientId).first();
  if (!row || !email(row.alert_email)) return false;
  const issue = kind === 'offline' ? 'station stopped reporting' : 'lift-station alarm contact active';
  const subject = `${recovery ? 'RESTORED' : 'ALERT'}: ${row.site} / ${row.station}`;
  const text = `${row.client}: ${row.site} / ${row.station} — ${recovery ? 'recovered from' : issue} (${kind}).\n\nCheck the dashboard and confirm conditions at the station. The radio and internet link can delay reporting; this email is not a substitute for onsite alarm equipment.`;
  const response = await fetch('https://api.resend.com/emails', {
    method: 'POST', headers: { authorization: `Bearer ${env.RESEND_API_KEY}`, 'content-type': 'application/json' },
    body: JSON.stringify({ from: env.ALERT_FROM, to: [row.alert_email], subject, text, tags: [{ name: 'station', value: stationId }, { name: 'kind', value: kind }] })
  });
  return response.ok;
}

async function deliverPending(env, alertId) {
  const alert = await env.DB.prepare('SELECT id,client_id,station_id,kind,status,sent_at,recovery_sent_at FROM alerts WHERE id=?').bind(alertId).first();
  if (!alert) return;
  if (!alert.sent_at) {
    if (await sendEmail(env, alert.client_id, alert.station_id, alert.kind))
      await env.DB.prepare('UPDATE alerts SET sent_at=? WHERE id=? AND sent_at IS NULL').bind(new Date().toISOString(), alert.id).run();
  } else if (alert.status === 'closed' && !alert.recovery_sent_at) {
    if (await sendEmail(env, alert.client_id, alert.station_id, alert.kind, true))
      await env.DB.prepare('UPDATE alerts SET recovery_sent_at=? WHERE id=? AND recovery_sent_at IS NULL').bind(new Date().toISOString(), alert.id).run();
  }
}

async function openAlert(env, station, kind, now) {
  await env.DB.prepare('INSERT OR IGNORE INTO alerts(id,station_id,client_id,kind,opened_at) VALUES(?,?,?,?,?)')
    .bind(id(), station.id, station.client_id, kind, now).run();
  const row = await env.DB.prepare("SELECT id FROM alerts WHERE station_id=? AND kind=? AND status='open'").bind(station.id, kind).first();
  if (row) await deliverPending(env, row.id);
}

async function closeAlert(env, stationId, kind, now) {
  const result = await env.DB.prepare("SELECT id FROM alerts WHERE station_id=? AND kind=? AND status='open'").bind(stationId, kind).first();
  if (!result) return;
  await env.DB.prepare("UPDATE alerts SET status='closed', closed_at=? WHERE id=? AND status='open'").bind(now, result.id).run();
  await deliverPending(env, result.id);
}

async function ingest(request, env, stationId) {
  const token = request.headers.get('authorization')?.match(/^Bearer ([A-Za-z0-9_-]{32,128})$/)?.[1];
  if (!token) return fail('Unauthorized', 401);
  const station = await env.DB.prepare('SELECT id,client_id,token_hash,last_seen_at FROM stations WHERE id=?').bind(stationId).first();
  if (!station || await hash(token) !== station.token_hash) return fail('Unauthorized', 401);
  let data;
  try { data = await body(request); } catch { return fail('Invalid JSON'); }
  if (typeof data.alarm !== 'boolean' || typeof data.message_id !== 'string' || !/^[A-Za-z0-9_-]{8,80}$/.test(data.message_id) ||
      (data.level != null && (typeof data.level !== 'number' || !Number.isFinite(data.level))) ||
      (data.battery != null && (typeof data.battery !== 'number' || !Number.isFinite(data.battery)))) return fail('Invalid telemetry');
  let pumps;
  try { pumps = pumpTelemetry(data.pumps); } catch (e) { return fail(e.message); }
  const note = clean(data.note, 240);
  const observed = typeof data.observed_at === 'string' ? new Date(data.observed_at) : null;
  if (!observed || !Number.isFinite(observed.getTime()) || observed.getTime() > Date.now() + 5 * 60_000) return fail('Valid observed_at required (gateway UTC clock)');
  const now = new Date().toISOString();
  // The gateway retries with the same message_id; the row key prevents duplicate readings.
  const readingId = `${stationId}:${data.message_id}`;
  const inserted = await env.DB.prepare('INSERT OR IGNORE INTO readings(id,station_id,client_id,received_at,observed_at,alarm,level,battery,note,pump_telemetry) VALUES(?,?,?,?,?,?,?,?,?,?)')
    .bind(readingId, stationId, station.client_id, now, observed?.toISOString() || null, Number(data.alarm), data.level ?? null, data.battery ?? null, note, pumps).run();
  if (!inserted.meta?.changes) return json({ ok: true, duplicate: true });
  // Delayed packets cannot reset the current alarm state. Only the newest observed time wins.
  const latest = await env.DB.prepare('SELECT MAX(observed_at) AS observed_at FROM readings WHERE station_id=? AND id<>?').bind(stationId, readingId).first();
  const isNewest = Date.now() - observed.getTime() <= 10 * 60_000 && (!latest?.observed_at || observed.toISOString() >= latest.observed_at);
  if (isNewest) {
    await env.DB.prepare('UPDATE stations SET last_seen_at=?,last_status=?,commissioned=1 WHERE id=?').bind(now, data.alarm ? 'alarm' : 'normal', stationId).run();
    await closeAlert(env, stationId, 'offline', now);
    if (data.alarm) await openAlert(env, station, 'alarm', now);
    else await closeAlert(env, stationId, 'alarm', now);
  }
  return json({ ok: true, duplicate: false });
}

async function permission(env, address) {
  if (address === env.ADMIN_EMAIL?.toLowerCase() && !env.ADMIN_EMAIL?.startsWith('REPLACE_')) return { role: 'admin', email: address };
  const rows = await env.DB.prepare('SELECT client_id FROM users WHERE email=?').bind(address).all();
  return rows.results.length ? { role: 'client', email: address, clients: rows.results.map(r => r.client_id) } : null;
}

export function mayAccess(actor, clientId) { return actor.role === 'admin' || actor.clients.includes(clientId); }

async function api(request, env, actor, url) {
  const path = url.pathname, method = request.method;
  if (method !== 'GET' && (request.headers.get('origin') !== url.origin)) return fail('Origin mismatch', 403);
  
if(path==='/api/mesh/remote'&&method==='GET'){
 if(actor.role!=='admin')return fail('Forbidden',403);
 const [g,h,stations,dm]=await Promise.all([
  env.DB.prepare("SELECT gateway_id,version,ota_enabled,ota_version,ota_url,ota_sha256,updated_at FROM mesh_gateway_config WHERE gateway_id='heltec-moab'").first(),
  env.DB.prepare("SELECT received_at,firmware,note FROM mesh_gateway_health WHERE gateway_id='heltec-moab' ORDER BY id DESC LIMIT 1").first(),
  env.DB.prepare('SELECT node_num,node_id,long_name,report_interval_minutes,enabled FROM mesh_stations ORDER BY long_name').all(),
  env.DB.prepare('SELECT node_num,interval_minutes,enabled FROM mesh_gateway_dm_overrides ORDER BY node_num').all()
 ]);
 return json({gateway:g,health:h,stations:stations.results,auto_dm:dm.results});
}
if(path==='/api/mesh/remote'&&method==='POST'){
 if(actor.role!=='admin')return fail('Forbidden',403);
 const v=await body(request);
 const gate=await env.DB.prepare("SELECT version FROM mesh_gateway_config WHERE gateway_id='heltec-moab'").first();
 if(!gate)return fail('Gateway config unavailable',503);
 const increment=()=>env.DB.prepare("UPDATE mesh_gateway_config SET version=version+1,updated_at=CURRENT_TIMESTAMP WHERE gateway_id='heltec-moab'");
 if(v.action==='add_node'){
   const id=String(v.node_id||'').trim().toLowerCase(),name=clean(v.long_name,80),minutes=Number(v.report_interval_minutes);
   if(!/^![0-9a-f]{8}$/.test(id)||!name||![15,30,60,180].includes(minutes))return fail('Valid node ID, station name, and interval required',400);
   const node=Number.parseInt(id.slice(1),16),count=await env.DB.prepare('SELECT count(*) AS n FROM mesh_stations WHERE enabled=1').first();
   if(count.n>=24)return fail('Maximum 24 cloud stations',409);
   if(await env.DB.prepare('SELECT node_num FROM mesh_stations WHERE node_num=? OR node_id=?').bind(node,id).first())return fail('Station already exists',409);
   await env.DB.batch([env.DB.prepare('INSERT INTO mesh_stations(node_num,node_id,long_name,sensor_model,sensor_type,report_interval_minutes,enabled) VALUES(?,?,?,?,?,?,1)').bind(node,id,name,'DYP-A08',6,minutes),increment()]);
 }else if(v.action==='set_dm'){
   const node=Number(v.node_num),minutes=Number(v.interval_minutes);
   if(!Number.isInteger(node)||node<1||node>4294967295||typeof v.enabled!=='boolean'||![15,30,45,60,360,720,1440].includes(minutes))return fail('Invalid DM station or interval',400);
   const count=await env.DB.prepare('SELECT count(*) AS n FROM mesh_gateway_dm_overrides').first();
   if(count.n>=16&&!await env.DB.prepare('SELECT node_num FROM mesh_gateway_dm_overrides WHERE node_num=?').bind(node).first())return fail('Maximum 16 DM overrides',409);
   await env.DB.batch([env.DB.prepare('INSERT INTO mesh_gateway_dm_overrides(node_num,enabled,interval_minutes,updated_at) VALUES(?,?,?,CURRENT_TIMESTAMP) ON CONFLICT(node_num) DO UPDATE SET enabled=excluded.enabled,interval_minutes=excluded.interval_minutes,updated_at=CURRENT_TIMESTAMP').bind(node,v.enabled?1:0,minutes),increment()]);
 }else if(v.action==='set_firmware'){
   const enabled=v.ota_enabled===true,version=clean(v.ota_version,63);
   const urlValue=String(v.ota_url||'').trim(),sha=String(v.ota_sha256||'').trim().toLowerCase();
   if(urlValue.length>1024)return fail('Firmware URL too long',400);
   if(enabled){
     if(!version||!/^[0-9a-f]{64}$/.test(sha)||!/^https:\/\/(raw\.githubusercontent\.com|github\.com)\/canyoncountryadventure\/firmware\//.test(urlValue))return fail('Approved GitHub URL, version, and SHA-256 required',400);
     const h=await env.DB.prepare("SELECT received_at,note FROM mesh_gateway_health WHERE gateway_id='heltec-moab' ORDER BY id DESC LIMIT 1").first();
     const age=h?Date.now()-Date.parse(String(h.received_at).replace(' ','T')+'Z'):Infinity;
     if(age>1800000||!h?.note?.includes('loader_ready=1'))return fail('Matching CCA recovery loader has not checked in',409);
   }
   await env.DB.prepare("UPDATE mesh_gateway_config SET ota_enabled=?,ota_version=?,ota_url=?,ota_sha256=?,version=version+1,updated_at=CURRENT_TIMESTAMP WHERE gateway_id='heltec-moab'").bind(enabled?1:0,version,urlValue,sha).run();
 }else return fail('Unknown remote action',400);
 const updated=await env.DB.prepare("SELECT version FROM mesh_gateway_config WHERE gateway_id='heltec-moab'").first();
 return json({ok:true,version:updated.version});
}
if(path==='/api/mesh'&&method==='GET'){
 if(actor.role!=='admin')return fail('Forbidden',403);
 const n=Number(url.searchParams.get('hours'));const hours=[24,72,168,720].includes(n)?n:168;
 const [s,w,b,h]=await Promise.all([
  env.DB.prepare('SELECT node_num,node_id,long_name,sensor_model,report_interval_minutes,enabled,last_received_at FROM mesh_stations WHERE enabled=1 ORDER BY long_name').all(),
  env.DB.prepare("SELECT node_num,received_at,distance_mm,stage_mm,stage_calibrated FROM mesh_observations WHERE received_at>=datetime('now',?) ORDER BY received_at DESC LIMIT 2000").bind('-'+hours+' hours').all(),
  env.DB.prepare("SELECT node_num,received_at,battery_voltage,battery_percent FROM mesh_device_telemetry WHERE received_at>=datetime('now',?) ORDER BY received_at DESC LIMIT 2000").bind('-'+hours+' hours').all(),
  env.DB.prepare('SELECT gateway_id,received_at,firmware,config_version FROM mesh_gateway_health ORDER BY received_at DESC LIMIT 1').first()
 ]);
 return json({stations:s.results,observations:w.results,batteries:b.results,gateway:h});
}
if (path === '/api/me' && method === 'GET') return json(actor);
  if (path === '/api/overview' && method === 'GET') {
    const requested = url.searchParams.get('client');
    if (requested && !mayAccess(actor, requested)) return fail('Forbidden', 403);
    const scope = requested ? [requested] : actor.role === 'admin' ? null : actor.clients;
    let clients, sites, stations, alerts;
    if (scope === null) {
      [clients, sites, stations, alerts] = await Promise.all([
        env.DB.prepare('SELECT id,name,alert_email FROM clients ORDER BY name').all(),
        env.DB.prepare('SELECT id,client_id,name,description FROM sites ORDER BY name').all(),
        env.DB.prepare('SELECT id,client_id,site_id,name,report_interval_minutes,offline_after_minutes,commissioned,last_seen_at,last_status,(SELECT level FROM readings WHERE station_id=stations.id ORDER BY received_at DESC LIMIT 1) AS latest_level,(SELECT battery FROM readings WHERE station_id=stations.id ORDER BY received_at DESC LIMIT 1) AS latest_battery FROM stations ORDER BY name').all(),
        env.DB.prepare("SELECT id,client_id,station_id,kind,status,opened_at,closed_at,sent_at FROM alerts WHERE status='open' ORDER BY opened_at DESC LIMIT 100").all()
      ]);
    } else {
      const q = scope.map(() => '?').join(',');
      [clients, sites, stations, alerts] = await Promise.all([
        env.DB.prepare(`SELECT id,name,alert_email FROM clients WHERE id IN (${q}) ORDER BY name`).bind(...scope).all(),
        env.DB.prepare(`SELECT id,client_id,name,description FROM sites WHERE client_id IN (${q}) ORDER BY name`).bind(...scope).all(),
        env.DB.prepare(`SELECT id,client_id,site_id,name,report_interval_minutes,offline_after_minutes,commissioned,last_seen_at,last_status,(SELECT level FROM readings WHERE station_id=stations.id ORDER BY received_at DESC LIMIT 1) AS latest_level,(SELECT battery FROM readings WHERE station_id=stations.id ORDER BY received_at DESC LIMIT 1) AS latest_battery FROM stations WHERE client_id IN (${q}) ORDER BY name`).bind(...scope).all(),
        env.DB.prepare(`SELECT id,client_id,station_id,kind,status,opened_at,closed_at,sent_at FROM alerts WHERE client_id IN (${q}) AND status='open' ORDER BY opened_at DESC LIMIT 100`).bind(...scope).all()
      ]);
    }
    return json({ clients: clients.results, sites: sites.results, stations: stations.results, alerts: alerts.results });
  }
  if (path === '/api/readings' && method === 'GET') {
    const stationId = url.searchParams.get('station');
    const hours = Number(url.searchParams.get('hours') || 24);
    if (![24, 168, 720].includes(hours)) return fail('Choose 24 hours, 7 days or 30 days');
    const station = await env.DB.prepare('SELECT client_id FROM stations WHERE id=?').bind(stationId).first();
    if (!station || !mayAccess(actor, station.client_id)) return fail('Not found', 404);
    const cutoff = new Date(Date.now() - hours * 3_600_000).toISOString();
    const rows = await env.DB.prepare('SELECT received_at,observed_at,alarm,level,battery,note,pump_telemetry FROM readings WHERE station_id=? AND client_id=? AND received_at>=? ORDER BY received_at DESC LIMIT 12001').bind(stationId, station.client_id, cutoff).all();
    return json({ readings: rows.results.slice(0, 12000).reverse(), truncated: rows.results.length > 12000, hours });
  }
  if (path === '/api/pump-diagnostics' && method === 'GET') {
    const stationId = url.searchParams.get('station'), hours = Number(url.searchParams.get('hours') || 168);
    if (![24, 168, 720].includes(hours)) return fail('Choose 24 hours, 7 days or 30 days');
    const station = await env.DB.prepare('SELECT client_id FROM stations WHERE id=?').bind(stationId).first();
    if (!station || !mayAccess(actor, station.client_id)) return fail('Not found', 404);
    const cutoff = new Date(Date.now() - hours * 3_600_000).toISOString();
    const rows = await env.DB.prepare('SELECT received_at,observed_at,pump_telemetry FROM readings WHERE station_id=? AND client_id=? AND received_at>=? ORDER BY received_at DESC LIMIT 12001').bind(stationId, station.client_id, cutoff).all();
    return json({ ...analyzePumps(rows.results.slice(0, 12000)), truncated: rows.results.length > 12000, hours });
  }
  if (actor.role !== 'admin') return fail('Forbidden', 403);
  if (path === '/api/users' && method === 'GET') {
    const rows = await env.DB.prepare('SELECT email,client_id,display_name FROM users ORDER BY display_name,email').all();
    return json({ users: rows.results });
  }
  if (path === '/api/clients' && method === 'POST') {
    const v = await body(request), name = clean(v.name), alertEmail = clean(v.alert_email, 254).toLowerCase();
    if (!name || !email(alertEmail)) return fail('Name and alert email required');
    const key = id();
    await env.DB.prepare('INSERT INTO clients(id,name,alert_email) VALUES(?,?,?)').bind(key, name, alertEmail).run();
    return json({ id: key }, 201);
  }
  if (path === '/api/users' && method === 'POST') {
    const v = await body(request), address = clean(v.email, 254).toLowerCase(), name = clean(v.name);
    if (!name || !email(address) || !await env.DB.prepare('SELECT id FROM clients WHERE id=?').bind(v.client_id).first()) return fail('Name, valid email and client required');
    await env.DB.prepare('INSERT INTO users(email,client_id,display_name) VALUES(?,?,?) ON CONFLICT(email,client_id) DO UPDATE SET display_name=excluded.display_name').bind(address, v.client_id, name).run();
    return json({ ok: true }, 201);
  }
  if (path === '/api/users' && method === 'DELETE') {
    const v = await body(request), address = clean(v.email, 254).toLowerCase();
    if (!email(address) || address === env.ADMIN_EMAIL?.toLowerCase()) return fail('Invalid client email');
    await env.DB.prepare('DELETE FROM users WHERE email=? AND client_id=?').bind(address, v.client_id).run();
    return json({ ok: true });
  }
  if (path === '/api/sites' && method === 'POST') {
    const v = await body(request), name = clean(v.name);
    if (!name || !await env.DB.prepare('SELECT id FROM clients WHERE id=?').bind(v.client_id).first()) return fail('Name and client required');
    const key = id();
    await env.DB.prepare('INSERT INTO sites(id,client_id,name,description) VALUES(?,?,?,?)').bind(key, v.client_id, name, clean(v.description, 240)).run();
    return json({ id: key }, 201);
  }
  if (path === '/api/stations' && method === 'POST') {
    const v = await body(request), name = clean(v.name);
    const site = await env.DB.prepare('SELECT client_id FROM sites WHERE id=?').bind(v.site_id).first();
    if (!name || !site) return fail('Name and site required');
    const interval = Number(v.report_interval_minutes || 5), offline = Number(v.offline_after_minutes || 15);
    if (!Number.isInteger(interval) || interval < 1 || interval > 60 || !Number.isInteger(offline) || offline < 10 || offline > 240 || offline < interval * 2) return fail('Invalid heartbeat settings');
    const key = id(), token = newToken();
    let sealed; try { sealed = await sealToken(token, key, env); } catch { return fail('Token vault is not configured', 503); }
    await env.DB.prepare('INSERT INTO stations(id,client_id,site_id,name,token_hash,token_ciphertext,report_interval_minutes,offline_after_minutes) VALUES(?,?,?,?,?,?,?,?)')
      .bind(key, site.client_id, v.site_id, name, await hash(token), sealed, interval, offline).run();
    return json({ id: key, token, warning: 'The token can be revealed later by the CCES administrator.' }, 201);
  }
  const tokenRoute = path.match(/^\/api\/stations\/([a-f0-9-]{36})\/(reveal|rotate)$/);
  if (tokenRoute && method === 'POST') {
    const station = await env.DB.prepare('SELECT id,token_ciphertext FROM stations WHERE id=?').bind(tokenRoute[1]).first();
    if (!station) return fail('Station not found', 404);
    if (tokenRoute[2] === 'reveal') {
      if (!station.token_ciphertext) return fail('Old token cannot be recovered. Replace it to save an encrypted copy.', 409);
      try { return json({ token: await openToken(station.token_ciphertext, station.id, env) }); } catch { return fail('Token vault unavailable', 503); }
    }
    const token = newToken();
    let sealed; try { sealed = await sealToken(token, station.id, env); } catch { return fail('Token vault unavailable', 503); }
    await env.DB.prepare('UPDATE stations SET token_hash=?,token_ciphertext=? WHERE id=?').bind(await hash(token), sealed, station.id).run();
    return json({ token, warning: 'Previous gateway token is now invalid. Update the gateway.' });
  }
  return fail('Not found', 404);
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    try {
      if (url.pathname === '/api/health' && request.method === 'GET') return json({ ok: true });
      if (env.MODE === 'ingest') {
        const match = url.pathname.match(/^\/api\/ingest\/([a-f0-9-]{36})$/);
        if (match && request.method === 'POST') return await ingest(request, env, match[1]);
        return fail('Not found', 404);
      }
      if (env.MODE !== 'dashboard') return fail('Setup incomplete', 503);
      if (!env.DB || !env.ACCESS_AUD || env.ACCESS_AUD.startsWith('REPLACE_')) return fail('Setup incomplete', 503);
      const identity = await verifiedIdentity(request, env);
      if (!identity) return fail('Cloudflare Access authentication required', 401);
      const actor = await permission(env, identity);
      if (!actor) return fail('No client access assigned', 403);
      if (url.pathname.startsWith('/api/')) return await api(request, env, actor, url);
      if (url.pathname === '/' && request.method === 'GET') return new Response(dashboard, { headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store', 'content-security-policy': "default-src 'none'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; base-uri 'none'; frame-ancestors 'none'", 'x-content-type-options': 'nosniff', 'referrer-policy': 'no-referrer' } });
      return fail('Not found', 404);
    } catch (error) {
      console.error('Request failed', error?.message); // Never log tokens or request bodies.
      return fail('Server error', 500);
    }
  },
  async scheduled(_controller, env) {
    if (env.MODE !== 'ingest') return;
    const now = new Date().toISOString();
    const stations = await env.DB.prepare('SELECT id,client_id,last_seen_at,offline_after_minutes FROM stations WHERE commissioned=1').all();
    for (const station of stations.results) {
      if (Date.now() - Date.parse(station.last_seen_at) >= station.offline_after_minutes * 60_000)
        await openAlert(env, station, 'offline', now);
    }
    const pending = await env.DB.prepare("SELECT id FROM alerts WHERE sent_at IS NULL OR (status='closed' AND sent_at IS NOT NULL AND recovery_sent_at IS NULL) ORDER BY opened_at LIMIT 50").all();
    for (const alert of pending.results) await deliverPending(env, alert.id);
  }
};

