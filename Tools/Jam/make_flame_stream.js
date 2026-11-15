// make_flame_stream.js - Draws the Flamethrower's pixel-art flame puffs and writes them as GBPixelVfx clips
// (Glyphbound/Content/Data/Vfx/FlameStreamA|B|C.json + .png), then renders previews for people to look at:
//   Saved/PixelVfx/Previews/flamestream_frames.png   every frame of the three clips, enlarged
//   Saved/PixelVfx/Previews/flamestream_firstperson.gif   the stream as the player sees it (same emitter maths as
//                                                        AGBSpell_Beam::UpdateStream), from the staff head to the crosshair
// One clip = the life of one puff of flame: a small white-hot core, a turbulent yellow and orange ball, red rags
// with holes, dark smoke, nothing. The spell plays many of them along the cone, each at its own age.
// Hand-made by this script (no Niagara source, no third-party art). Palette: the PastCastle lava and fireball colours.
// Usage: node Tools/Jam/make_flame_stream.js      (ffmpeg on PATH for the GIF)
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '..', '..');
const vfxDir = path.join(root, 'Glyphbound', 'Content', 'Data', 'Vfx');
const outDir = path.join(root, 'Glyphbound', 'Saved', 'PixelVfx', 'Previews');
fs.mkdirSync(outDir, { recursive: true });

const SIZE = 32, FRAMES = 16, COLUMNS = 4, FPS = 24;
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

function hash3(x, y, z, seed) {
  let h = Math.imul(x, 374761393) ^ Math.imul(y, 668265263) ^ Math.imul(z, 2147483647) ^ Math.imul(seed, 1274126177);
  h = Math.imul(h ^ (h >>> 13), 1274126177); h ^= h >>> 16;
  return (h >>> 0) / 4294967296;
}
const smooth = t => t * t * (3 - 2 * t);
function noise3(x, y, z, seed) {
  const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z);
  const xf = smooth(x - xi), yf = smooth(y - yi), zf = smooth(z - zi);
  let v = 0;
  for (let dz = 0; dz < 2; dz++) for (let dy = 0; dy < 2; dy++) for (let dx = 0; dx < 2; dx++) {
    v += hash3(xi + dx, yi + dy, zi + dz, seed) * (dx ? xf : 1 - xf) * (dy ? yf : 1 - yf) * (dz ? zf : 1 - zf);
  }
  return v;
}
const lerp = (a, b, t) => a + (b - a) * Math.max(0, Math.min(1, t));

// One frame as palette indices. a = age 0..1.
function drawFrame(frame, seed) {
  const a = frame / FRAMES;
  const px = new Uint8Array(SIZE * SIZE);
  if (frame === FRAMES - 1) return px; // the last frame is empty: the puff is gone before it is reused
  const radius = lerp(0.55, 0.98, a / 0.35);
  const turb = lerp(0.35, 1.7, a / 0.55);
  // The silhouette stays big and gets ragged and holed with age; the temperature inside it falls.
  const erode = lerp(0.0, 0.5, (a - 0.2) / 0.75);
  const cool = 0.8 * Math.pow(a, 1.35);
  const z = a * 3.2;
  for (let y = 0; y < SIZE; y++) for (let x = 0; x < SIZE; x++) {
    const u = (x + 0.5) / SIZE * 2 - 1, v = (y + 0.5) / SIZE * 2 - 1;
    const d = Math.hypot(u, v);
    // Turbulence: two octaves, drifting as the puff ages.
    const n = 0.65 * noise3(u * 2.1 + 7, v * 2.1 + 3 + a * 1.2, z, seed) + 0.35 * noise3(u * 4.3 + 1, v * 4.3 + a * 2.0, z * 1.7 + 9, seed + 5);
    const edge = 1 - d / radius;
    const body = edge * 1.05 + (n - 0.5) * turb;
    let c = 0;
    // The frame border always stays empty so a puff never shows a cut edge.
    if (d <= 0.97 && body - erode >= 0.03) {
      const heat = body - cool;
      if (heat >= 0.60) c = 1; else if (heat >= 0.40) c = 2; else if (heat >= 0.20) c = 3; else if (heat >= 0.02) c = 4; else if (heat >= -0.3) c = 5; else c = 6;
      // A young puff is all fire: no dark rim, so the puffs near the staff merge into one stream.
      if (a < 0.2) c = Math.min(c, 3); else if (a < 0.4) c = Math.min(c, 4);
    }
    px[y * SIZE + x] = c;
  }
  clean(px);
  return px;
}

// Pixel-art cleanup: no stray pixels, no pin holes, no single-pixel colour specks.
function clean(px) {
  const at = (x, y) => (x < 0 || y < 0 || x >= SIZE || y >= SIZE) ? 0 : px[y * SIZE + x];
  for (let pass = 0; pass < 8; pass++) {
    const src = px.slice();
    const sat = (x, y) => (x < 0 || y < 0 || x >= SIZE || y >= SIZE) ? 0 : src[y * SIZE + x];
    for (let y = 0; y < SIZE; y++) for (let x = 0; x < SIZE; x++) {
      const c = sat(x, y);
      const nb = [sat(x - 1, y), sat(x + 1, y), sat(x, y - 1), sat(x, y + 1)];
      const solid = nb.filter(k => k !== 0).length;
      const same = nb.filter(k => k === c).length;
      if (c !== 0 && solid <= 1) { px[y * SIZE + x] = 0; continue; }
      if (c === 0 && solid >= 3) { px[y * SIZE + x] = majority(nb.filter(k => k !== 0)); continue; }
      if (c !== 0 && same === 0) { px[y * SIZE + x] = majority(nb.filter(k => k !== 0)); }
    }
  }
  // Fill enclosed holes smaller than 8 pixels with the colour around them.
  {
    const seenHole = new Uint8Array(SIZE * SIZE);
    for (let i = 0; i < SIZE * SIZE; i++) {
      if (px[i] !== 0 || seenHole[i]) continue;
      const stack = [i], hole = [], around = [];
      let open = false;
      seenHole[i] = 1;
      while (stack.length) {
        const k = stack.pop(); hole.push(k);
        const x = k % SIZE, y = (k / SIZE) | 0;
        for (const [nx, ny] of [[x - 1, y], [x + 1, y], [x, y - 1], [x, y + 1]]) {
          if (nx < 0 || ny < 0 || nx >= SIZE || ny >= SIZE) { open = true; continue; }
          const c = px[ny * SIZE + nx];
          if (c !== 0) around.push(c);
          else if (!seenHole[ny * SIZE + nx]) { seenHole[ny * SIZE + nx] = 1; stack.push(ny * SIZE + nx); }
        }
      }
      if (!open && hole.length < 8 && around.length) { const fill = majority(around); for (const k of hole) px[k] = fill; }
    }
  }
  // Drop islands smaller than 8 pixels.
  const seen = new Uint8Array(SIZE * SIZE);
  for (let i = 0; i < SIZE * SIZE; i++) {
    if (px[i] === 0 || seen[i]) continue;
    const stack = [i], island = [];
    seen[i] = 1;
    while (stack.length) {
      const k = stack.pop(); island.push(k);
      const x = k % SIZE, y = (k / SIZE) | 0;
      for (const [nx, ny] of [[x - 1, y], [x + 1, y], [x, y - 1], [x, y + 1]]) {
        if (at(nx, ny) !== 0 && !seen[ny * SIZE + nx]) { seen[ny * SIZE + nx] = 1; stack.push(ny * SIZE + nx); }
      }
    }
    if (island.length < 8) for (const k of island) px[k] = 0;
  }
}
function majority(list) {
  const count = {};
  let best = list[0], bestN = 0;
  for (const k of list) { count[k] = (count[k] || 0) + 1; if (count[k] > bestN) { bestN = count[k]; best = k; } }
  return best;
}

// ---- clips ----
const variants = [['FlameStreamA', 11], ['FlameStreamB', 47], ['FlameStreamC', 83]];
const clips = [];
for (const [id, seed] of variants) {
  const frames = [];
  for (let f = 0; f < FRAMES; f++) frames.push(drawFrame(f, seed));
  clips.push({ id, frames });
  const rows = Math.ceil(FRAMES / COLUMNS);
  const w = COLUMNS * SIZE, h = rows * SIZE;
  const rgba = Buffer.alloc(w * h * 4);
  frames.forEach((px, f) => {
    const ox = (f % COLUMNS) * SIZE, oy = Math.floor(f / COLUMNS) * SIZE;
    for (let y = 0; y < SIZE; y++) for (let x = 0; x < SIZE; x++) {
      const c = px[y * SIZE + x];
      if (!c) continue;
      const o = ((oy + y) * w + ox + x) * 4;
      rgba[o] = RGB[c][0]; rgba[o + 1] = RGB[c][1]; rgba[o + 2] = RGB[c][2]; rgba[o + 3] = 255;
    }
  });
  fs.writeFileSync(path.join(vfxDir, id + '.png'), png(w, h, rgba));
  const json = {
    Id: id, Atlas: id + '.png', FrameWidth: SIZE, FrameHeight: SIZE, Columns: COLUMNS, FrameCount: FRAMES, Fps: FPS, Loop: true,
    WorldWidthCm: 100, WorldHeightCm: 100, PivotU: 0.5, PivotV: 0.5, Facing: 'Billboard', Glow: 1.5, Lit: false,
    Light: { Enabled: false, Color: '#F3610B', Intensity: 0, RadiusCm: 0, CastShadows: false, AnchorU: 0.5, AnchorV: 0.5, TrackMode: 'Constant', Track: [] },
    Source: { System: 'Tools/Jam/make_flame_stream.js (hand-made pixel frames, no Niagara source)', DurationSeconds: FRAMES / FPS, SimFps: FPS, EngineVersion: '', BakedUtc: '' }
  };
  fs.writeFileSync(path.join(vfxDir, id + '.json'), JSON.stringify(json, null, '\t') + '\n');
  console.log('clip ' + id);
}

// ---- preview 1: every frame, enlarged ----
{
  const S = 8, pad = 4;
  const w = FRAMES * (SIZE * S + pad) + pad, h = clips.length * (SIZE * S + pad) + pad;
  const rgba = Buffer.alloc(w * h * 4);
  for (let i = 0; i < w * h; i++) { rgba[i * 4] = 0x16; rgba[i * 4 + 1] = 0x16; rgba[i * 4 + 2] = 0x1C; rgba[i * 4 + 3] = 255; }
  clips.forEach((clip, r) => clip.frames.forEach((px, f) => {
    const ox = pad + f * (SIZE * S + pad), oy = pad + r * (SIZE * S + pad);
    for (let y = 0; y < SIZE * S; y++) for (let x = 0; x < SIZE * S; x++) {
      const c = px[((y / S) | 0) * SIZE + ((x / S) | 0)];
      const o = ((oy + y) * w + ox + x) * 4;
      const col = c ? RGB[c] : [0x22, 0x22, 0x2A];
      rgba[o] = col[0]; rgba[o + 1] = col[1]; rgba[o + 2] = col[2];
    }
  }));
  fs.writeFileSync(path.join(outDir, 'flamestream_frames.png'), png(w, h, rgba));
  console.log('frames -> ' + path.join(outDir, 'flamestream_frames.png'));
}

// ---- preview 2: first person. Same numbers as GBSpell_Beam.cpp (namespace GBBeamStream) ----
const STREAM = { puffs: 16, range: 700, reach: 0.92, power: 2.0, nearSize: 9, visHalfAngleDeg: 10, jitter: 0.2 };
function puffState(i, elapsed) {
  const life = FRAMES / FPS;
  const phase = i * life / STREAM.puffs;
  const t = elapsed + phase;
  if (i > 0 && t < life) return null; // not emitted yet: the stream grows out of the staff
  const a = (t % life) / life;
  const frame = Math.floor((t % life) * FPS) % FRAMES;
  const along = STREAM.range * STREAM.reach * Math.pow(a, STREAM.power);
  const size = STREAM.nearSize + 2 * along * Math.tan(STREAM.visHalfAngleDeg * Math.PI / 180);
  const cycle = Math.floor(t / life);
  const spread = STREAM.jitter * size * Math.min(1, along / 60);
  const jy = (hash3(i, cycle, 1, 99) * 2 - 1) * spread, jz = (hash3(i, cycle, 2, 99) * 2 - 1) * spread;
  return { a, frame, along, size, jy, jz, clip: i % clips.length };
}
{
  const W = 480, H = 270, S = 2, fps = 24, seconds = 2.5;
  const muzzle = [40, 15, -12]; // aim space (X forward, Y right, Z up): about where the HUD draws the staff head
  const target = [STREAM.range, 0, 0];
  const dirLen = Math.hypot(target[0] - muzzle[0], target[1] - muzzle[1], target[2] - muzzle[2]);
  const dir = [0, 1, 2].map(k => (target[k] - muzzle[k]) / dirLen);
  const tmp = fs.mkdtempSync(path.join(require('os').tmpdir(), 'flamestream-'));
  const total = Math.round(seconds * fps);
  for (let n = 0; n < total; n++) {
    const elapsed = n / fps;
    const img = Buffer.alloc(W * H * 4);
    // Backdrop: a castle wall in the PastCastle colours (plain bricks), a floor, a crosshair.
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const o = (y * W + x) * 4;
      let col;
      if (y > H * 0.68) col = ((x + y * 2) >> 5) % 2 ? [0x2B, 0x21, 0x18] : [0x3D, 0x2F, 0x21];
      else { const row = y >> 4; const brick = ((x + (row % 2) * 16) >> 5); col = (y % 16 === 0 || (x + (row % 2) * 16) % 32 === 0) ? [0x41, 0x29, 0x0D] : (brick + row) % 3 ? [0x86, 0x60, 0x2F] : [0x6C, 0x4B, 0x24]; }
      img[o] = col[0]; img[o + 1] = col[1]; img[o + 2] = col[2]; img[o + 3] = 255;
    }
    const list = [];
    for (let i = 0; i < STREAM.puffs; i++) { const p = puffState(i, elapsed); if (p) list.push(p); }
    list.sort((p, q) => q.along - p.along);
    for (const p of list) {
      const pos = [muzzle[0] + dir[0] * p.along, muzzle[1] + dir[1] * p.along + p.jy, muzzle[2] + dir[2] * p.along + p.jz];
      const f = (W / 2) / pos[0]; // 90 degree horizontal field of view
      const cx = W / 2 + pos[1] * f, cy = H / 2 - pos[2] * f, sz = p.size * f;
      const px = clips[p.clip].frames[p.frame];
      const x0 = Math.round(cx - sz / 2), y0 = Math.round(cy - sz / 2), n2 = Math.max(1, Math.round(sz));
      for (let y = 0; y < n2; y++) for (let x = 0; x < n2; x++) {
        const X = x0 + x, Y = y0 + y;
        if (X < 0 || Y < 0 || X >= W || Y >= H) continue;
        const c = px[Math.min(SIZE - 1, (y * SIZE / n2) | 0) * SIZE + Math.min(SIZE - 1, (x * SIZE / n2) | 0)];
        if (!c) continue;
        const o = (Y * W + X) * 4;
        img[o] = RGB[c][0]; img[o + 1] = RGB[c][1]; img[o + 2] = RGB[c][2];
      }
    }
    // Staff (a plain bar up to its head) and the crosshair, drawn over the stream like the HUD does.
    const hx = Math.round(W / 2 + muzzle[1] * (W / 2) / muzzle[0]), hy = Math.round(H / 2 - muzzle[2] * (W / 2) / muzzle[0]);
    for (let y = hy; y < H; y++) for (let x = -3; x <= 3; x++) {
      const X = hx + x + Math.round((y - hy) * 0.27); if (X < 0 || X >= W) continue;
      const o = (y * W + X) * 4; img[o] = 0x5A; img[o + 1] = 0x3B; img[o + 2] = 0x18;
    }
    for (let k = -4; k <= 4; k++) { if (Math.abs(k) < 2) continue; for (const [X, Y] of [[W / 2 + k, H / 2], [W / 2, H / 2 + k]]) { const o = (Y * W + X) * 4; img[o] = img[o + 1] = img[o + 2] = 0xEE; } }
    fs.writeFileSync(path.join(tmp, 'f' + String(n).padStart(3, '0') + '.png'), png(W, H, img));
  }
  const gif = path.join(outDir, 'flamestream_firstperson.gif');
  execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-framerate', String(fps), '-i', path.join(tmp, 'f%03d.png'), '-filter_complex',
    `scale=${W * S}:${H * S}:flags=neighbor,split[a][b];[a]palettegen=max_colors=48:reserve_transparent=0[p];[b][p]paletteuse=dither=none`, '-loop', '0', gif]);
  // Stills for a quick look without a GIF player.
  for (const n of [4, 12, 30, 45]) fs.copyFileSync(path.join(tmp, 'f' + String(n).padStart(3, '0') + '.png'), path.join(outDir, 'flamestream_firstperson_' + n + '.png'));
  fs.rmSync(tmp, { recursive: true, force: true });
  console.log('first person -> ' + gif);
}
