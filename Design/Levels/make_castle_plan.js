// make_castle_plan.js - draws the castle's plan to scale (version 1): slice 1 over the keep as the pack's author
// assembled it. Top: plan at ground level, with the crypt (below and behind the keep) in blue. Bottom: section along
// the route, on the same X axis. The keep's own floor plan is the Editor's orthographic cut at 2.5 m, embedded.
// Everything outside the keep is a proposal; sizes come from the measured pieces (Design/Levels/kits/castle).
//   node Design/Levels/make_castle_plan.js <keep_cut.png> <out.svg>
const fs = require('fs');
const [cutPng, outSvg] = process.argv.slice(2);

const S = 12.5; // cm per pixel
const X0 = -2400, X1 = 13600, Y0 = -7900, Y1 = 5100, Z0 = -1300, Z1 = 5700;
const M = 50, TOP = 60;
const W = (X1 - X0) / S + 2 * M;
const PH = (Y1 - Y0) / S;            // plan height
const SH = (Z1 - Z0) / S;            // section height
const SEC = TOP + PH + 70;           // section top
const H = SEC + SH + 150;
const px = (x) => M + (x - X0) / S;
const py = (y) => TOP + (y - Y0) / S;
const pz = (z) => SEC + (Z1 - z) / S;
const KX = 5700, KZ = 150;           // keep's own origin in the map; its axis (its y = -1400) is the map's Y = 0
const out = [];
const f1 = (v) => v.toFixed(1);
const rect = (x0, y0, x1, y1, fill, stroke, extra = '') =>
  out.push('<rect x="' + f1(px(x0)) + '" y="' + f1(py(y0)) + '" width="' + f1((x1 - x0) / S) + '" height="' + f1((y1 - y0) / S) + '" fill="' + fill + '" stroke="' + stroke + '" ' + extra + '/>');
const srect = (x0, z0, x1, z1, fill, stroke, extra = '') =>
  out.push('<rect x="' + f1(px(x0)) + '" y="' + f1(pz(z1)) + '" width="' + f1((x1 - x0) / S) + '" height="' + f1((z1 - z0) / S) + '" fill="' + fill + '" stroke="' + stroke + '" ' + extra + '/>');
const text = (x, y, t, size = 13, fill = '#eee', anchor = 'middle', sec = false) =>
  out.push('<text x="' + f1(px(x)) + '" y="' + f1(sec ? pz(y) : py(y)) + '" fill="' + fill + '" font-size="' + size + '" text-anchor="' + anchor + '">' + t + '</text>');
const line = (a, b, c, d, stroke, w = 1, extra = '', sec = false) =>
  out.push('<line x1="' + f1(px(a)) + '" y1="' + f1(sec ? pz(b) : py(b)) + '" x2="' + f1(px(c)) + '" y2="' + f1(sec ? pz(d) : py(d)) + '" stroke="' + stroke + '" stroke-width="' + w + '" ' + extra + '/>');
const poly = (pts, sec, attrs, closed) =>
  out.push('<' + (closed ? 'polygon' : 'polyline') + ' points="' + pts.map(([x, y]) => f1(px(x)) + ',' + f1(sec ? pz(y) : py(y))).join(' ') + '" ' + attrs + '/>');
const circle = (x, y, r, attrs) => out.push('<circle cx="' + f1(px(x)) + '" cy="' + f1(py(y)) + '" r="' + r + '" ' + attrs + '/>');

const STONE = '#8a8372', OURS = '#c9a64b', CRYPT = '#5aa9e6', ROUTE = '#ff7a3d', LATER = '#9a9a9a';
const DASH = 'stroke-dasharray="6 3"';
out.push('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="' + W + '" height="' + H + '" viewBox="0 0 ' + W + ' ' + H + '" font-family="Segoe UI,Arial,sans-serif">');
out.push('<defs><marker id="a" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto"><path d="M1 1L9 5L1 9z" fill="' + ROUTE + '"/></marker></defs>');
out.push('<rect width="100%" height="100%" fill="#161616"/>');
out.push('<text x="' + M + '" y="28" fill="#fff" font-size="20">Blackwall Castle, plan version 1: slice 1 (gate to seal) over the keep as its author built it</text>');
out.push('<text x="' + M + '" y="48" fill="#aaa" font-size="13">To scale, grid 10 m. The keep\'s floor plan is the Editor\'s cut at 2.5 m. Gold: what we add. Blue: the crypt, 12 m below the keep\'s floor. Orange: the route of slice 1. Grey: later slices.</text>');

// grid
for (let x = Math.ceil(X0 / 1000) * 1000; x <= X1; x += 1000) {
  line(x, Y0, x, Y1, '#262626');
  out.push('<line x1="' + px(x) + '" y1="' + SEC + '" x2="' + px(x) + '" y2="' + (SEC + SH) + '" stroke="#262626"/>');
  out.push('<text x="' + px(x) + '" y="' + (TOP + PH + 14) + '" fill="#666" font-size="10" text-anchor="middle">' + x / 100 + '</text>');
}
for (let y = Math.ceil(Y0 / 1000) * 1000; y <= Y1; y += 1000) line(X0, y, X1, y, '#262626');
for (let z = Math.ceil(Z0 / 1000) * 1000; z <= Z1; z += 1000) {
  line(X0, z, X1, z, z === 0 ? '#444' : '#262626', 1, '', true);
  out.push('<text x="' + (M - 6) + '" y="' + (pz(z) + 4) + '" fill="#777" font-size="10" text-anchor="end">' + z / 100 + ' m</text>');
}

// the keep's own plan: the cut is 1280 x 720 px at 9.375 cm/px, centred on the keep's (1890, -1400), +X up and +Y right
const b64 = fs.readFileSync(cutPng).toString('base64');
out.push('<g transform="translate(' + px(KX + 1890) + ',' + py(0) + ') rotate(90) scale(' + 9.375 / S + ') translate(-640,-360)" style="mix-blend-mode:screen"><image width="1280" height="720" xlink:href="data:image/png;base64,' + b64 + '"/></g>');

// ---- plan: what we add ----
// curtain walls (4.3 m thick), wall walk at about +10 m
rect(-1050, -3015, 7480, -2585, '#3a3524', STONE);
rect(-1050, 2585, 7480, 3015, '#3a3524', STONE);
rect(-1050, -2585, -620, -1380, '#3a3524', STONE);
rect(-1050, 1380, -620, 2585, '#3a3524', STONE);
// gatehouse (the author's section, 27.5 x 10.5 m)
rect(-1050, -1380, 0, 1380, '#4a4330', OURS, 'stroke-width="2"');
rect(-1050, -500, 0, 500, '#161616', OURS);
text(-525, -1500, 'Gatehouse', 13, OURS);
text(-525, 20, 'gate', 11, '#bbb');
// wall towers (the author's section, 7 x 7.7 m, door at the wall walk)
for (const [x, y, n] of [[-830, -2800, ''], [-830, 2800, ''], [7480, -2800, 'wall tower'], [7480, 2800, 'wall tower']]) {
  rect(x - 350, y - 385, x + 350, y + 385, '#5a5238', OURS, 'stroke-width="2"');
  if (n) text(x - 420, y + (y < 0 ? -470 : 560), n, 12, LATER, 'end');
}
// lower ward, upper ward and the two side yards
rect(0, -2585, 2800, 2585, 'rgba(201,166,75,0.08)', 'none');
rect(2800, -2585, 5290, 2585, 'rgba(201,166,75,0.16)', 'none');
rect(5290, -2585, 7480, -1740, 'rgba(201,166,75,0.16)', 'none');
rect(5290, 1740, 7480, 2585, 'rgba(201,166,75,0.16)', 'none');
line(2800, -2585, 2800, -410, OURS, 3);
line(2800, 410, 2800, 1900, OURS, 3);
rect(2500, -410, 2800, 410, '#6b5a2a', OURS);                    // wide stair, 8.2 m, rises 2.1 m
for (let i = 1; i < 6; i++) line(2500 + i * 50, -410, 2500 + i * 50, 410, '#a89050');
rect(2500, 1900, 2800, 2400, '#4a4330', OURS, 'stroke-dasharray="4 3"'); // breach with rubble steps
text(1400, -2300, 'Lower ward  0 m', 15, OURS);
text(1400, -2080, 'the orcs\' camp: first fight', 12, '#ccc');
text(4040, -2300, 'Upper ward  +2.1 m', 15, OURS);
text(2650, -520, 'wide stair', 11, '#ccc');
text(2650, 2560, 'breach, rubble steps', 11, '#ccc');
// camp
circle(1400, 0, 14, 'fill="#c4502a" stroke="#ffb070"');
text(1400, 330, 'fire', 11, '#ffb070');
for (const [x0, y0, x1, y1] of [[900, -1300, 1500, -1180], [1800, 900, 1920, 1500], [700, 1000, 1100, 1120], [2000, -900, 2120, -400]]) rect(x0, y0, x1, y1, '#7a5c2e', OURS);
text(1200, -1380, 'barricades', 11, '#ccc');
rect(600, -500, 900, -300, '#7a5c2e', OURS);
text(750, -560, 'cart', 11, '#ccc');
// side yards
rect(5600, -2585, 6510, -1760, '#5a5238', LATER, 'stroke-dasharray="5 3"');
text(6050, -2130, 'wall stair', 11, LATER);
text(6050, -1960, 'to +10 m', 11, LATER);
rect(5700, 1850, 6900, 2500, 'none', OURS, 'stroke-dasharray="4 3"');
text(6300, 2230, 'orcs\' stores', 11, '#ccc');
// keep labels
text(6490, -80, 'Front Hall', 15, '#fff');
text(6490, 130, '+2.4 m', 12, '#ccc');
text(8500, 800, 'Main Hall', 15, '#fff');
text(8500, 1010, 'grand stair to +26 m', 12, '#ccc');
text(8990, -3000, 'King\'s Hall', 15, LATER);
text(8990, -2790, 'floor, and gallery at +11.7 m', 12, LATER);
text(8990, 3000, 'Side Hall', 15, LATER);
text(5710, -1560, 'spiral', 11, LATER);
text(5710, 1680, 'spiral', 11, LATER);
// crypt (below): stair in a rear corner of the Main Hall, two flights back, a turn, two flights, cells, the seal's hall
const cr = (x0, y0, x1, y1, label) => {
  rect(x0, y0, x1, y1, 'rgba(90,169,230,0.16)', CRYPT, DASH);
  if (label) text((x0 + x1) / 2, (y0 + y1) / 2 + 40, label, 11, CRYPT);
};
cr(9290, -1300, 10090, -500, 'stair 1');
cr(10090, -1300, 10890, -500, 'stair 2');
cr(10890, -1300, 11690, -500, 'turn');
cr(10890, -2100, 11690, -1300, 'stair 3');
cr(10890, -2900, 11690, -2100, 'stair 4');
cr(10890, -5300, 11690, -2900, 'cell corridor');
for (const y of [-3700, -4500, -5300]) cr(11690, y, 12490, y + 800, 'cell');
cr(10890, -7700, 13290, -5300, '');
text(12090, -6500, 'The seal\'s hall', 15, CRYPT);
text(12090, -6290, '-9.8 m, no fight', 12, CRYPT);
circle(12090, -7200, 12, 'fill="none" stroke="#bfe6ff" stroke-width="3"');
text(12090, -7390, 'seal', 11, '#bfe6ff');
text(10900, -1390, 'crypt stair (its spot is fixed on the model)', 11, CRYPT);
// route of slice 1
poly([[-1900, 0], [-200, 0], [1000, -500], [2300, 0], [3000, 0], [5300, 0], [7400, 0], [7850, 600], [8500, 300], [9000, -900], [9400, -900], [11290, -900], [11290, -5200], [11900, -6100], [12090, -7000]],
  false, 'fill="none" stroke="' + ROUTE + '" stroke-width="3" marker-end="url(#a)" stroke-dasharray="10 5"');
text(-1900, -120, 'start', 13, ROUTE);
// later loop
poly([[6050, -2200], [6050, -2800], [7480, -2800], [8200, -2800]], false, 'fill="none" stroke="' + LATER + '" stroke-width="2" stroke-dasharray="3 4"');
text(3200, -3120, 'wall walk +10 m (later): along the wall to the tower and into the gallery of the King\'s Hall', 11, LATER);

// ---- section along the route ----
out.push('<text x="' + M + '" y="' + (SEC - 12) + '" fill="#fff" font-size="15">Section along the route (same X axis, heights in metres above the lower ward)</text>');
const F = 'rgba(138,131,114,0.25)';
srect(X0, Z0, 2800, 0, '#221f18', 'none');                          // ground under the lower ward
srect(2800, Z0, 5290, 210, '#221f18', 'none');                      // terrace
srect(5290, Z0, X1, KZ + 90, '#221f18', 'none');                    // rock under and behind the keep
srect(-1050, 0, 0, 2380, F, OURS);
srect(-1050, 0, 0, 800, '#161616', OURS);
text(-525, 2500, 'gatehouse 24 m', 11, OURS, 'middle', true);
line(0, 1000, 5290, 1000, STONE, 2, DASH, true);
text(2600, 1060, 'wall walk +10 m (behind)', 11, STONE, 'middle', true);
poly([[2500, 0], [2800, 210], [2800, 0]], true, 'fill="#6b5a2a" stroke="' + OURS + '"', true);
text(1400, 60, 'lower ward 0', 12, OURS, 'middle', true);
text(4040, 270, 'upper ward +2.1', 12, OURS, 'middle', true);
// keep: front hall (its x -918..1990), main hall (1990..4968), top level (2085..4694)
const k = (x) => KX + x, kz = (z) => KZ + z;
srect(k(-918), kz(0), k(1990), kz(1850), F, STONE);
srect(k(-720), kz(1850), k(-20), kz(2840), F, STONE);
text(k(-370), kz(2900), 'front towers, spiral stairs', 11, LATER, 'middle', true);
for (const z of [90, 1020, 1900]) line(k(-410), kz(z), k(1990), kz(z), '#d8d2c0', 2, '', true);
text(k(790), kz(90) + 60, 'Front Hall +2.4', 12, '#fff', 'middle', true);
text(k(790), kz(1020) + 60, 'upper floor +11.7', 11, LATER, 'middle', true);
text(k(880), kz(1900) + 60, 'roof walk between the towers +20.7', 11, LATER, 'middle', true);
srect(k(1990), kz(0), k(4968), kz(2400), F, STONE);
line(k(1990), kz(90), k(4968), kz(90), '#d8d2c0', 2, '', true);
poly([[k(3600), kz(90)], [k(4390), kz(690)], [k(3300), kz(1020)], [k(2500), kz(1430)], [k(3100), kz(1930)], [k(3900), kz(2500)]], true,
  'fill="none" stroke="#d8d2c0" stroke-width="2" stroke-dasharray="5 3"', false);
text(k(3480), kz(1620), 'Main Hall: grand stair', 12, '#fff', 'middle', true);
srect(k(2085), kz(2400), k(4694), kz(5300), F, STONE);
for (const z of [2500, 3420, 4340]) line(k(2085), kz(z), k(4694), kz(z), '#d8d2c0', 2, '', true);
text(k(3390), kz(2500) + 60, 'Top level +26.5', 11, LATER, 'middle', true);
text(k(3390), kz(3420) + 60, '+35.7', 11, LATER, 'middle', true);
text(k(3390), kz(4340) + 60, '+44.9, four spiral turrets', 11, LATER, 'middle', true);
// crypt in section: two flights back (to -3.7 m), then two more to the side, drawn as the hall's box
const cz = kz(90);
poly([[9290, cz], [10090, cz - 305], [10890, cz - 610], [10890, cz - 160], [10090, cz + 145], [9290, cz + 300]], true,
  'fill="rgba(90,169,230,0.16)" stroke="' + CRYPT + '" ' + DASH, true);
srect(10890, cz - 1220, 13290, cz - 120, 'rgba(90,169,230,0.16)', CRYPT, DASH);
text(12090, cz - 560, 'cells and the seal\'s hall  -9.8 m', 12, CRYPT, 'middle', true);
text(12090, cz - 760, 'two more flights to the side; vault 11 m high', 11, CRYPT, 'middle', true);
text(8700, cz - 720, 'crypt stair: four flights of 3.05 m', 11, CRYPT, 'middle', true);

// legend
const ly = SEC + SH + 40;
const leg = [[OURS, 'what we add: wards, walls, gate, wall towers'], [STONE, 'the keep, whole, as its author assembled it'], [CRYPT, 'crypt, from the second interior set'],
  [ROUTE, 'route of slice 1: the opening'], [LATER, 'later slices']];
leg.forEach(([c, t], i) => {
  const x = M + (i % 3) * 430, y = ly + Math.floor(i / 3) * 26;
  out.push('<rect x="' + x + '" y="' + y + '" width="16" height="16" fill="' + c + '"/><text x="' + (x + 24) + '" y="' + (y + 13) + '" fill="#ddd" font-size="12">' + t + '</text>');
});
out.push('<text x="' + M + '" y="' + (ly + 78) + '" fill="#888" font-size="11">Measured: storeys every 9 m, floor tiles 4 m, spiral stair 7 m across and 4.5 m per turn, wide stair 8.2 m rising 2.1 m, curtain wall 13.6 m, crypt tiles 8 m. Proposed: everything in gold and blue.</text>');
out.push('</svg>');
fs.writeFileSync(outSvg, out.join('\n'));
console.log(outSvg + ' ' + W + 'x' + H);
