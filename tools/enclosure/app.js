import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js'
import { CSS2DRenderer, CSS2DObject } from 'three/examples/jsm/renderers/CSS2DRenderer.js'

const scene = JSON.parse(document.getElementById('scene-data').textContent)
const STEPS = window.STEPS
const view = document.getElementById('view')
const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true })
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
view.appendChild(renderer.domElement)
const labels = new CSS2DRenderer()
labels.domElement.className = 'labels'
view.appendChild(labels.domElement)

const world = new THREE.Scene()
world.background = new THREE.Color('#eef0f2')
const camera = new THREE.PerspectiveCamera(30, 1, 1, 5000)
const controls = new OrbitControls(camera, renderer.domElement)
controls.enableDamping = true
world.add(new THREE.HemisphereLight('#ffffff', '#9aa3ad', 1.6))
const sun = new THREE.DirectionalLight('#ffffff', 1.6); sun.position.set(-150, 300, 250); world.add(sun)
const fill = new THREE.DirectionalLight('#ffffff', 0.7); fill.position.set(300, 120, -300); world.add(fill)

let FOAM = '#f6f3ec'; const NEW = '#ffcf9e'
const edgeMat = new THREE.LineBasicMaterial({ color: '#3d4247', transparent: true, opacity: 0.55 })
const items = [] // {obj, step, kind:'panel'|'comp', id, mats:[], base:color, label}

function basis(pl) {
  const m = new THREE.Matrix4()
  m.makeBasis(new THREE.Vector3(...pl.U), new THREE.Vector3(...pl.V), new THREE.Vector3(...pl.N))
  m.setPosition(...pl.O)
  return m
}

function panelMesh(p, cut = false) {
  const shape = new THREE.Shape(((cut && p.outline_cut) || p.outline).map(([x, y]) => new THREE.Vector2(x, y)))
  for (const h of ((cut && p.holes_cut) || p.holes)) {
    const path = new THREE.Path()
    if (h.poly) { path.moveTo(...h.poly[0]); for (const q of h.poly.slice(1)) path.lineTo(...q); path.closePath() }
    else if (h.rrect) {
      const [x, y, w, hh, r] = h.rrect
      path.moveTo(x + r, y); path.lineTo(x + w - r, y); path.quadraticCurveTo(x + w, y, x + w, y + r)
      path.lineTo(x + w, y + hh - r); path.quadraticCurveTo(x + w, y + hh, x + w - r, y + hh)
      path.lineTo(x + r, y + hh); path.quadraticCurveTo(x, y + hh, x, y + hh - r)
      path.lineTo(x, y + r); path.quadraticCurveTo(x, y, x + r, y); path.closePath()
    }
    else if (h.circle) { const [cx, cy, r] = h.circle; path.absarc(cx, cy, r, 0, Math.PI * 2, true) }
    else if (h.rect) { const [x, y, w, hh] = h.rect; path.moveTo(x, y); path.lineTo(x, y + hh); path.lineTo(x + w, y + hh); path.lineTo(x + w, y); path.closePath() }
    shape.holes.push(path)
  }
  const geo = new THREE.ExtrudeGeometry(shape, { depth: scene.T, bevelEnabled: false, curveSegments: 24 })
  const mat = new THREE.MeshStandardMaterial({ color: FOAM, roughness: 0.9, side: THREE.DoubleSide, transparent: true })
  const mesh = new THREE.Mesh(geo, mat)
  const g = new THREE.Group()
  g.add(mesh)
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(geo, 25), edgeMat))
  g.matrixAutoUpdate = false
  return { g, mat }
}

for (const p of scene.panels) {
  const { g, mat } = panelMesh(p)
  g.matrix.copy(basis(p.place))
  world.add(g)
  const flat = panelMesh(p, true)
  flat.g.matrix.copy(basis(scene.flat[p.id]))
  world.add(flat.g)
  items.push({ obj: g, step: p.step, kind: 'panel', id: p.id, mats: [mat], base: FOAM, name: p.name })
  items.push({ obj: flat.g, step: 1, kind: 'flat', id: p.id, mats: [flat.mat], base: FOAM, name: p.id })
}
for (const s of scene.flat_boxes) {
  const geo = new THREE.BoxGeometry(s.max[0] - s.min[0], s.max[1] - s.min[1], s.max[2] - s.min[2])
  const mat = new THREE.MeshStandardMaterial({ color: FOAM, roughness: 0.9, transparent: true })
  const g = new THREE.Mesh(geo, mat)
  g.position.set((s.min[0] + s.max[0]) / 2, (s.min[1] + s.max[1]) / 2, (s.min[2] + s.max[2]) / 2)
  g.add(new THREE.LineSegments(new THREE.EdgesGeometry(geo), edgeMat))
  world.add(g)
  items.push({ obj: g, step: 1, kind: 'flat', id: s.label, mats: [mat], base: FOAM })
}
for (const c of scene.comps) {
  let geo, pos
  if (c.kind === 'box') {
    geo = new THREE.BoxGeometry(c.max[0] - c.min[0], c.max[1] - c.min[1], c.max[2] - c.min[2])
    pos = [(c.min[0] + c.max[0]) / 2, (c.min[1] + c.max[1]) / 2, (c.min[2] + c.max[2]) / 2]
  } else {
    geo = new THREE.CylinderGeometry(c.r, c.r, c.len, 32)
    if (c.axis === 'z') geo.rotateX(Math.PI / 2)
    if (c.axis === 'x') geo.rotateZ(Math.PI / 2)
    pos = c.c
  }
  const isFoam = c.color === 'foam'
  const mat = new THREE.MeshStandardMaterial({ color: isFoam ? FOAM : c.color, roughness: isFoam ? 0.9 : 0.6, transparent: true })
  const m = new THREE.Mesh(geo, mat)
  m.position.set(...pos)
  if (c.kind === 'box' && Math.min(c.max[0] - c.min[0], c.max[1] - c.min[1], c.max[2] - c.min[2]) > 1) m.add(new THREE.LineSegments(new THREE.EdgesGeometry(geo), edgeMat))
  world.add(m)
  items.push({ obj: m, step: c.step, kind: isFoam ? 'foambox' : 'comp', id: c.id, group: c.group, mats: [mat], base: isFoam ? FOAM : c.color, name: c.name })
}

// labels: one per panel and per component group, placed at the bounding-box centre
const labelObjs = []
const OFF = { 'spacer strips C': [0, -16, 0], 'posts F': [0, -3, 0] }
function addLabel(text, objs, step, kind) {
  const box = new THREE.Box3()
  for (const o of objs) { o.updateMatrixWorld(true); box.expandByObject(o) }
  const div = document.createElement('div'); div.className = 'tag'; div.textContent = text
  const l = new CSS2DObject(div); box.getCenter(l.position); if (OFF[text]) l.position.add(new THREE.Vector3(...OFF[text])); world.add(l)
  labelObjs.push({ l, step, kind })
}
const LBL = { posts: 'posts F', cams: 'cameras ×4', flash: 'flash', strips: 'spacer strips C', wagos: 'WAGO 221-415 ×4', cap: 'capacitor',
  breakout: 'USB-C breakout', core: 'CoreS3-SE', switch: 'switch', bank: 'power bank', button: 'Mini Button' }
for (const p of scene.panels) {
  addLabel(p.id, items.filter(i => i.kind === 'panel' && i.id === p.id).map(i => i.obj), p.step, 'panel')
  addLabel(p.id, items.filter(i => i.kind === 'flat' && i.id === p.id).map(i => i.obj), 1, 'flat')
}
for (const lb of new Set(scene.flat_boxes.map(b => b.label))) addLabel(lb, items.filter(i => i.kind === 'flat' && i.id === lb).map(i => i.obj), 1, 'flat')
const groups = [...new Set(scene.comps.map(c => c.group))]
for (const g of groups) {
  const its = items.filter(i => i.group === g)
  addLabel(LBL[g] ?? g, its.map(i => i.obj), its[0].step, 'comp')
}

let cur = 0, xray = false
function apply(n) {
  cur = n
  const st = STEPS[n]
  for (const it of items) {
    let vis
    if (st.flat) vis = it.kind === 'flat'
    else vis = it.kind !== 'flat' && it.step <= st.upto && !(st.hide || []).includes(it.id)
    it.obj.visible = vis
    const isNew = !st.flat && it.step === st.upto && st.highlight !== false
    for (const m of it.mats) {
      m.color.set(it.kind === 'comp' ? it.base : (isNew ? NEW : (st.black ? '#2b2c2f' : FOAM)))
      const ghost = (xray || (st.ghost || []).includes(it.id)) && it.kind === 'panel'
      m.opacity = ghost ? 0.18 : 1
      m.depthWrite = !ghost
    }
  }
  for (const { l, step, kind } of labelObjs) {
    l.visible = st.nolabels ? false : st.flat ? kind === 'flat' : (kind !== 'flat' && (step === st.upto || (!!st.labelsAll && step <= st.upto && kind === 'comp')))
  }
  edgeMat.color.set(st.black || FOAM !== '#f6f3ec' ? '#8a9096' : '#3d4247')
  world.background.set(st.black ? '#d9dcdf' : '#eef0f2')
  const [p, t] = st.cam
  camera.position.set(...p); controls.target.set(...t); controls.update()
  document.querySelectorAll('#steps li').forEach((li, i) => li.classList.toggle('on', i === n))
  document.getElementById('step-title').textContent = `${n === 0 ? '' : n + '. '}${st.title}`
  document.getElementById('step-body').innerHTML = st.body
  document.getElementById('prev').disabled = n === 0
  document.getElementById('next').disabled = n === STEPS.length - 1
  history.replaceState(null, '', '#step-' + n)
}
window.goStep = apply; window.__labels = labelObjs
const ol = document.getElementById('steps')
STEPS.forEach((s, i) => { const li = document.createElement('li'); li.textContent = (i ? i + '. ' : '') + s.short; li.onclick = () => apply(i); ol.appendChild(li) })
document.getElementById('prev').onclick = () => apply(Math.max(0, cur - 1))
document.getElementById('next').onclick = () => apply(Math.min(STEPS.length - 1, cur + 1))
document.getElementById('xray').onchange = e => { xray = e.target.checked; apply(cur) }
const bb = document.getElementById('black'); if (bb) bb.onchange = e => { FOAM = e.target.checked ? '#2b2c2f' : '#f6f3ec'; edgeMat.color.set(e.target.checked ? '#8a9096' : '#3d4247'); for (const it of items) if (it.kind !== 'comp') it.base = FOAM; apply(cur) }
window.setBlack = v => { if (bb) { bb.checked = v; bb.onchange({ target: bb }) } }
document.addEventListener('keydown', e => { if (e.key === 'ArrowRight') document.getElementById('next').click(); if (e.key === 'ArrowLeft') document.getElementById('prev').click() })

function resize() {
  const w = view.clientWidth, h = view.clientHeight
  renderer.setSize(w, h); labels.setSize(w, h); camera.aspect = w / h; camera.updateProjectionMatrix()
}
new ResizeObserver(resize).observe(view)
resize()
const m = location.hash.match(/step-(\d+)/)
apply(m ? Math.min(+m[1], STEPS.length - 1) : 0)
function loop() { controls.update(); renderer.render(world, camera); labels.render(world, camera); requestAnimationFrame(loop) }
loop()
window.__ready = true
