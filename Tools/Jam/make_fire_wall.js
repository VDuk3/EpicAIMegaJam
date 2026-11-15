// make_fire_wall.js - Draws the Fire Wall's pixel-art curtain of fire and writes it as GBPixelVfx clips
// (Glyphbound/Content/Data/Vfx/FireCurtain*.json + .png), then renders previews for people to look at:
//   Saved/PixelVfx/Previews/firewall_frames.png      frames of every clip, enlarged
//   Saved/PixelVfx/Previews/firewall_view.gif        the wall in the game's layout (GBSpell_Wall.cpp, namespace
//                                                    GBWallFlames), seen from three quarters: rise, burn, sink
//   Saved/PixelVfx/Previews/firewall_*.png           stills: front, three quarters, end-on
// The look (owner's reference, 2026-11-15): ONE sheet of fire along a line on the ground. A white-hot seam on the floor
// spitting sparks, yellow and orange tongues rising from it, and a tall billowing red body above that breaks up at
// the top. Clips:
//   FireCurtainA / B    the sheet (two layers, one behind the other, different shapes)
//   FireCurtainRib      a narrow piece of the same sheet, stood across the wall so it never vanishes seen end-on
//   FireCurtainSeam     the burning line, flat on the floor
//   FireCurtainSparks   sparks spitting from the seam (camera-facing)
// Same style and fire palette as make_flame_stream.js (the Flamethrower). Hand-made by this script, no third-party art.
// Usage: node Tools/Jam/make_fire_wall.js      (ffmpeg on PATH for the GIF)
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '..', '..');
const vfxDir = path.join(root, 'Glyphbound', 'Content', 'Data', 'Vfx');
const outDir = path.join(root, 'Glyphbound', 'Saved', 'PixelVfx', 'Previews');
fs.mkdirSync(outDir, { recursive: true });

const FRAMES = 16, COLUMNS = 4, FPS = 12;
const CM_PER_PIXEL = 4.375; // every clip has the same pixel size in the world
// Hot to cold. Index 0 = transparent.
const PALETTE = [null, '#FFF6C4', '#FFDC5A', '#FF9A2A', '#D8401A', '#B02434', '#6E1622'];
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
// Value noise that repeats every `period` cells in y: moving through one period is a seamless loop.
function noiseLoop(x, y, period, seed) {
  const xi = Math.floor(x), yi = Math.floor(y);
  const xf = smooth(x - xi), yf = smooth(y - yi);
  const wrap = k => ((k % period) + period) % period;
  const a = hash2(xi, wrap(yi), seed), b = hash2(xi + 1, wrap(yi), seed), c = hash2(xi, wrap(yi + 1), seed), d = hash2(xi + 1, wrap(yi + 1), seed);
  return (a * (1 - xf) + b * xf) * (1 - yf) + (c * (1 - xf) + d * xf) * yf;
}
const lerp = (a, b, t) => a + (b - a) * Math.max(0, Math.min(1, t));

// ---- the sheet: seam, tongues, billowing body. Row 0 is the top. ----
function drawSheet(frame, seed, FW, FH) {
  const t = frame / FRAMES;
  const px = new Uint8Array(FW * FH);
  for (let y = 0; y < FH; y++) for (let x = 0; x < FW; x++) {
    const h = 1 - (y + 0.5) / FH;       // 0 at the floor, 1 at the top
    const ux = x / 8;                   // noise cells across
    const k = (1 - h) * (FH / 64);      // noise rows are sized for a 64 pixel tall frame
    // The ends slope down to the floor.
    const taper = Math.min(1, (Math.min(x, FW - 1 - x) + 1.5) / 7);
    // The top edge swells and drops along the wall and in time (its noise runs along time, so it loops).
    const top = (0.6 + 0.36 * noiseLoop(ux * 0.55 + 3, t * 2, 2, seed + 1)) * taper;
    // Billows: big soft blobs rising through the body; they tear the top edge and leave gaps high up.
    const billow = noiseLoop(ux * 0.9 + 11, k * 5 + t * 5, 5, seed + 2);
    const body = (top - h) * 4 + (billow - 0.5) * 1.5;
    let c = 0;
    if (body > 0 && h < 0.97) {
      // Tongues: narrow streaks that rise fast; the temperature falls with height.
      const streak = 0.68 * noiseLoop(ux * 1.5 + 5, k * 3 + t * 3, 3, seed) + 0.32 * noiseLoop(ux * 2.7 + 9, k * 5 + t * 5, 5, seed + 3);
      const temp = 0.98 - h * 1.45 + (streak - 0.5) * lerp(0.5, 1.0, h / 0.3);
      if (temp >= 0.9) c = 1; else if (temp >= 0.68) c = 2; else if (temp >= 0.44) c = 3; else if (temp >= 0.2) c = 4; else if (temp >= -0.12) c = 5; else c = 6;
      // The thin edge of a billow is always the coldest red: the body reads as one soft shape.
      if (body < 0.22 && c < 5) c = 5;
    }
    px[y * FW + x] = c;
  }
  clean(px, FW, FH, true);
  // The seam: a white-hot line on the floor, a yellow line over it, inside the tapered ends.
  for (let x = 2; x < FW - 2; x++) {
    px[(FH - 1) * FW + x] = 1; px[(FH - 2) * FW + x] = 1;
    if (px[(FH - 3) * FW + x] !== 1) px[(FH - 3) * FW + x] = 2;
  }
  return px;
}

// ---- the seam on the floor: seen from above ----
function drawSeam(frame, seed, FW, FH) {
  const t = frame / FRAMES;
  const px = new Uint8Array(FW * FH);
  for (let x = 2; x < FW - 2; x++) {
    const flick = noiseLoop(x / 3 + 2, t * 4, 4, seed);
    const half = flick > 0.62 ? 3 : flick > 0.3 ? 2 : 1; // half-width of the line in pixels
    for (let y = 0; y < FH; y++) {
      const d = Math.abs(y + 0.5 - FH / 2);
      if (d < 1) px[y * FW + x] = 1; else if (d < half) px[y * FW + x] = d < 2 ? 2 : 3;
    }
  }
  return px;
}

// ---- sparks: plain blocks thrown up and out of the seam ----
function drawSparks(frame, seed, FW, FH) {
  const px = new Uint8Array(FW * FH);
  for (let s = 0; s < 6; s++) {
    const life = ((frame / FRAMES) + hash2(s, 1, seed)) % 1;
    const dir = hash2(s, 2, seed) * 2 - 1, power = 0.6 + 0.4 * hash2(s, 3, seed);
    const x = Math.round(FW / 2 + dir * life * FW * 0.42);
    const up = power * (life * 2.2 - life * life * 2.0); // up, then falling a little
    const y = Math.round(FH - 2 - up * FH * 1.35);
    const big = hash2(s, 4, seed) > 0.25 && life < 0.7;
    const c = life < 0.3 ? 1 : life < 0.6 ? 2 : life < 0.85 ? 3 : 4;
    for (let dy = 0; dy < (big ? 2 : 1); dy++) for (let dx = 0; dx < (big ? 2 : 1); dx++) {
      const X = x + dx, Y = y - dy;
      if (X >= 0 && Y >= 0 && X < FW && Y < FH) px[Y * FW + X] = c;
    }
  }
  return px;
}

// Pixel-art cleanup: no stray pixels, no pin holes, no single-pixel colour specks, no thin spurs.
// With bGround the floor counts as solid below, so the base stays a flat line.
function clean(px, FW, FH, bGround) {
  for (let pass = 0; pass < 8; pass++) {
    const src = px.slice();
    const sat = (x, y) => (y >= FH) ? (bGround ? 1 : 0) : (x < 0 || y < 0 || x >= FW) ? 0 : src[y * FW + x];
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
  // Enclosed holes under 10 pixels are filled; islands under 10 pixels are dropped.
  for (const r of regions(c => c === 0)) if (!r.open && r.cells.length < 10 && r.around.length) { const fill = majority(r.around); for (const k of r.cells) px[k] = fill; }
  for (const r of regions(c => c !== 0)) if (r.cells.length < 10) for (const k of r.cells) px[k] = 0;
}
function majority(list) {
  const count = {};
  let best = list[0], bestN = 0;
  for (const k of list) { count[k] = (count[k] || 0) + 1; if (count[k] > bestN) { bestN = count[k]; best = k; } }
  return best;
}

// ---- clips ----
const defs = [
  { id: 'FireCurtainA', fw: 96, fh: 64, draw: drawSheet, seed: 17, facing: 'Fixed', pivotV: 1 },
  { id: 'FireCurtainB', fw: 96, fh: 64, draw: drawSheet, seed: 59, facing: 'Fixed', pivotV: 1 },
  { id: 'FireCurtainRib', fw: 20, fh: 64, draw: drawSheet, seed: 101, facing: 'Fixed', pivotV: 1 },
  { id: 'FireCurtainSeam', fw: 96, fh: 8, draw: drawSeam, seed: 7, facing: 'Ground', pivotV: 0.5 },
  { id: 'FireCurtainSparks', fw: 24, fh: 24, draw: drawSparks, seed: 23, facing: 'Upright', pivotV: 1 }
];
const clips = {};
for (const def of defs) {
  const frames = [];
  for (let f = 0; f < FRAMES; f++) frames.push(def.draw(f, def.seed, def.fw, def.fh));
  clips[def.id] = { ...def, frames };
  const rows = Math.ceil(FRAMES / COLUMNS);
  const w = COLUMNS * def.fw, h = rows * def.fh;
  const rgba = Buffer.alloc(w * h * 4);
  frames.forEach((px, f) => {
    const ox = (f % COLUMNS) * def.fw, oy = Math.floor(f / COLUMNS) * def.fh;
    for (let y = 0; y < def.fh; y++) for (let x = 0; x < def.fw; x++) {
      const c = px[y * def.fw + x];
      if (!c) continue;
      const o = ((oy + y) * w + ox + x) * 4;
      rgba[o] = RGB[c][0]; rgba[o + 1] = RGB[c][1]; rgba[o + 2] = RGB[c][2]; rgba[o + 3] = 255;
    }
  });
  fs.writeFileSync(path.join(vfxDir, def.id + '.png'), png(w, h, rgba));
  const json = {
    Id: def.id, Atlas: def.id + '.png', FrameWidth: def.fw, FrameHeight: def.fh, Columns: COLUMNS, FrameCount: FRAMES, Fps: FPS, Loop: true,
    WorldWidthCm: def.fw * CM_PER_PIXEL, WorldHeightCm: def.fh * CM_PER_PIXEL, PivotU: 0.5, PivotV: def.pivotV, Facing: def.facing, Glow: 1.5, Lit: false,
    Light: { Enabled: false, Color: '#F3610B', Intensity: 0, RadiusCm: 0, CastShadows: false, AnchorU: 0.5, AnchorV: 0.6, TrackMode: 'Constant', Track: [] },
    Source: { System: 'Tools/Jam/make_fire_wall.js (hand-made pixel frames, no Niagara source)', DurationSeconds: FRAMES / FPS, SimFps: FPS, EngineVersion: '', BakedUtc: '' }
  };
  fs.writeFileSync(path.join(vfxDir, def.id + '.json'), JSON.stringify(json, null, '\t') + '\n');
  console.log('clip ' + def.id);
}

// ---- preview 1: frames, enlarged (every second frame of each clip) ----
{
  const S = 3, pad = 4, shown = [0, 2, 4, 6, 8, 10, 12, 14];
  const rowsH = defs.map(d => d.fh * S + pad);
  const w = Math.max(...defs.map(d => shown.length * (d.fw * S + pad) + pad)), h = rowsH.reduce((a, b) => a + b, pad);
  const rgba = Buffer.alloc(w * h * 4);
  for (let i = 0; i < w * h; i++) { rgba[i * 4] = 0x16; rgba[i * 4 + 1] = 0x16; rgba[i * 4 + 2] = 0x1C; rgba[i * 4 + 3] = 255; }
  let oy = pad;
  defs.forEach((d, r) => {
    shown.forEach((f, i) => {
      const px = clips[d.id].frames[f], ox = pad + i * (d.fw * S + pad);
      for (let y = 0; y < d.fh * S; y++) for (let x = 0; x < d.fw * S; x++) {
        const c = px[((y / S) | 0) * d.fw + ((x / S) | 0)];
        const o = ((oy + y) * w + ox + x) * 4;
        const col = c ? RGB[c] : [0x22, 0x22, 0x2A];
        rgba[o] = col[0]; rgba[o + 1] = col[1]; rgba[o + 2] = col[2];
      }
    });
    oy += rowsH[r];
  });
  fs.writeFileSync(path.join(outDir, 'firewall_frames.png'), png(w, h, rgba));
  console.log('frames -> ' + path.join(outDir, 'firewall_frames.png'));
}

// ---- preview 2: the wall in the world. Same layout as GBSpell_Wall.cpp (namespace GBWallFlames) ----
// Wall space: X = away from the caster, Y = along the wall, Z = up, origin on the floor under the wall's middle.
const WALL = { width: 400, thickness: 100, duration: 6, sheetOffset: 0.18, ribs: 3, sparks: 8, rise: 0.4, sink: 0.45 };
function wallPieces(elapsed) {
  const scale = WALL.width / 400;
  const sheetH = clips.FireCurtainA.fh * CM_PER_PIXEL * scale;
  const rise = Math.min(1, elapsed / WALL.rise), left = WALL.duration - elapsed;
  const shown = (1 - (1 - rise) * (1 - rise)) * (left < WALL.sink ? Math.max(0, left / WALL.sink) : 1);
  const sunk = -(1 - shown) * sheetH; // the fire rises out of the floor and sinks back into it
  const frameOf = (offset) => Math.floor((elapsed + offset) * FPS) % FRAMES;
  const list = [];
  const add = (clip, origin, right, up, offset, floorClip) => list.push({ clip: clips[clip], origin, right, up, scale, frame: frameOf(offset), floorClip });
  add('FireCurtainA', [-WALL.sheetOffset * WALL.thickness, 0, sunk], [0, 1, 0], [0, 0, 1], 0, true);
  add('FireCurtainB', [WALL.sheetOffset * WALL.thickness, 0, sunk], [0, -1, 0], [0, 0, 1], 0.55, true);
  for (let i = 0; i < WALL.ribs; i++) {
    const y = -WALL.width / 2 + (i + 0.5) * WALL.width / WALL.ribs;
    add('FireCurtainRib', [0, y, sunk], [1, 0, 0], [0, 0, 1], 0.3 + i * 0.37, true);
  }
  if (shown > 0.05) {
    add('FireCurtainSeam', [0, 0, 2], [0, 1, 0], [1, 0, 0], 0.2, false);
    for (let i = 0; i < WALL.sparks; i++) {
      const y = -WALL.width / 2 + (i + 0.5) * WALL.width / WALL.sparks;
      list.push({ clip: clips.FireCurtainSparks, origin: [(i % 2 ? 1 : -1) * (WALL.sheetOffset * WALL.thickness + 12), y, 0], billboard: true, up: [0, 0, 1], scale, frame: frameOf(hash2(i, 5, 3) * FRAMES / FPS), floorClip: false });
    }
  }
  return list;
}
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = a => { const l = Math.hypot(a[0], a[1], a[2]); return [a[0] / l, a[1] / l, a[2] / l]; };
function renderView(elapsed, cam, lookAt, W, H) {
  const fwd = norm(sub(lookAt, cam)), right = norm(cross([0, 0, 1], fwd)), up = cross(fwd, right);
  const pieces = wallPieces(elapsed);
  for (const p of pieces) {
    if (p.billboard) { const toCam = sub(cam, p.origin); toCam[2] = 0; p.right = norm(cross([0, 0, 1], norm(toCam))); }
    p.normal = cross(p.right, p.up);
    p.w = p.clip.fw * CM_PER_PIXEL * p.scale; p.h = p.clip.fh * CM_PER_PIXEL * p.scale;
  }
  const img = Buffer.alloc(W * H * 4);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const d = norm([0, 1, 2].map(k => fwd[k] + right[k] * (x + 0.5 - W / 2) / (W / 2) - up[k] * (y + 0.5 - H / 2) / (W / 2)));
    // Backdrop: a floor of plain slabs and a far brick wall, in the castle's colours.
    let col, best = 1e9;
    if (d[2] < -0.001) {
      const tt = -cam[2] / d[2]; best = tt;
      const fx = cam[0] + d[0] * tt, fy = cam[1] + d[1] * tt;
      col = (Math.floor(fx / 100) + Math.floor(fy / 100)) & 1 ? [0x3D, 0x2F, 0x21] : [0x33, 0x27, 0x1B];
      if (tt > 2500) col = [0x2B, 0x21, 0x18];
    } else col = (y >> 4) % 2 ? [0x5A, 0x3B, 0x18] : [0x54, 0x36, 0x16];
    for (const p of pieces) {
      const denom = dot(d, p.normal);
      if (Math.abs(denom) < 1e-6) continue;
      const tt = dot(sub(p.origin, cam), p.normal) / denom;
      if (tt <= 0 || tt >= best) continue;
      const hit = [cam[0] + d[0] * tt, cam[1] + d[1] * tt, cam[2] + d[2] * tt];
      if (p.floorClip && hit[2] < 0) continue; // under the floor
      const rel = sub(hit, p.origin);
      const u = dot(rel, p.right) / p.w + 0.5, v = p.clip.pivotV === 1 ? 1 - dot(rel, p.up) / p.h : 0.5 - dot(rel, p.up) / p.h;
      if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
      const c = p.clip.frames[p.frame][Math.floor(v * p.clip.fh) * p.clip.fw + Math.floor(u * p.clip.fw)];
      if (!c) continue;
      best = tt; col = RGB[c];
    }
    const o = (y * W + x) * 4;
    img[o] = col[0]; img[o + 1] = col[1]; img[o + 2] = col[2]; img[o + 3] = 255;
  }
  return png(W, H, img);
}
{
  const W = 480, H = 270, S = 2, fps = 24;
  const look = [0, 0, 110];
  const views = { front: [-560, 0, 150], threequarter: [-430, -420, 170], endon: [0, -620, 150], above: [-330, -150, 420] };
  for (const [name, cam] of Object.entries(views)) fs.writeFileSync(path.join(outDir, 'firewall_' + name + '.png'), renderView(2.0, cam, look, W, H));
  const tmp = fs.mkdtempSync(path.join(require('os').tmpdir(), 'firewall-'));
  // 1.8 s: rise and burn; then the last 0.9 s of the wall: burn and sink.
  const times = [];
  for (let n = 0; n < Math.round(1.8 * fps); n++) times.push(n / fps);
  for (let n = 0; n < Math.round(0.9 * fps); n++) times.push(WALL.duration - 0.9 + n / fps);
  times.forEach((elapsed, n) => fs.writeFileSync(path.join(tmp, 'f' + String(n).padStart(3, '0') + '.png'), renderView(elapsed, views.threequarter, look, W, H)));
  const gif = path.join(outDir, 'firewall_view.gif');
  execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-framerate', String(fps), '-i', path.join(tmp, 'f%03d.png'), '-filter_complex',
    `scale=${W * S}:${H * S}:flags=neighbor,split[a][b];[a]palettegen=max_colors=48:reserve_transparent=0[p];[b][p]paletteuse=dither=none`, '-loop', '0', gif]);
  fs.copyFileSync(path.join(tmp, 'f005.png'), path.join(outDir, 'firewall_rise.png'));
  fs.copyFileSync(path.join(tmp, 'f' + String(times.length - 6).padStart(3, '0') + '.png'), path.join(outDir, 'firewall_sink.png'));
  fs.rmSync(tmp, { recursive: true, force: true });
  console.log('view -> ' + gif);
}
