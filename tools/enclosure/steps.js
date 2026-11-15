// Step list for the viewer and the PDF. upto = show parts with step <= upto.
const C = [78.75, 41.75, 16]
const at = (dx, dy, dz, t = [0, 0, 0]) => [[C[0] + dx, C[1] + dy, C[2] + dz], [C[0] + t[0], C[1] + t[1], C[2] + t[2]]]
const ALL = ['A', 'D', 'K', 'T', 'G', 'M', 'R', 'B1', 'B2', 'B3', 'P1', 'P2']

window.STEPS = [
  {
    short: 'Overview', title: 'What you are building', upto: 8, ghost: ALL, labelsAll: true, highlight: false,
    cam: at(150, 110, -235, [5, 0, 0]),
    body: `
<p>Your render, in 3 mm board: <b>157.5 × 83.5 × 32 mm</b>, plus the Mini Button on top. The box is exactly as tall as the bank, so the bank reads as the grip and nothing sticks out except the button.</p>
<ul>
<li><b>Grip:</b> the bank slides into a sleeve from the left end. Its own end is the end of the grip.</li>
<li><b>Front:</b> four lenses in a row (24.1 mm apart) from the grip, ~18 mm of plain front to the right of them, and a small rounded flash window in the top-right corner.</li>
<li><b>Inside, front layer:</b> cameras touching, Grove ends down, held by a laminated top bar, posts and the walls.</li>
<li><b>Cooling gap:</b> 3 mm spacer strips on the camera backs. Everything rests on something: front ← cameras ← strips ← Core and flash ← door.</li>
<li><b>Back layer:</b> breakout and switch by the bank wall, the Core in the door window, the flash half behind camera 4 with its LED just past camera 4's edge and above it, the capacitor under the flash.</li>
<li><b>Bottom:</b> 3 WAGOs in a row and 1 on top of the middle one, pushed against the door, holes facing forward. The Grove cables drop straight into them.</li>
</ul>
<p>Drag to rotate, scroll to zoom, ← → for steps. "Black board" shows it in black foam board; "X-ray" makes the walls see-through.</p>`,
  },
  {
    short: 'Measure, print, cut', title: 'Measure, print, cut', upto: 0, flat: true,
    cam: [[178, 940, 30], [178, 0, -150]],
    body: `
<p><b>Measure first.</b> If anything differs, send me the numbers and I'll regenerate the sheets.</p>
<table>
<tr><td>foam board</td><td>3.0 mm</td><td>"3 mm" board is often 3.0–3.3</td></tr>
<tr><td>bank</td><td>37 × 77 × 25</td><td>listings differ (76–77 × 35.5–37 × 25–25.4)</td></tr>
<tr><td>bank plug</td><td>socket centre 40 mm below the top inner face, 15 mm behind the front inner face</td><td><b>Not sure</b>: check where your plug comes out</td></tr>
<tr><td>flash LED</td><td>23.5 mm from the Grove end</td><td>from M5's 3D model (±0.5)</td></tr>
<tr><td>capacitor</td><td>Ø10 × 20</td><td>room for about Ø13 × 22 under the flash</td></tr>
<tr><td>Grove plug</td><td>sticks out ~6.5 mm</td><td><b>Not sure</b>: measure one</td></tr>
<tr><td>switch, SD slot, button cable</td><td>dashed on the sheets</td><td>mark from the real part</td></tr>
</table>
<p><b>Print</b> the 3 pages at <b>100 % / actual size</b> (no "fit to page") and measure the 50 mm bar on each.</p>
<p><b>Cut list:</b> A front, D door, K sleeve back, B1–B3 top bar, F1–F2 post strips, C × 8 spacer strips, P1–P2 flash pad, T top, G bottom, M middle wall, R right wall.</p>
<ul>
<li>Fresh blade, steel ruler, <b>three passes</b> (top paper, foam, bottom paper), blade vertical.</li>
<li>Lens holes and the flash window: cut 0.5 mm small, then sand to size.</li>
<li>Cut only the solid lines now. The dashed ones wait for the real parts.</li>
</ul>`,
  },
  {
    short: 'Laminate', title: 'Laminate the top bar and the posts', upto: 2,
    cam: [[150, 140, 250], [88, 72, 24]],
    body: `
<ul>
<li><b>Top bar:</b> glue B1 + B2 + B3 with thin PVA, left ends and top edges flush. <b>B3 goes at the back</b>: it's the long one, and its 2 mm lip stops camera 4 from sliding up into the flash opening.</li>
<li><b>Posts:</b> glue F1 + F2 the same way. When dry, cut the 5 dashed notches through both layers at once, so the posts match.</li>
<li>Press both flat under books for 2 h or more.</li>
</ul>`,
  },
  {
    short: 'Onto the front', title: 'Top bar and posts onto the front', upto: 3,
    cam: at(30, 100, -280),
    body: `
<ul>
<li>Lay A face down. Glue the top bar on its inside face, <b>top edge 3 mm below A's top edge</b>, <b>left end 40.5 mm from A's left end</b>. B3's lip sits behind the flash window.</li>
<li>Put the four cameras in, lenses in their holes, tops against the bar. Slide the posts under them, one between each pair of Grove plugs and one at each end, and glue the posts to A.</li>
<li>Take the cameras out before the glue sets. Keep glue 5 mm away from every hole.</li>
</ul>`,
  },
  {
    short: 'Shell', title: 'Build the shell', upto: 4,
    cam: at(110, 150, -265),
    body: `
<ol>
<li><b>M</b> (middle wall) on A's inside, 37.5 mm from A's left end, touching the left ends of the top bar and posts. "front →" on M faces A.</li>
<li><b>R</b> (right wall) at A's right end.</li>
<li><b>T</b> and <b>G</b> on top and bottom, the full 157.5 mm, over the ends of M and R. T's slots go over the gap behind the cameras.</li>
<li><b>K</b> closes the back of the sleeve. The left end stays open: that's where the bank goes in.</li>
</ol>
<ul>
<li>Use a low-temperature glue gun and hold each joint square for about 20 s. Then run a fillet along every inside corner.</li>
<li>Check it's square: the two diagonals of the back opening should match within 1 mm.</li>
<li>Sand the four outside edges at the sleeve's open end to a small radius (3 mm) and wrap them in paper tape. That's the rounded grip end.</li>
</ul>`,
  },
  {
    short: 'Cameras + gap', title: 'Cameras and the cooling gap', upto: 5,
    cam: at(55, 115, -265, [5, 6, 5]),
    body: `
<ul>
<li>Plug the Grove cables in first. Then put the cameras in, <b>Grove end down</b>, numbered 1–4 from the bank side.</li>
<li>A strip of paper tape across each camera's back, from the top bar to the posts, holds it in.</li>
<li><b>Spacer strips C</b>: double-sided tape, two per camera along the long edges. The 3 mm channel between them is the air path: in through R's slots, up behind the cameras, out through T's slots.</li>
<li>Check the picture orientation now (<b>Not sure</b> what Grove-down does to it). Take one shot and look at it.</li>
</ul>`,
  },
  {
    short: 'Flash, cap, WAGOs', title: 'Flash, capacitor, breakout, WAGOs', upto: 6, hide: ['K'],
    cam: at(75, 85, -280, [5, -8, 0]),
    body: `
<ul>
<li><b>Flash</b>: Grove end down, face forward, top 0.5 mm under T, ~1 mm from R. Its left half rests on camera 4's spacer strip. Look through the window in A: you should see the whole LED, just right of camera 4's edge and above it.</li>
<li><b>Capacitor</b>: lying under the flash, below its Grove plug, taped to R. Mind the polarity stripe.</li>
<li><b>Breakout</b>: on M, socket through M's hole, mouth <b>flush with M's sleeve-side face</b>. Anker says the plug reaches through 4.9 mm phone cases, so a 3 mm wall is fine. Hot glue the board's edges, not the socket.</li>
<li><b>WAGOs</b>: 3 in a row on the bottom and 1 on top of the middle one, <b>holes facing forward</b>, backs against the door line, hook-and-loop underneath. The Grove cables come straight down in the 7 mm in front of them. Power wires from the back layer go round the ends of the row.</li>
<li>Levers: the outer two have ~15 mm free above them. The middle pair slide out backwards when the door is open.</li>
<li>Wire everything as in WIRING.md. The Mini Button cable comes in through the hole in T, just behind camera 1.</li>
</ul>`,
  },
  {
    short: 'Door', title: 'The door: Core, switch, flash pad', upto: 7,
    cam: at(130, 95, -255),
    body: `
<ul>
<li><b>Core</b>: press it into D's window from inside, screen out, flush with the outside face. Two strips of tape across its inner face onto the door hold it when the door is open.</li>
<li><b>Flash pad</b>: glue P1 + P2 inside the door on the dashed mark. With the door closed, the pad presses the flash onto camera 4's strips.</li>
<li><b>Switch</b>: snap-in rockers are usually made for 1–2 mm panels (<b>Not sure</b> about yours). In 3 mm foam the clips won't catch, so hot glue it from inside or use a thin plastic plate.</li>
<li><b>Hinge</b>: cloth or gaffer tape along the top edge, inside and outside. Close it with a hook-and-loop tab at the bottom. Leave about 30 mm of slack in the Core and switch wires.</li>
</ul>`,
  },
  {
    short: 'Bank + button', title: 'Bank and Mini Button', upto: 8, ghost: ['A'],
    cam: at(-195, 105, 220, [-30, 0, 0]),
    body: `
<ul>
<li>Put a hook-and-loop pad on the sleeve floor and one on the bank. Slide the bank in from the open end and push it into the socket. The sleeve carries the bank's weight, so <b>the plug and the breakout's solder joints carry none</b>. That's the fix for the contact losses.</li>
<li>To charge it, pull it out by its exposed end.</li>
<li><b>Mini Button</b>: on the sleeve top with hook-and-loop, cable into the hole in T.</li>
<li>The bank's own indicator is the battery gauge (ADR-014). <b>Not sure</b> where its LEDs are: if the sleeve covers them, cut a small window over them in A or K.</li>
</ul>`,
  },
  {
    short: 'Finish + test', title: 'Finish and test', upto: 8, highlight: false, nolabels: true,
    cam: at(-80, 60, 315),
    body: `
<p><b>Finish</b></p>
<ul>
<li><b>Black out the flash opening</b>: matte black paint inside it (top bar end, lip, R, the underside of T, the window edge). Do the lens-hole edges too. White foam glows, and the flash sits right next to camera 4's lens.</li>
<li>Paper tape over the outside corners, then sand the edges lightly (400 grit).</li>
<li>Skin it with kraft paper or bookcloth and thin PVA, or paint it with gesso and then water-based acrylic. <b>No spray paint, no solvent glue, no superglue.</b> Black board (next page) skips most of this.</li>
</ul>
<p><b>Test</b></p>
<ol>
<li><b>Flash coverage</b>: the window lets the beam out to ~27° up, ~29° down and ~37° to each side. The cameras see roughly ±37° × ±29° (worked out from 88° DFoV). The top edge may come out slightly darker (<b>Not sure</b>). Shoot a white wall at 1–2 m with the flash and look at the corners.</li>
<li><b>Light leak</b>: in a dark room, fire the flash at a dark wall. A haze or ring in camera 4 means a leak.</li>
<li><b>Heat</b>: run the soak test with the door closed. Cameras are rated 0–40 °C (M5). Touch T over the slots: warm is fine, hot is not.</li>
<li><b>Power</b>: wiggle and drop tests (SOLDERING.md) with the bank in the sleeve.</li>
</ol>`,
  },
  {
    short: 'Black board', title: 'Same body in black foam board', upto: 8, highlight: false, black: true, nolabels: true,
    cam: at(-150, 85, 235, [5, 0, 0]),
    body: `
<p>Black 3 mm foam board (black core, black paper) is sold in the same sizes. You get no glowing edges, no light leaks through the walls, and no painting. Mark it with a silver pen or white pencil, or tape the paper template on and cut through it.</p>`,
  },
]
