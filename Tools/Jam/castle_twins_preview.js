// castle_twins_preview.js - draws the pack's piece and its pixel twin side by side, from two corners, flat shaded, so a
// twin can be judged without the Editor.  node Tools/Jam/castle_twins_preview.js <src dir> <out dir> <png dir> name ...
// Left half: the pack's piece (grey). Right half: the twin, coloured by tile kind. Light from the top left.
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const [, , SRC, OUT, PNG, ...NAMES] = process.argv;
fs.mkdirSync(PNG, { recursive: true });
const W = 360, H = 360;
const KIND = { wall: [176, 164, 140], dressed: [214, 200, 168], dark: [110, 104, 110], wood: [140, 92, 52], iron: [92, 96, 104],
  cobble: [150, 146, 140], cloth: [200, 190, 150], earth: [120, 92, 60], roof: [150, 70, 60], src: [180, 180, 180] };

function png(w, h, rgb) {
  const t = []; for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  const crc = (b) => { let c = 0xffffffff; for (const x of b) c = t[(c ^ x) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const chunk = (ty, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(ty), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ih = Buffer.alloc(13); ih.writeUInt32BE(w, 0); ih.writeUInt32BE(h, 4); ih[8] = 8; ih[9] = 2;
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3);
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

// tris: [[p0, p1, p2, color]]
function render(tris, view, img, ox, bounds) {
  const [yawDeg, pitchDeg] = view;
  const ya = (yawDeg * Math.PI) / 180, pa = (pitchDeg * Math.PI) / 180;
  const cam = (p) => { // camera axes: right, up, forward
    const x = p[0] * Math.cos(ya) + p[1] * Math.sin(ya), y = -p[0] * Math.sin(ya) + p[1] * Math.cos(ya);
    return [y, p[2] * Math.cos(pa) - x * Math.sin(pa), x * Math.cos(pa) + p[2] * Math.sin(pa)];
  };
  const c = bounds.map((b) => cam(b));
  let x0 = Infinity, x1 = -Infinity, y0 = Infinity, y1 = -Infinity;
  for (const p of c) { x0 = Math.min(x0, p[0]); x1 = Math.max(x1, p[0]); y0 = Math.min(y0, p[1]); y1 = Math.max(y1, p[1]); }
  const s = (W - 20) / Math.max(x1 - x0, y1 - y0, 1);
  const zb = new Float32Array(W * H).fill(-Infinity);
  const light = [-0.45, 0.35, 0.82]; // top left, in world terms (x forward, y right of the viewer at yaw 0)
  for (const [a, b, cc, col] of tris) {
    const e1 = b.map((v, i) => v - a[i]), e2 = cc.map((v, i) => v - a[i]);
    let n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]];
    const ln = Math.hypot(...n) || 1; n = n.map((v) => v / ln);
    const P = [a, b, cc].map((p) => { const q = cam(p); return [10 + (q[0] - x0) * s + (W - 20 - (x1 - x0) * s) / 2, H - 10 - (q[1] - y0) * s - (H - 20 - (y1 - y0) * s) / 2, q[2]]; });
    let shade = Math.abs(n[0] * light[0] + n[1] * light[1] + n[2] * light[2]);
    shade = 0.35 + 0.65 * shade;
    const minx = Math.max(0, Math.floor(Math.min(P[0][0], P[1][0], P[2][0]))), maxx = Math.min(W - 1, Math.ceil(Math.max(P[0][0], P[1][0], P[2][0])));
    const miny = Math.max(0, Math.floor(Math.min(P[0][1], P[1][1], P[2][1]))), maxy = Math.min(H - 1, Math.ceil(Math.max(P[0][1], P[1][1], P[2][1])));
    const d = (P[1][1] - P[2][1]) * (P[0][0] - P[2][0]) + (P[2][0] - P[1][0]) * (P[0][1] - P[2][1]);
    if (Math.abs(d) < 1e-9) continue;
    for (let y = miny; y <= maxy; y++) for (let x = minx; x <= maxx; x++) {
      const w0 = ((P[1][1] - P[2][1]) * (x + 0.5 - P[2][0]) + (P[2][0] - P[1][0]) * (y + 0.5 - P[2][1])) / d;
      const w1 = ((P[2][1] - P[0][1]) * (x + 0.5 - P[2][0]) + (P[0][0] - P[2][0]) * (y + 0.5 - P[2][1])) / d;
      const w2 = 1 - w0 - w1;
      if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      const z = w0 * P[0][2] + w1 * P[1][2] + w2 * P[2][2];
      if (z <= zb[y * W + x]) continue;
      zb[y * W + x] = z;
      const o = (y * img.w + ox + x) * 3;
      for (let k = 0; k < 3; k++) img.px[o + k] = Math.min(255, col[k] * shade);
    }
  }
}

const SHEET = NAMES[0] === '--sheet' ? NAMES.shift() && NAMES.shift() : null;   // --sheet <file>: one picture, 4 pieces a row
const sheetCols = 4;
const sheet = SHEET ? { w: W * 2 * sheetCols, px: Buffer.alloc(W * 2 * sheetCols * H * Math.ceil(NAMES.length / sheetCols) * 3, 40) } : null;
let cell = 0;
for (const name of NAMES) {
  const src = JSON.parse(fs.readFileSync(path.join(SRC, name + '.json'), 'utf8'));
  const tw = JSON.parse(fs.readFileSync(path.join(OUT, name + '.json'), 'utf8'));
  const st = [];
  for (let k = 0; k < src.t.length / 3; k++) st.push([0, 1, 2].map((q) => { const i = src.t[k * 3 + q] * 3; return [src.v[i], src.v[i + 1], src.v[i + 2]]; }).concat([KIND.src]));
  const tt = [];
  for (const [kind, s] of Object.entries(tw.sections)) for (let k = 0; k < s.t.length / 3; k++) tt.push([0, 1, 2].map((q) => { const i = s.t[k * 3 + q] * 3; return [s.v[i], s.v[i + 1], s.v[i + 2]]; }).concat([KIND[kind] || [255, 0, 255]]));
  const b = src.bounds;
  const corners = []; for (const x of [b[0][0], b[1][0]]) for (const y of [b[0][1], b[1][1]]) for (const z of [b[0][2], b[1][2]]) corners.push([x, y, z]);
  if (sheet) {
    const row = Math.floor(cell / sheetCols), col = cell % sheetCols;
    const sub = { w: W * 2, px: Buffer.alloc(W * 2 * H * 3, (cell % 2) ? 46 : 36) };
    render(st, [35, 25], sub, 0, corners); render(tt, [35, 25], sub, W, corners);
    for (let y = 0; y < H; y++) sub.px.copy(sheet.px, ((row * H + y) * sheet.w + col * W * 2) * 3, y * W * 2 * 3, (y + 1) * W * 2 * 3);
    cell++;
    continue;
  }
  const img = { w: W * 4, px: Buffer.alloc(W * 4 * H * 3, 40) };
  render(st, [35, 25], img, 0, corners); render(tt, [35, 25], img, W, corners);
  render(st, [215, 25], img, 2 * W, corners); render(tt, [215, 25], img, 3 * W, corners);
  fs.writeFileSync(path.join(PNG, name + '.png'), png(W * 4, H, img.px));
}
if (sheet) fs.writeFileSync(path.join(PNG, SHEET), png(sheet.w, sheet.px.length / 3 / sheet.w, sheet.px));
