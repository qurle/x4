// Renders the step pictures and the PDF from docs/assets/enclosure/x4cam-body.html.
//   node render.cjs   -> docs/assets/enclosure/step-<n>.jpg (for ENCLOSURE.md) + x4cam-body.pdf
// Needs Playwright with Chromium (npx playwright install chromium) and WebGL (SwiftShader is fine).
const { chromium } = require('playwright')
const fs = require('fs')
const path = require('path')
const docs = path.join(__dirname, '..', '..', 'docs', 'assets', 'enclosure')
const tmp = path.join(__dirname, 'out')
const window = {}
eval(fs.readFileSync(path.join(__dirname, 'steps.js'), 'utf8'))
const STEPS = window.STEPS

;(async () => {
  const b = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] })
  for (const [scale, dir, type] of [[1, docs, 'jpeg'], [2, tmp, 'png']]) {
    const pg = await b.newPage({ viewport: { width: 1400, height: 860 }, deviceScaleFactor: scale })
    const errs = []
    pg.on('pageerror', e => errs.push(String(e)))
    await pg.goto('file://' + path.join(docs, 'x4cam-body.html'))
    await pg.waitForFunction(() => window.__ready, null, { timeout: 30000 })
    for (let i = 0; i < STEPS.length; i++) {
      await pg.evaluate(i => window.goStep(i), i)
      await pg.waitForTimeout(600)
      const file = path.join(dir, `step-${i}.${type === 'jpeg' ? 'jpg' : 'png'}`)
      await pg.locator('#view').screenshot(type === 'jpeg' ? { path: file, type, quality: 86 } : { path: file })
    }
    if (errs.length) throw new Error(errs.join('\n'))
    await pg.close()
  }
  const img = i => 'data:image/png;base64,' + fs.readFileSync(path.join(tmp, `step-${i}.png`)).toString('base64')
  const pages = STEPS.map((s, i) => `<section class="page"><div class="hd">x4cam foam-board body · ${i === 0 ? 'overview' : 'step ' + i + ' of ' + (STEPS.length - 1)}</div>
<h2>${i ? i + '. ' : ''}${s.title}</h2><img src="${img(i)}"><div class="body">${s.body.replace(/<p>Drag to rotate[^<]*<\/p>/, '')}</div></section>`).join('')
  const sheets = [1, 2, 3].map(n => `<section class="sheet">${fs.readFileSync(path.join(docs, `sheet-${n}.svg`), 'utf8')}</section>`).join('')
  const html = `<!DOCTYPE html><html><head><meta charset="utf-8"><style>
@page{size:A4;margin:0}body{margin:0;font:10.5pt/1.45 system-ui,-apple-system,Segoe UI,Roboto,Helvetica,sans-serif;color:#1d2125}
.page{width:210mm;height:297mm;padding:12mm 16mm;box-sizing:border-box;break-after:page;overflow:hidden}
.hd{font-size:8pt;color:#6b737b;border-bottom:.3mm solid #d5d9de;padding-bottom:2mm;margin-bottom:4mm}
h2{font-size:15pt;margin:0 0 3mm}img{width:100%;height:auto;border-radius:2mm;background:#eef0f2}
.body ul,.body ol{padding-left:6mm;margin:2mm 0}.body li{margin:1.2mm 0}.body p{margin:2mm 0}
table{border-collapse:collapse;font-size:9pt}td{border-top:.2mm solid #d5d9de;padding:1mm 2mm;vertical-align:top}
.warn{font-weight:600;color:#c85e10}.sheet{width:210mm;height:297mm;break-after:page}.sheet svg{width:210mm;height:297mm;display:block}
</style></head><body>${pages}${sheets}</body></html>`
  const pg = await b.newPage()
  await pg.setContent(html, { waitUntil: 'load' })
  await pg.pdf({ path: path.join(docs, 'x4cam-body.pdf'), format: 'A4', printBackground: true, margin: { top: 0, bottom: 0, left: 0, right: 0 }, preferCSSPageSize: true })
  await b.close()
  console.log(`${STEPS.length} steps rendered; PDF written`)
})()
