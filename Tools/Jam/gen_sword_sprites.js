// gen_sword_sprites.js - the Elemental Sword's first-person sprites (owner, 2026-11-15): one animated sprite per element
// (fire, lightning, ice), 8 frames each, ids sword_blade_<element>_<frame>. Solid hilt, guard and blade base; above the
// base the blade is ethereal: a gradient of bands that dissolves toward the tip, with energy flowing up it.
// Usage: node Tools/Jam/gen_sword_sprites.js [preview.png]   (rewrites the sword_blade_* entries of Sprites.json)
const fs = require('fs');
const zlib = require('zlib');
const path = require('path');

const SPRITES = path.join(__dirname, '..', '..', 'Glyphbound', 'Content', 'Data', 'Sprites.json');
const W = 16, H = 88, FRAMES = 8;
const CX = 7.5;
const Y_BASE_TOP = 62, Y_BASE_BOT = 69, Y_GUARD = 70, Y_GRIP = 73, Y_POMMEL = 84;

const hexRgb = h => [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16));
const hash = (a, b, c) => { let n = (a * 374761393 + b * 668265263 + c * 2147483647) | 0; n = (n ^ (n >>> 13)) * 1274126177 | 0; return ((n ^ (n >>> 16)) >>> 0) / 4294967296; };

const THEMES = {
  fire: {
    outline: '#2A0E0A', base: ['#9C2A12', '#E0561C', '#FF9A3C'], guard: ['#3A2A2E', '#6A4C52', '#A07A70'], grip: ['#241418', '#4A2A2E'], gem: ['#FF6A20', '#FFD060'],
    bands: ['#7A1608', '#D8420F', '#FF8A1E', '#FFE27A'], spark: '#FFF6C8', wisp: 1.3, flow: 14,
  },
  lightning: {
    outline: '#16102E', base: ['#4A2EA0', '#8A66FF', '#D8C8FF'], guard: ['#2A2A3E', '#52526E', '#8A8AA8'], grip: ['#181826', '#34344A'], gem: ['#FFE84A', '#FFFFFF'],
    bands: ['#3A22A0', '#7A52F0', '#C8B4FF', '#FFFBD0'], spark: '#FFFFFF', wisp: 1.0, flow: 12,
  },
  ice: {
    outline: '#0A1A30', base: ['#2A70B8', '#58B8F0', '#C0F0FF'], guard: ['#26323E', '#4E6478', '#8AA6B8'], grip: ['#14202C', '#2C4056'], gem: ['#58E8FF', '#FFFFFF'],
    bands: ['#1E5C9E', '#46A8E8', '#9CE8FF', '#F4FFFF'], spark: '#FFFFFF', wisp: 0.7, flow: 16,
  },
};

function frame(el, T, f) {
  const px = Array.from({ length: H }, () => Array(W).fill(null)); // [r,g,b,a] or null
  const set = (x, y, hex, a = 255) => { if (x >= 0 && x < W && y >= 0 && y < H) px[y][x] = [...hexRgb(hex), a]; };

  // --- ethereal blade (rows 2..Y_BASE_TOP-1): gradient bands that dissolve toward the tip.
  const top = 2;
  const len = Y_BASE_TOP - top;
  for (let y = top; y < Y_BASE_TOP; y++) {
    const t = (Y_BASE_TOP - 1 - y) / (len - 1);               // 0 at the base, 1 at the tip
    const taper = t > 0.82 ? Math.max(0.15, (1 - t) / 0.18) : 1;
    const hw = (2.1 + 1.3 * (1 - t)) * taper;                 // half width of the core
    const fade = Math.pow(1 - t, 0.75) * 0.88 + 0.12;
    for (let x = 0; x < W; x++) {
      let cxs = CX;
      if (el === 'lightning') {                               // the core zigzags
        const j = Math.round((hash(Math.floor(y / 4), f, 7) - 0.5) * 2.2 * (0.4 + t));
        cxs += j;
      }
      const d = Math.abs(x - cxs);
      const flow = 0.5 + 0.5 * Math.sin(2 * Math.PI * (y / T.flow + f / FRAMES));
      let I = 1 - d / (hw + 2.6);
      I += 0.20 * (flow - 0.5) * (0.5 + t);
      // wisps: noise that moves up the blade (pattern at row y, frame f = pattern at row y + 2 f)
      const yy = ((y + 2 * f) % 16 + 16) % 16;
      const n = hash(x, yy, el.length);
      if (d > hw * 0.9) I += (n - 0.45) * 0.42 * T.wisp * (0.6 + t * 0.5);
      if (el === 'fire' && d > hw) I += Math.sin((y + f * 2) * 0.9 + x) * 0.07 + (n > 0.8 ? 0.1 : 0);
      I *= fade;
      if (I < 0.13) continue;
      const band = I < 0.30 ? 0 : I < 0.50 ? 1 : I < 0.70 ? 2 : 3;
      const aLv = I < 0.30 ? 0.35 : I < 0.50 ? 0.62 : I < 0.70 ? 0.85 : 1;
      const a = Math.round(255 * Math.min(1, aLv * (0.55 + 0.45 * fade)) / 17) * 17;
      set(x, y, T.bands[band], a);
    }
    // a bright core line that keeps its brightness up the blade
    if (el !== 'lightning' || true) {
      const jx = el === 'lightning' ? Math.round((hash(Math.floor(y / 4), f, 7) - 0.5) * 2.2 * (0.4 + t)) : 0;
      if (t < 0.9 && Math.abs(0) === 0) {
        const cL = Math.floor(CX + jx), cR = cL + 1;
        const a = Math.round(255 * Math.min(1, 0.55 + 0.5 * fade) / 17) * 17;
        set(cL, y, T.bands[3], a); set(cR, y, T.bands[3], a);
      }
    }
  }
  // sparkles (ice twinkles, fire embers, lightning arcs): single pixels beside the blade
  for (let k = 0; k < 5; k++) {
    const y = top + 4 + Math.floor(hash(k, f, 31) * (len - 12));
    const side = hash(k, 1, 5) < 0.5 ? -1 : 1;
    const x = Math.round(CX + side * (3.6 + hash(k, f, 9) * 3));
    if (hash(k, f, 13) < (el === 'ice' ? 0.7 : 0.5)) set(x, y, T.spark, 255);
  }

  // --- solid base of the blade (outline + 3 shades), solid guard, grip and pommel
  for (let y = Y_BASE_TOP; y < Y_BASE_BOT + 1; y++) {
    const k = y - Y_BASE_TOP;
    set(4, y, T.outline); set(11, y, T.outline);
    for (let x = 5; x <= 10; x++) set(x, y, x <= 6 ? T.base[1] : x >= 9 ? T.base[0] : T.base[2]);
    if (k === 0) { for (let x = 5; x <= 10; x++) set(x, y, T.base[2]); }
    if (k >= 5 && k % 2 === 1) for (let x = 5; x <= 10; x++) set(x, y, T.base[x <= 7 ? 1 : 0]); // ridge lines
  }
  for (let x = 5; x <= 10; x++) set(x, Y_BASE_TOP - 1, T.outline, 0);
  // guard
  for (let y = Y_GUARD; y < Y_GUARD + 3; y++) {
    for (let x = 0; x < W; x++) {
      const edge = x === 0 || x === W - 1 || y === Y_GUARD + 2 || y === Y_GUARD;
      if (x >= 1 && x <= W - 2 || y === Y_GUARD + 1) set(x, y, edge ? T.outline : (y === Y_GUARD + 1 ? T.guard[2] : T.guard[1]));
    }
  }
  for (let x = 1; x <= W - 2; x++) { set(x, Y_GUARD, T.guard[2]); set(x, Y_GUARD + 1, x % 4 < 2 ? T.guard[1] : T.guard[0]); set(x, Y_GUARD + 2, T.guard[0]); }
  for (let x = 0; x < W; x++) { if (x === 0 || x === W - 1) { set(x, Y_GUARD + 1, T.outline); } }
  // the gem in the middle of the guard
  set(7, Y_GUARD + 1, T.gem[0]); set(8, Y_GUARD + 1, T.gem[0]); set(7, Y_GUARD, T.gem[1]); set(8, Y_GUARD, T.gem[1]);
  // grip with wrap bands
  for (let y = Y_GRIP; y < Y_POMMEL; y++) {
    set(5, y, T.outline); set(10, y, T.outline);
    const band = (y - Y_GRIP) % 3 === 2;
    for (let x = 6; x <= 9; x++) set(x, y, band ? T.grip[0] : (x <= 7 ? T.grip[1] : T.grip[0]));
  }
  // pommel
  for (let y = Y_POMMEL; y < H; y++) {
    const w = y === Y_POMMEL || y === H - 1 ? 3 : 4;
    for (let x = 8 - w; x < 8 + w; x++) set(x, y, (y === H - 1 || x === 8 - w || x === 7 + w) ? T.outline : (x <= 7 ? T.guard[2] : T.guard[1]));
  }
  set(7, Y_POMMEL + 1, T.gem[1]); set(8, Y_POMMEL + 1, T.gem[0]);
  return px;
}

const CHARS = '0123456789abcdefghijklmnopqrstuvwxyz';
function toEntry(id, px) {
  const pal = {}, rows = [];
  let n = 0;
  for (const row of px) {
    let r = '';
    for (const c of row) {
      if (!c || c[3] === 0) { r += '.'; continue; }
      const hex = '#' + c.map(v => v.toString(16).padStart(2, '0').toUpperCase()).join('');
      if (!(hex in pal)) { if (n >= CHARS.length) throw new Error('palette overflow in ' + id); pal[hex] = CHARS[n++]; }
      r += pal[hex];
    }
    rows.push(r);
  }
  const palette = {};
  for (const [hex, ch] of Object.entries(pal)) palette[ch] = hex.endsWith('FF') ? hex.slice(0, 7) : hex;
  return { Id: id, Palette: palette, Rows: rows };
}

const doc = JSON.parse(fs.readFileSync(SPRITES, 'utf8'));
doc.Sprites = doc.Sprites.filter(s => !/^sword_blade_/.test(s.Id));
const all = {};
for (const el of Object.keys(THEMES)) {
  all[el] = [];
  for (let f = 0; f < FRAMES; f++) {
    const px = frame(el, THEMES[el], f);
    all[el].push(px);
    doc.Sprites.push(toEntry(`sword_blade_${el}_${f}`, px));
  }
}
fs.writeFileSync(SPRITES, JSON.stringify(doc, null, '\t') + '\n');
console.log('wrote', Object.keys(THEMES).length * FRAMES, 'sprites');

// preview PNG: every frame x6 on a dark backdrop
if (process.argv[2]) {
  const S = 4, gap = 4, els = Object.keys(THEMES);
  const PW = FRAMES * (W * S + gap) + gap, PH = els.length * (H * S + gap) + gap;
  const buf = Buffer.alloc(PW * PH * 4);
  for (let i = 0; i < PW * PH; i++) { buf[i * 4] = 40; buf[i * 4 + 1] = 40; buf[i * 4 + 2] = 48; buf[i * 4 + 3] = 255; }
  els.forEach((el, ei) => all[el].forEach((px, f) => {
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const c = px[y][x]; if (!c) continue;
      for (let dy = 0; dy < S; dy++) for (let dx = 0; dx < S; dx++) {
        const X = gap + f * (W * S + gap) + x * S + dx, Y = gap + ei * (H * S + gap) + y * S + dy, o = (Y * PW + X) * 4, a = c[3] / 255;
        for (let k = 0; k < 3; k++) buf[o + k] = Math.round(buf[o + k] * (1 - a) + c[k] * a);
      }
    }
  }));
  const raw = Buffer.alloc((PW * 4 + 1) * PH);
  for (let y = 0; y < PH; y++) { raw[y * (PW * 4 + 1)] = 0; buf.copy(raw, y * (PW * 4 + 1) + 1, y * PW * 4, (y + 1) * PW * 4); }
  const crcT = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
  const crc = b => { let c = 0xFFFFFFFF; for (const v of b) c = crcT[(c ^ v) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
  const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(PW, 0); ihdr.writeUInt32BE(PH, 4); ihdr[8] = 8; ihdr[9] = 6;
  fs.writeFileSync(process.argv[2], Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
}
