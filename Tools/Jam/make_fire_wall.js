// make_fire_wall.js - Draws the Fire Wall's pixel-art flame columns and writes them as GBPixelVfx clips
// (Glyphbound/Content/Data/Vfx/FireWallA|B|C.json + .png), then renders previews for people to look at:
//   Saved/PixelVfx/Previews/firewall_frames.png   every frame of the three clips, enlarged
//   Saved/PixelVfx/Previews/firewall_front.gif    the wall as the player sees it (same layout as GBSpell_Wall.cpp,
//                                                 namespace GBWallFlames): rise, burn, sink
// One clip = one tall column of flame that loops: a white-hot base, yellow and orange tongues rising, dark red tips,
// a few embers above. The spell stands many of them side by side in two rows, each at its own phase.
// Same style and palette as make_flame_stream.js (the Flamethrower). Hand-made by this script, no third-party art.
// Usage: node Tools/Jam/make_fire_wall.js      (ffmpeg on PATH for the GIF)
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '..', '..');
const vfxDir = path.join(root, 'Glyphbound', 'Content', 'Data', 'Vfx');
const outDir = path.join(root, 'Glyphbound', 'Saved', 'PixelVfx', 'Previews');
fs.mkdirSync(outDir, { recursive: true });

const FW = 24, FH = 40, FRAMES = 16, COLUMNS = 4, FPS = 12;
const WORLD_H = 200, WORLD_W = WORLD_H * FW / FH;
// Hot to cold. Index 0 = transparent.
const PALETTE = [null, '#FFF6C4', '#FFDC5A', '#FF9A2A', '#D8401A', '#7A1C0C', '#33201A'];
const RGB = PALETTE.map(h => h ? [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)] : null);

function png(w, h, rgba) {
  const crcTable = [];
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crcTable[n] = c >>> 0; }
  const crc = (buf) => { let c = 0xffffffff; for (const b of buf) c = crcTable[(c ^ b) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const c = Buffer.alloc(4); c.writeUInt32BE(crc(td));
    return Buffer.concat([len, td, c]);
  };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6;
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 4 + 1)] = 0; rgba.copy(raw, y * (w * 4 + 1) + 1, y * w * 4, (y + 1) * w * 4); }
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

function hash2(x, y, seed) {
  let h = Math.imul(x, 374761393) ^ Math.imul(y, 668265263) ^ Math.imul(seed, 1274126177);
  h = Math.imul(h ^ (h >>> 13), 1274126177); h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}
const smooth = t => t * t * (3 - 2 * t);
// Value noise that repeats every `period` cells in y: scrolling it by one period is a seamless loop.
function noiseLoop(x, y, period, seed) {
  const xi = Math.floor(x), yi = Math.floor(y);
  const xf = smooth(x - xi), yf = smooth(y - yi);
  const wrap = k => ((k % period) + period) % period;
  const a = hash2(xi, wrap(yi), seed), b = hash2(xi + 1, wrap(yi), seed), c = hash2(xi, wrap(yi + 1), seed), d = hash2(xi + 1, wrap(yi + 1), seed);
  return (a * (1 - xf) + b * xf) * (1 - yf) + (c * (1 - xf) + d * xf) * yf;
}
const lerp = (a, b, t) => a + (b - a) * Math.max(0, Math.min(1, t));

// One frame as palette indices. Row 0 is the top.
function drawFrame(frame, seed) {
  const t = frame / FRAMES;
  const px = new Uint8Array(FW * FH);
  for (let y = 0; y < FH; y++) for (let x = 0; x < FW; x++) {
    const u = (x + 0.5) / FW * 2 - 1;
    const h = 1 - (y + 0.5) / FH; // 0 at the ground, 1 at the top
    // Tongues rise: the noise scrolls up one whole period per loop (two for the fine octave).
    const ny = (1 - h) * 3.0;
    const n = 0.6 * noiseLoop(u * 2.3 + 5, ny + t * 3, 3, seed) + 0.4 * noiseLoop(u * 4.1 + 9, ny * 2 + t * 6, 6, seed + 3);
    const halfWidth = lerp(1.05, 0.62, h / 0.9);
    const side = Math.min(1, (1 - Math.abs(u) / halfWidth) * 2.2);
    const turb = lerp(0.4, 1.6, h / 0.55);
    // Hot at the ground, cooling upward; the top of the frame always stays empty.
    const heat = side * (1.02 - 0.95 * Math.pow(h, 1.3)) + (n - 0.5) * turb - Math.max(0, h - 0.8) * 3 - (side < 0 ? 1 : 0);
    let c = 0;
    if (heat >= 0.92) c = 1; else if (heat >= 0.62) c = 2; else if (heat >= 0.38) c = 3; else if (heat >= 0.2) c = 4; else if (heat >= 0.08) c = 5;
    px[y * FW + x] = c;
  }
  clean(px);
  // Embers: plain 2 x 2 blocks rising above the flames, each on its own phase and lane.
  for (let e = 0; e < 3; e++) {
    const phase = (t + hash2(e, 1, seed)) % 1;
    const lane = 4 + Math.floor(hash2(e, 2, seed) * (FW - 10));
    const ex = lane + Math.round(Math.sin((phase + e) * 6.283) * 1.5);
    const ey = Math.round(lerp(FH * 0.5, 1, phase));
    const col = phase < 0.45 ? 2 : phase < 0.8 ? 3 : 4;
    let free = true;
    for (let dy = -1; dy < 3; dy++) for (let dx = -1; dx < 3; dx++) {
      const X = ex + dx, Y = ey + dy;
      if (X < 0 || Y < 0 || X >= FW || Y >= FH || px[Y * FW + X] !== 0) free = false;
    }
    if (free) for (let dy = 0; dy < 2; dy++) for (let dx = 0; dx < 2; dx++) px[(ey + dy) * FW + ex + dx] = col;
  }
  return px;
}

// Pixel-art cleanup: no stray pixels, no pin holes, no single-pixel colour specks, no thin spurs.
// The ground row counts as solid below, so the base stays a flat line.
function clean(px) {
  for (let pass = 0; pass < 8; pass++) {
    const src = px.slice();
    const sat = (x, y) => (y >= FH) ? 1 : (x < 0 || y < 0 || x >= FW) ? 0 : src[y * FW + x];
    for (let y = 0; y < FH; y++) for (let x = 0; x < FW; x++) {
      const c = sat(x, y);
      const nb = [sat(x - 1, y), sat(x + 1, y), sat(x, y - 1), sat(x, y + 1)];
      const solid = nb.filter(k => k !== 0).length;
      const same = nb.filter(k => k === c).length;
      if (c !== 0 && solid <= 1) { px[y * FW + x] = 0; continue; }
      if (c === 0 && solid >= 3) { px[y * FW + x] = majority(nb.filter(k => k !== 0)); continue; }
      if (c !== 0 && same === 0) { px[y * FW + x] = majority(nb.filter(k => k !== 0)); }
    }
  }
  const regions = (isIn) => {
    const seen = new Uint8Array(FW * FH), out = [];
    for (let i = 0; i < FW * FH; i++) {
      if (!isIn(px[i]) || seen[i]) continue;
      const stack = [i], cells = [], around = [];
      let open = false;
      seen[i] = 1;
      while (stack.length) {
        const k = stack.pop(); cells.push(k);
        const x = k % FW, y = (k / FW) | 0;
        for (const [nx, ny] of [[x - 1, y], [x + 1, y], [x, y - 1], [x, y + 1]]) {
          if (nx < 0 || ny < 0 || nx >= FW || ny >= FH) { open = true; continue; }
          const c = px[ny * FW + nx];
          if (!isIn(c)) around.push(c);
          else if (!seen[ny * FW + nx]) { seen[ny * FW + nx] = 1; stack.push(ny * FW + nx); }
        }
      }
      out.push({ cells, around, open });
    }
    return out;
  };
  // Enclosed holes under 8 pixels are filled; islands under 8 pixels are dropped.
  for (const r of regions(c => c === 0)) if (!r.open && r.cells.length < 8 && r.around.length) { const fill = majority(r.around); for (const k of r.cells) px[k] = fill; }
  for (const r of regions(c => c !== 0)) if (r.cells.length < 8) for (const k of r.cells) px[k] = 0;
}
function majority(list) {
  const count = {};
  let best = list[0], bestN = 0;
  for (const k of list) { count[k] = (count[k] || 0) + 1; if (count[k] > bestN) { bestN = count[k]; best = k; } }
  return best;
}

// ---- clips ----
const variants = [['FireWallA', 17], ['FireWallB', 59], ['FireWallC', 101]];
const clips = [];
for (const [id, seed] of variants) {
  const frames = [];
  for (let f = 0; f < FRAMES; f++) frames.push(drawFrame(f, seed));
  clips.push({ id, frames });
  const rows = Math.ceil(FRAMES / COLUMNS);
  const w = COLUMNS * FW, h = rows * FH;
  const rgba = Buffer.alloc(w * h * 4);
  frames.forEach((px, f) => {
    const ox = (f % COLUMNS) * FW, oy = Math.floor(f / COLUMNS) * FH;
    for (let y = 0; y < FH; y++) for (let x = 0; x < FW; x++) {
      const c = px[y * FW + x];
      if (!c) continue;
      const o = ((oy + y) * w + ox + x) * 4;
      rgba[o] = RGB[c][0]; rgba[o + 1] = RGB[c][1]; rgba[o + 2] = RGB[c][2]; rgba[o + 3] = 255;
    }
  });
  fs.writeFileSync(path.join(vfxDir, id + '.png'), png(w, h, rgba));
  const json = {
    Id: id, Atlas: id + '.png', FrameWidth: FW, FrameHeight: FH, Columns: COLUMNS, FrameCount: FRAMES, Fps: FPS, Loop: true,
    WorldWidthCm: WORLD_W, WorldHeightCm: WORLD_H, PivotU: 0.5, PivotV: 1, Facing: 'Upright', Glow: 1.5, Lit: false,
    Light: { Enabled: false, Color: '#F3610B', Intensity: 0, RadiusCm: 0, CastShadows: false, AnchorU: 0.5, AnchorV: 0.6, TrackMode: 'Constant', Track: [] },
    Source: { System: 'Tools/Jam/make_fire_wall.js (hand-made pixel frames, no Niagara source)', DurationSeconds: FRAMES / FPS, SimFps: FPS, EngineVersion: '', BakedUtc: '' }
  };
  fs.writeFileSync(path.join(vfxDir, id + '.json'), JSON.stringify(json, null, '\t') + '\n');
  console.log('clip ' + id);
}

// ---- preview 1: every frame, enlarged ----
{
  const S = 6, pad = 4;
  const w = FRAMES * (FW * S + pad) + pad, h = clips.length * (FH * S + pad) + pad;
  const rgba = Buffer.alloc(w * h * 4);
  for (let i = 0; i < w * h; i++) { rgba[i * 4] = 0x16; rgba[i * 4 + 1] = 0x16; rgba[i * 4 + 2] = 0x1C; rgba[i * 4 + 3] = 255; }
  clips.forEach((clip, r) => clip.frames.forEach((px, f) => {
    const ox = pad + f * (FW * S + pad), oy = pad + r * (FH * S + pad);
    for (let y = 0; y < FH * S; y++) for (let x = 0; x < FW * S; x++) {
      const c = px[((y / S) | 0) * FW + ((x / S) | 0)];
      const o = ((oy + y) * w + ox + x) * 4;
      const col = c ? RGB[c] : [0x22, 0x22, 0x2A];
      rgba[o] = col[0]; rgba[o + 1] = col[1]; rgba[o + 2] = col[2];
    }
  }));
  fs.writeFileSync(path.join(outDir, 'firewall_frames.png'), png(w, h, rgba));
  console.log('frames -> ' + path.join(outDir, 'firewall_frames.png'));
}

// ---- preview 2: the wall from the front. Same numbers as GBSpell_Wall.cpp (namespace GBWallFlames) ----
const WALL = { width: 400, height: 200, thickness: 100, duration: 6, heightShare: 1.15, spacingShare: 0.5, backScale: 0.85, rise: 0.35, sink: 0.4, minScale: 0.12 };
function wallColumns() {
  const colW = WORLD_W * WALL.height * WALL.heightShare / WORLD_H;
  const count = Math.max(2, Math.min(10, Math.round(WALL.width / (colW * WALL.spacingShare))));
  const list = [];
  for (let row = 0; row < 2; row++) {
    const n = row === 0 ? count : count - 1;
    for (let i = 0; i < n; i++) {
      const k = list.length;
      list.push({
        x: (row === 0 ? -0.25 : 0.25) * WALL.thickness, // -X = toward the caster = front row
        y: -WALL.width / 2 + (i + 0.5 + row * 0.5) * WALL.width / count,
        scale: (WALL.height * WALL.heightShare / WORLD_H) * (row === 0 ? 1 : WALL.backScale) * (0.92 + 0.16 * hash2(k, 7, 5)),
        clip: k % clips.length, offset: hash2(k, 3, 5) * FRAMES / FPS, front: row === 0
      });
    }
  }
  return list;
}
function wallScale(elapsed) {
  const rise = Math.min(1, elapsed / WALL.rise), left = WALL.duration - elapsed;
  const up = 1 - (1 - rise) * (1 - rise);
  const down = left < WALL.sink ? Math.max(0, left / WALL.sink) : 1;
  return Math.max(WALL.minScale, up * down);
}
{
  const W = 480, H = 270, S = 2, fps = 24;
  const camDist = 520, eye = 150; // the player, a little past the cast distance
  const tmp = fs.mkdtempSync(path.join(require('os').tmpdir(), 'firewall-'));
  const cols = wallColumns();
  // 1.6 s: rise and burn; then the last 0.8 s of the wall: burn and sink.
  const times = [];
  for (let n = 0; n < Math.round(1.6 * fps); n++) times.push(n / fps);
  for (let n = 0; n < Math.round(0.8 * fps); n++) times.push(WALL.duration - 0.8 + n / fps);
  times.forEach((elapsed, n) => {
    const img = Buffer.alloc(W * H * 4);
    const horizon = H / 2 + Math.round(eye * (W / 2) / 900);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const o = (y * W + x) * 4;
      let col;
      if (y > horizon) col = ((x + y * 2) >> 5) % 2 ? [0x2B, 0x21, 0x18] : [0x3D, 0x2F, 0x21];
      else { const row = y >> 4; const brick = ((x + (row % 2) * 16) >> 5); col = (y % 16 === 0 || (x + (row % 2) * 16) % 32 === 0) ? [0x41, 0x29, 0x0D] : (brick + row) % 3 ? [0x86, 0x60, 0x2F] : [0x6C, 0x4B, 0x24]; }
      img[o] = col[0]; img[o + 1] = col[1]; img[o + 2] = col[2]; img[o + 3] = 255;
    }
    const s = wallScale(elapsed);
    for (const c of cols.slice().sort((a, b) => (a.front ? 1 : 0) - (b.front ? 1 : 0))) {
      const depth = camDist + (c.front ? -1 : 1) * Math.abs(c.x);
      const f = (W / 2) / depth;
      const hpx = Math.round(WORLD_H * c.scale * s * f), wpx = Math.round(WORLD_W * c.scale * s * f);
      const baseY = Math.round(H / 2 + eye * f), cx = Math.round(W / 2 + c.y * f);
      const px = clips[c.clip].frames[Math.floor((elapsed + c.offset) * FPS) % FRAMES];
      for (let y = 0; y < hpx; y++) for (let x = 0; x < wpx; x++) {
        const X = cx - (wpx >> 1) + x, Y = baseY - hpx + y;
        if (X < 0 || Y < 0 || X >= W || Y >= H) continue;
        const col = px[Math.min(FH - 1, (y * FH / hpx) | 0) * FW + Math.min(FW - 1, (x * FW / wpx) | 0)];
        if (!col) continue;
        const o = (Y * W + X) * 4;
        img[o] = RGB[col][0]; img[o + 1] = RGB[col][1]; img[o + 2] = RGB[col][2];
      }
    }
    fs.writeFileSync(path.join(tmp, 'f' + String(n).padStart(3, '0') + '.png'), png(W, H, img));
  });
  const gif = path.join(outDir, 'firewall_front.gif');
  execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-framerate', String(fps), '-i', path.join(tmp, 'f%03d.png'), '-filter_complex',
    `scale=${W * S}:${H * S}:flags=neighbor,split[a][b];[a]palettegen=max_colors=48:reserve_transparent=0[p];[b][p]paletteuse=dither=none`, '-loop', '0', gif]);
  for (const n of [3, 20, 34, 52]) fs.copyFileSync(path.join(tmp, 'f' + String(n).padStart(3, '0') + '.png'), path.join(outDir, 'firewall_front_' + n + '.png'));
  fs.rmSync(tmp, { recursive: true, force: true });
  console.log('front -> ' + gif);
}
