// The web debug/viewer page served at http://192.168.4.1/ (same origin as the API).
// Vanilla JS, no external assets (the rig is offline). Polls /api/status, tails /logs, edits
// /config, lists photos, triggers the shutter. Spec: docs/COMPANION.md "Web debug page".
#pragma once
#include <pgmspace.h>

static const char kWebPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>x4 · debug</title>
<style>
:root{--bg:#000;--sf:#141414;--ln:#2e2e2e;--fg:#f2efe8;--mu:#8c877e;--ac:#ff4a1c;--ok:#7bd88f;--wa:#ffb020;--er:#ff5a4f;
--serif:"Instrument Serif",Georgia,"Times New Roman",serif;--mono:"Space Mono",ui-monospace,Menlo,Consolas,monospace}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:13px/1.45 var(--mono)}
header{display:flex;align-items:baseline;gap:16px;padding:20px 20px 8px}
h1{font:400 44px/1 var(--serif);margin:0;letter-spacing:-.5px}h2{font:400 26px/1.1 var(--serif);margin:0 0 10px}
.mu{color:var(--mu)}main{display:grid;gap:14px;padding:12px 20px 40px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}
section{background:var(--sf);border:1px solid var(--ln);border-radius:12px;padding:16px}
table{width:100%;border-collapse:collapse}td,th{padding:4px 6px;border-bottom:1px solid var(--ln);text-align:left;vertical-align:top}
th{color:var(--mu);font-weight:400}.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px;background:var(--er)}
.on{background:var(--ok)}.warn{color:var(--wa)}.err{color:var(--er)}.ok{color:var(--ok)}
button{font:inherit;color:var(--fg);background:#1f1f1f;border:1px solid var(--ln);border-radius:999px;padding:8px 14px;cursor:pointer}
button:hover{border-color:var(--mu)}button.pri{background:var(--ac);border-color:var(--ac);color:#000}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
#log{height:300px;overflow:auto;white-space:pre;font-size:11px;background:#000;border-radius:8px;padding:8px}
input{font:inherit;background:#000;color:var(--fg);border:1px solid var(--ln);border-radius:6px;padding:3px 6px;width:90px}
.shots img{width:100%;aspect-ratio:4/3;object-fit:cover;border-radius:6px;background:#000}
.grid4{display:grid;grid-template-columns:1fr 1fr;gap:6px}.big{font:400 40px/1 var(--serif)}
.bar{height:4px;background:var(--ln);border-radius:2px;overflow:hidden}.bar i{display:block;height:100%;background:var(--ok);width:0}
</style></head><body>
<header><h1>Four Eyes</h1><span class="mu" id="hdr">connecting…</span></header>
<main>
<section><h2>Rig</h2>
<div class="row" style="justify-content:space-between"><div><div class="big" id="state">–</div><div class="mu" id="faults"></div></div>
<div style="text-align:right"><div class="big" id="shots">0</div><div class="mu">shots</div></div></div>
<div class="row" style="margin-top:12px"><button class="pri" onclick="act('shutter')">Shoot</button>
<button onclick="act('flash_arm')">Arm flash</button><button onclick="act('flash_disarm')">Disarm</button>
<button onclick="act('preview_on')">Preview on</button><button onclick="act('preview_off')">off</button>
<button onclick="if(confirm('Power bank fully charged? Resets the battery estimate to 100%.'))act('bat_full')">Bank charged</button></div>
<table style="margin-top:12px" id="sys"></table></section>
<section><h2>Cameras</h2><table id="cams"></table><p class="mu" id="lastshot"></p></section>
<section><h2>Log</h2><div class="row" style="margin-bottom:8px">
<select id="lvl"><option value="">all</option><option>ERROR</option><option>WARN</option><option>INFO</option><option>DEBUG</option></select>
<input id="flt" placeholder="filter"><button onclick="logEl.textContent='';">clear</button><label><input type="checkbox" id="pause" style="width:auto"> pause</label></div>
<div id="log"></div></section>
<section><h2>Config</h2><p class="mu">Safety options (⚑) can only be tightened. Changes persist in NVS. [cam] options reach the cameras when they next register (reboot them, ↻ = reboot needed).</p><table id="cfg"></table></section>
<section><h2>Photos</h2><div class="row"><button onclick="shots()">refresh</button><button onclick="act('sd_bench')">SD speed test</button></div><div id="shots" class="shots"></div></section>
<section><h2>Bring-up</h2><p class="warn">Flash test fires one 50 ms pulse. Only with the Flashlight Unit wired as in docs/WIRING.md.</p>
<div class="row"><button onclick="act('flash_arm')">1. Arm</button><button onclick="if(confirm('Fire one 50 ms flash pulse?'))act('flash_test')">2. Test pulse</button>
<button onclick="if(confirm('Reboot the Core?'))act('reboot')">Reboot</button></div></section>
</main>
<script>
const $=id=>document.getElementById(id),logEl=$('log');let seq=0;
const esc=s=>String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
async function act(a){const r=await fetch('/api/action?a='+a,{method:'POST'});const j=await r.json();if(!j.ok)alert(j.detail||'failed')}
function kb(b){return b>1048576?(b/1048576).toFixed(2)+' MB':(b/1024).toFixed(0)+' kB'}
async function status(){try{const s=await (await fetch('/api/status')).json();
$('hdr').textContent=`fw ${s.fw} · ${s.git} · up ${(s.uptime_ms/1000|0)} s · ${s.ap_stations} stations`;
$('state').textContent=s.state;$('shots').textContent=s.shots_taken;
const f=[];if(s.faults&1)f.push('no cameras');if(s.faults&2)f.push('SD missing');if(s.faults&4)f.push('SD full');if(s.faults&8)f.push('overheat');
$('faults').innerHTML=f.length?'<span class=err>'+f.join(' · ')+'</span>':'ready';
const p=s.power,y=s.system,d=s.sd,u=s.ui,fl=s.flash;
$('sys').innerHTML=[['5V out (BUS_OUT_EN)',p.bus_out_en?'<b class=err>ON (unsafe)</b>':'<span class=ok>off</span>'],
['USB OTG',p.usb_otg_en?'<b class=err>ON</b>':'<span class=ok>off</span>'],['rail (VBUS)',p.vbus_mv+' mV'],['battery (estimate)',`~${p.battery_pct_est}% · ${p.battery_hours_est.toFixed(1)} h left at ${p.power_mw_est} mW · ${p.battery_used_mwh.toFixed(0)} mWh used`],
['chip temp',`<span class=${y.overheat?'err':y.hot?'warn':''}>${y.temp_c.toFixed(1)} °C</span>`],['heap / min / psram',`${kb(y.heap)} / ${kb(y.heap_min)} / ${kb(y.psram)}`],
['reset · boots · brown-outs',`${y.reset} · ${y.boots} · ${y.brownouts}`],['SD',d.mounted?`${d.free_mb} / ${d.total_mb} MB free · w ${d.write_kBps.toFixed(0)} kB/s r ${d.read_kBps.toFixed(0)} kB/s`:'<span class=err>not mounted</span>'],
['UI',`${u.fps.toFixed(1)} fps · worst frame ${(u.frame_max_us/1000).toFixed(1)} ms · preview ${u.preview_on?u.preview_fps.toFixed(1)+' fps':'off'}`],
['flash',`${fl.enabled?'enabled':'disabled'} · ${fl.armed?'armed':'not armed'} · ${fl.ready?'ready':'wait '+(fl.ready_in_ms/1000).toFixed(1)+' s'} · ${fl.pulses} pulses`],
['button',u.button_down?'pressed':'released']].map(r=>`<tr><th>${r[0]}</th><td>${r[1]}</td></tr>`).join('');
$('cams').innerHTML='<tr><th>#</th><th>ip</th><th>state</th><th>temp</th><th>rssi</th><th>shot</th></tr>'+s.cams.map(c=>{
const pct=c.xfer_total?Math.min(100,100*c.xfer_bytes/c.xfer_total):0;
return `<tr><td><span class="dot ${c.online?'on':''}"></span>${c.id}</td><td>${c.ip}<br><span class=mu>${c.fw} · 0x${c.sensor.toString(16)}</span></td><td>${c.state}<br><span class=mu>hb ${c.hb_age_ms} ms</span></td><td>${c.temp_c}°</td><td>${c.rssi}</td><td>${c.shot_stored?'<span class=ok>stored</span>':c.shot_failed?'<span class=err>failed</span>':c.shot_ok?'captured':'–'} ${c.bytes?kb(c.bytes):''}<br><span class=mu>${(c.cap_offset_us/1000).toFixed(1)} ms vs plan</span><div class=bar><i style="width:${pct}%"></i></div></td></tr>`}).join('');
const l=s.last_shot;$('lastshot').textContent=l.id?`last shot #${l.id}: ${l.stored}/${l.expected} stored · captured in ${l.captured_ms} ms · all stored in ${l.total_ms} ms · skew ${(l.skew_us/1000).toFixed(1)} ms`:'no shot yet';
}catch(e){$('hdr').textContent='offline: '+e}}
async function logs(){if($('pause').checked)return;try{const r=await fetch('/logs?since='+seq);seq=+r.headers.get('X-Log-Seq')||seq;const t=await r.text();
const lv=$('lvl').value,fq=$('flt').value;const lines=t.split('\n').filter(x=>x&&(!lv||x.includes('lvl='+lv))&&(!fq||x.includes(fq)));
if(lines.length){const atEnd=logEl.scrollTop+logEl.clientHeight>=logEl.scrollHeight-4;logEl.insertAdjacentHTML('beforeend',lines.map(x=>{const c=x.includes('lvl=ERROR')?'err':x.includes('lvl=WARN')?'warn':'';return `<span class="${c}">${esc(x)}</span>`}).join('\n')+'\n');if(atEnd)logEl.scrollTop=logEl.scrollHeight}}catch(e){}}
async function cfg(){const c=await (await fetch('/config')).json();$('cfg').innerHTML='<tr><th>option</th><th>value</th><th>range</th></tr>'+c.options.map(o=>
`<tr title="${esc(o.doc)}"><td>${o.name}${o.scope=='cam'?' <span class=mu>[cam]</span>':''}${o.safety?' <span class=warn>⚑'+o.safety+'</span>':''}${o.reboot?' <span class=mu>↻</span>':''}<br><span class=mu>${esc(o.doc)}</span></td><td><input value="${o.value}" onchange="setc('${o.name}',this)"> ${o.unit}</td><td class=mu>${o.min}…${o.max}<br>def ${o.default}</td></tr>`).join('')}
async function setc(k,el){const r=await (await fetch('/config?'+k+'='+encodeURIComponent(el.value),{method:'POST'})).json();const x=r.results[0];el.value=x.value;el.style.borderColor=x.clamped?'var(--wa)':'var(--ok)';if(x.clamped)el.title='clamped: '+x.clamped}
async function shots(){const s=await (await fetch('/api/shots')).json();$('shots').innerHTML=s.shots.map(sh=>`<h3 class=mu>${sh.id}</h3><div class=grid4>`+sh.files.filter(f=>f.name.endsWith('.jpg')).map(f=>`<a href="/file?path=/x4/shots/${sh.id}/${f.name}" target=_blank><img loading=lazy src="/file?path=/x4/shots/${sh.id}/${f.name}" title="${f.name} ${kb(f.size)}"></a>`).join('')+`</div><a class=mu href="/file?path=/x4/shots/${sh.id}/shot.json" target=_blank>shot.json</a>`).join('')||'<p class=mu>no photos yet</p>'}
status();cfg();logs();setInterval(status,1000);setInterval(logs,1000);
</script></body></html>)HTML";
