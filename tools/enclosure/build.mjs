import { build } from 'esbuild'
import fs from 'node:fs'
import path from 'node:path'

const here = path.dirname(new URL(import.meta.url).pathname)
const out = path.join(here, 'out')
const docs = path.join(here, '..', '..', 'docs', 'assets', 'enclosure')
const js = (await build({ entryPoints: [path.join(here, 'app.js')], bundle: true, minify: true, format: 'iife', write: false }))
  .outputFiles[0].text
const steps = fs.readFileSync(path.join(here, 'steps.js'), 'utf8')
const sceneJson = fs.readFileSync(path.join(out, 'scene.json'), 'utf8')
const F = JSON.parse(sceneJson).facts
const sheets = [1, 2, 3].map(n => fs.readFileSync(path.join(docs, `sheet-${n}.svg`), 'utf8'))

const html = `<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>x4cam foam-board body: step by step</title>
<style>
:root{--ink:#1d2125;--mute:#5b636b;--line:#d5d9de;--acc:#e8711a}
*{box-sizing:border-box}body{margin:0;font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;color:var(--ink);background:#fafbfc}
header{padding:14px 20px;border-bottom:1px solid var(--line);display:flex;gap:16px;align-items:baseline;flex-wrap:wrap}
header h1{font-size:18px;margin:0}header span{color:var(--mute);font-size:13px}
main{display:grid;grid-template-columns:minmax(0,1fr) 400px;height:calc(100vh - 56px);min-height:520px}
#view{position:relative;overflow:hidden}#view canvas{display:block}
.labels{position:absolute;top:0;left:0;pointer-events:none}
.tag{font:600 11px/1 system-ui,sans-serif;background:rgba(29,33,37,.82);color:#fff;padding:3px 6px;border-radius:4px;white-space:nowrap}
#side{border-left:1px solid var(--line);display:flex;flex-direction:column;min-height:0;background:#fff}
#steps{list-style:none;margin:0;padding:8px;display:flex;flex-wrap:wrap;gap:4px;border-bottom:1px solid var(--line)}
#steps li{font-size:12px;padding:4px 8px;border:1px solid var(--line);border-radius:999px;cursor:pointer;color:var(--mute)}
#steps li.on{background:var(--ink);color:#fff;border-color:var(--ink)}
#content{padding:14px 18px;overflow:auto;flex:1}#content h2{font-size:17px;margin:0 0 8px}
#content ul,#content ol{padding-left:20px}#content li{margin:4px 0}
#content table{border-collapse:collapse;font-size:13px;margin:8px 0}#content td{border-top:1px solid var(--line);padding:4px 6px;vertical-align:top}
.warn{font-weight:600;color:var(--acc);margin-bottom:0}
#nav{display:flex;gap:8px;align-items:center;padding:10px 18px;border-top:1px solid var(--line)}
#nav button{font:inherit;padding:6px 14px;border-radius:6px;border:1px solid var(--ink);background:var(--ink);color:#fff;cursor:pointer}
#nav button:disabled{opacity:.3;cursor:default}#nav label{font-size:13px;color:var(--mute)}#nav label:first-of-type{margin-left:auto}
#templates{padding:24px 20px}#templates h2{font-size:18px}
.sheets{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:16px}
.sheet{background:#fff;border:1px solid var(--line);box-shadow:0 1px 3px rgba(0,0,0,.06)}.sheet svg{width:100%;height:auto;display:block}
@media (max-width:900px){main{grid-template-columns:1fr;height:auto}#view{height:60vh}}
@media print{@page{size:A4;margin:0}body{background:#fff}header,main,#templates>*:not(.sheets){display:none!important}
#templates{padding:0}.sheets{display:block}.sheet{border:0;box-shadow:none;break-after:page;width:210mm;height:297mm}
.sheet svg{width:210mm;height:297mm}}
</style></head><body>
<header><h1>x4cam foam-board body</h1><span>step by step · 3 mm foam board · outer ${F.L} × ${F.H} × ${F.D} mm</span></header>
<main><div id="view"></div>
<aside id="side"><ol id="steps"></ol><div id="content"><h2 id="step-title"></h2><div id="step-body"></div></div>
<div id="nav"><button id="prev">← Back</button><button id="next">Next →</button><label><input type="checkbox" id="black"> black board</label><label style="margin-left:12px"><input type="checkbox" id="xray"> X-ray</label></div></aside></main>
<section id="templates"><h2>Templates (1:1)</h2>
<p>Print this page (Ctrl/Cmd+P): only the three sheets print, one per A4 page. Use <b>100 % / actual size</b> and <b>no margins</b>, then measure the 50 mm bar. The PDF version has the same sheets.</p>
<div class="sheets">${sheets.map(s => `<div class="sheet">${s.replace(/^<\?xml[^>]*>/, '')}</div>`).join('\n')}</div></section>
<script id="scene-data" type="application/json">${sceneJson}</script>
<script>${steps}</script>
<script>${js}</script>
</body></html>`
fs.writeFileSync(path.join(docs, 'x4cam-body.html'), html)
console.log('html', (html.length / 1024).toFixed(0), 'KB')
