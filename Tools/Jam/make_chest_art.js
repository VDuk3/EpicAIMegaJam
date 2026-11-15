// make_chest_art.js - draws the pixel chest's two pictures (32 x 32 PNG): body and lid. Owner, 2026-11-15.
// node Tools/Jam/make_chest_art.js   -> Glyphbound/SourceArt/PixelArt/Chest/T_px_chest_body.png, T_px_chest_lid.png
const fs = require('fs'), zlib = require('zlib'), path = require('path');
const N = 32, OUT = path.join(__dirname, '..', '..', 'Glyphbound', 'SourceArt', 'PixelArt', 'Chest');
const C = { o: '1E130B', w: '6B3E1E', d: '4A2912', l: '8A5428', i: '434848', j: '6E7371', k: '262827', g: 'E2B33C', h: 'F5E08A', s: '8A5A12' };
function crc(buf) { let c, r = 0xFFFFFFFF; for (const b of buf) { c = (r ^ b) & 0xFF; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; r = c ^ (r >>> 8); } return (r ^ 0xFFFFFFFF) >>> 0; }
function chunk(type, data) { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([len, td, c]); }
function save(name, px) {
  const raw = Buffer.alloc(N * (N * 3 + 1));
  for (let y = 0; y < N; y++) { raw[y * (N * 3 + 1)] = 0; for (let x = 0; x < N; x++) { const v = parseInt(C[px[y][x]], 16); const o = y * (N * 3 + 1) + 1 + x * 3; raw[o] = v >> 16; raw[o + 1] = (v >> 8) & 255; raw[o + 2] = v & 255; } }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(N, 0); ihdr.writeUInt32BE(N, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(path.join(OUT, name), Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
}
function planks(plankH) {
  const px = [];
  for (let y = 0; y < N; y++) { px.push([]); for (let x = 0; x < N; x++) { const r = y % plankH; px[y].push(r === 0 ? 'd' : r === 1 ? 'l' : 'w'); } }
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) if ((x * 7 + Math.floor(y / plankH) * 13) % 17 === 0 && y % plankH > 1) px[y][x] = 'd';   // grain
  return px;
}
function band(px, x0, x1, y0, y1) { for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) px[y][x] = (x === x0 || y === y0) ? 'j' : (x === x1 || y === y1) ? 'k' : 'i'; }
function frame(px) { for (let i = 0; i < N; i++) { px[0][i] = px[N - 1][i] = px[i][0] = px[i][N - 1] = 'o'; } }
function rivet(px, x, y) { px[y][x] = 'j'; px[y + 1][x] = 'k'; }
fs.mkdirSync(OUT, { recursive: true });
// body: planks, iron straps at both ends and along the foot
let b = planks(8);
band(b, 1, 5, 1, 30); band(b, 26, 30, 1, 30); band(b, 1, 30, 26, 30);
for (const y of [5, 14, 22]) { rivet(b, 3, y); rivet(b, 28, y); }
frame(b); save('T_px_chest_body.png', b);
// lid: planks, iron rim all round, a brass lock plate in the middle
let l = planks(8);
band(l, 1, 30, 1, 4); band(l, 1, 30, 27, 30); band(l, 1, 4, 1, 30); band(l, 27, 30, 1, 30);
for (let y = 11; y <= 20; y++) for (let x = 12; x <= 19; x++) l[y][x] = (x === 12 || y === 11) ? 'h' : (x === 19 || y === 20) ? 's' : 'g';
l[14][15] = l[14][16] = l[15][15] = l[15][16] = 'o'; l[16][15] = l[17][15] = 'o';
for (const p of [[2, 2], [29, 2], [2, 28], [29, 28]]) rivet(l, p[0], p[1]);
frame(l); save('T_px_chest_lid.png', l);
console.log('chest art written to ' + OUT);
