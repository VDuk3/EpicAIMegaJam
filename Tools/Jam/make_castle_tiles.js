// make_castle_tiles.js - draws the castle's pixel-art surface tiles (32 x 32 px, seamless) and a preview sheet.
// They are our own art, made for the jam: drawn by rule from a seed, with the ramps of the visual style guide
// (Design/VisualStyleGuide.md: few ramps, shadows lean cold and lights lean warm, light from the top left, loose-pixel
// noise only on rough materials, no anti-aliasing). The castle's stone has no ramp in the guide; the one here is new.
//   node Tools/Jam/make_castle_tiles.js <out dir>
// Writes <name>.png for each tile and _preview.png (each tile 3 x 3 times, 4 screen pixels per pixel).
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const out = process.argv[2];
fs.mkdirSync(out, { recursive: true });
const N = 32;

function rng(seed) { let s = seed >>> 0; return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296; }; }
const hex = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];

function png(w, h, rgb) {
  const crcTable = [];
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crcTable[n] = c >>> 0; }
  const crc = (buf) => { let c = 0xffffffff; for (const b of buf) c = crcTable[(c ^ b) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const c = Buffer.alloc(4); c.writeUInt32BE(crc(td));
    return Buffer.concat([len, td, c]);
  };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; for (let x = 0; x < w * 3; x++) raw[y * (w * 3 + 1) + 1 + x] = rgb[y * w * 3 + x]; }
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

// A tile is a grid of indices into its ramp (0 = darkest).
function grid(fill) { return Array.from({ length: N }, () => Array(N).fill(fill)); }
const wrap = (v) => ((v % N) + N) % N;

// Coursed stone: `courses` rows of blocks `bw` wide, every other row shifted by half a block. Each block has its own
// tone, a light edge on top and left, a shadow edge on bottom and right, and a few loose pixels of wear.
function coursed(seed, courses, bw, tones, mortar) {
  const r = rng(seed), g = grid(mortar), ch = N / courses;
  for (let c = 0; c < courses; c++) {
    const shift = c % 2 ? bw / 2 : 0;
    for (let b = 0; b < N / bw; b++) {
      const tone = tones[Math.floor(r() * tones.length)];
      const x0 = b * bw + shift, y0 = c * ch;
      for (let y = 1; y < ch; y++) for (let x = 1; x < bw; x++) {
        let t = tone;
        if (y === 1 || x === 1) t = Math.min(tone + 1, tones[tones.length - 1] + 1);
        if (y === ch - 1 || x === bw - 1) t = Math.max(tone - 1, mortar + 1);
        if (y === 1 && x === bw - 1) t = tone;
        g[wrap(y0 + y)][wrap(x0 + x)] = t;
      }
      for (let k = 0; k < 3; k++) {       // wear
        const x = 2 + Math.floor(r() * (bw - 4)), y = 2 + Math.floor(r() * (ch - 4));
        g[wrap(y0 + y)][wrap(x0 + x)] = tone + (r() < 0.5 ? -1 : 0);
      }
      if (r() < 0.3) {                    // a chipped corner
        g[wrap(y0 + ch - 1)][wrap(x0 + bw - 1)] = mortar;
        g[wrap(y0 + ch - 2)][wrap(x0 + bw - 1)] = mortar + 1;
      }
    }
  }
  return g;
}

// Cobbles: round stones on a jittered grid that wraps; the gaps are dark, each stone is lit from the top left.
function cobbles(seed, cells, tones, gap) {
  const r = rng(seed), g = grid(gap), size = N / cells, seeds = [];
  for (let j = 0; j < cells; j++) for (let i = 0; i < cells; i++) {
    seeds.push({ x: (i + 0.5 + (r() - 0.5) * 0.55 + (j % 2 ? 0.5 : 0)) * size, y: (j + 0.5 + (r() - 0.5) * 0.4) * size, tone: tones[Math.floor(r() * tones.length)] });
  }
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
    let best = 1e9, second = 1e9, s = null, bx = 0, by = 0;
    for (const c of seeds) {
      let dx = Math.abs(x + 0.5 - c.x), dy = Math.abs(y + 0.5 - c.y);
      const sx = dx > N / 2 ? -1 : 1, sy = dy > N / 2 ? -1 : 1;
      dx = Math.min(dx, N - dx); dy = Math.min(dy, N - dy);
      const d = Math.hypot(dx, dy * 1.15);
      if (d < best) { second = best; best = d; s = c; bx = (x + 0.5 - c.x) * sx; by = (y + 0.5 - c.y) * sy; } else if (d < second) second = d;
    }
    if (second - best < 1.3) continue;                      // the gap between two stones
    let t = s.tone;
    if (bx + by < -size * 0.28) t += 1;                     // lit side
    if (bx + by > size * 0.34) t -= 1;                      // shadow side
    g[y][x] = Math.max(gap + 1, t);
  }
  return g;
}

// Planks: boards `bw` wide running down the tile, a dark joint between them, grain as short darker strokes, a nail.
function planks(seed, bw, tones, joint) {
  const r = rng(seed), g = grid(joint);
  for (let b = 0; b < N / bw; b++) {
    const tone = tones[Math.floor(r() * tones.length)];
    for (let y = 0; y < N; y++) for (let x = 1; x < bw; x++) {
      let t = tone;
      if (x === 1) t = tone + 1;
      if (x === bw - 1) t = tone - 1;
      g[y][b * bw + x] = Math.max(joint + 1, t);
    }
    for (let k = 0; k < 4; k++) {                           // grain
      const x = 2 + Math.floor(r() * (bw - 4)), y0 = Math.floor(r() * N), len = 3 + Math.floor(r() * 6);
      for (let y = 0; y < len; y++) g[wrap(y0 + y)][b * bw + x] = Math.max(joint + 1, tone - 1);
    }
    const ny = Math.floor(r() * 4) * 8 + 4;                 // one board end with a nail
    for (let x = 1; x < bw; x++) g[ny][b * bw + x] = joint;
    g[wrap(ny + 2)][b * bw + Math.floor(bw / 2)] = joint;
  }
  return g;
}

// Plain rough surface: three tones scattered, for earth and for cloth (with a weave).
function rough(seed, base, weave) {
  const r = rng(seed), g = grid(base);
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
    const v = r();
    g[y][x] = weave ? base + ((x + y) % 4 === 0 ? -1 : (x - y + N) % 4 === 0 ? 1 : 0) : base + (v < 0.14 ? -1 : v > 0.88 ? 1 : 0);
  }
  if (!weave) for (let k = 0; k < 10; k++) {                // small stones in the earth
    const x = Math.floor(r() * N), y = Math.floor(r() * N);
    g[y][x] = base + 2; g[y][wrap(x + 1)] = base + 1; g[wrap(y + 1)][x] = base - 1;
  }
  return g;
}

// Iron plate: bands with a rivet row.
function iron(seed, base) {
  const r = rng(seed), g = grid(base);
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
    let t = base + (r() < 0.1 ? -1 : 0);
    if (y % 16 === 0) t = base - 2;
    if (y % 16 === 1) t = base + 1;
    if (y % 16 === 15) t = base - 1;
    g[y][x] = t;
  }
  for (let y = 8; y < N; y += 16) for (let x = 4; x < N; x += 8) { g[y][x] = base + 2; g[y + 1][x] = base - 2; g[y][x + 1] = base - 1; }
  return g;
}

// Ramps, darkest first. Stone is new (the guide has none): cold in the joints, warm where the light falls.
const STONE = ['#17161D', '#2A2831', '#3F3D45', '#57544F', '#726D5E', '#8E8771', '#AAA184', '#C2B998'].map(hex);
const WOOD = ['#1C1108', '#2E1C0B', '#41290D', '#5A3B18', '#6C4B24', '#86602F', '#9C7640'].map(hex);    // the guide's wood, with two darker tones for joints
const IRON = ['#141615', '#262827', '#373837', '#434848', '#555957', '#758685', '#9DAAA9'].map(hex);      // the guide's iron
const EARTH = ['#1E1710', '#2B2118', '#3D2F21', '#54432F', '#6B5840', '#806B4E'].map(hex);
const CLOTH = ['#4D4B3D', '#6E6B57', '#85836A', '#A19F80', '#BDB995'].map(hex);                             // the guide's bone, greyed

const TILES = {
  castle_wall: [coursed(11, 4, 16, [3, 4, 4, 5], 0), STONE],        // the walls: blocks 16 x 8 px
  castle_dressed: [coursed(23, 2, 16, [5, 5, 6], 1), STONE],        // dressed stone: copings, steps, frames (16 x 16 px)
  castle_dark: [coursed(37, 4, 8, [2, 3, 3], 0), STONE],            // small dark stone: towers' trim, the retaining wall's foot
  castle_cobble: [cobbles(5, 5, [3, 3, 4, 4], 0), STONE],
  castle_wood: [planks(7, 8, [3, 4, 4, 5], 0), WOOD],
  castle_iron: [iron(3, 3), IRON],
  castle_earth: [rough(13, 3, false), EARTH],
  castle_cloth: [rough(17, 2, true), CLOTH],
};

const names = Object.keys(TILES);
for (const name of names) {
  const [g, ramp] = TILES[name];
  const rgb = Buffer.alloc(N * N * 3);
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
    const c = ramp[Math.max(0, Math.min(ramp.length - 1, g[y][x]))];
    rgb.set(c, (y * N + x) * 3);
  }
  TILES[name].push(rgb);
  fs.writeFileSync(path.join(out, name + '.png'), png(N, N, rgb));
}
// preview: each tile 3 x 3 times at 4x
const S = 4, R = 3, cell = N * R * S + 8, W = cell * 4, H = cell * Math.ceil(names.length / 4);
const pv = Buffer.alloc(W * H * 3, 24);
names.forEach((name, k) => {
  const rgb = TILES[name][2], ox = (k % 4) * cell + 4, oy = Math.floor(k / 4) * cell + 4;
  for (let y = 0; y < N * R * S; y++) for (let x = 0; x < N * R * S; x++) {
    const sx = Math.floor(x / S) % N, sy = Math.floor(y / S) % N;
    pv.set(rgb.subarray((sy * N + sx) * 3, (sy * N + sx) * 3 + 3), ((oy + y) * W + ox + x) * 3);
  }
});
fs.writeFileSync(path.join(out, '_preview.png'), png(W, H, pv));
console.log(names.length + ' tiles -> ' + out + ' (' + names.join(', ') + ')');
