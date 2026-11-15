// import_laptop_lightning.js - Turns the LAPTOP's purple ("arcano") lightning sprites into GBPixelVfx clips for the
// Storm staff's basic attack and the Lightning Bolt spell, then renders previews to look at.
// Source art: Incoming/LAPTOP/arte/20261115_hechizos/vfx_jugador (the team's own script-generated sprites, LAPTOP,
// 2026-11-15; not third-party). Clips written to Glyphbound/Content/Data/Vfx:
//   LightningArcH0 / H1     the two halves of the tileable ribbon (vfx_fp_relampago_haz_arcano, 256x48 -> 2 x 128x48)
//   LightningArcQ0..Q3      its four quarters (64x48), to end an arc close to its end point
//   LightningSpark          the spark at the staff's head when firing (vfx_fp_relampago_carga_arcano)
//   LightningImpact         the hit (vfx_fp_relampago_impacto_arcano); its dithered dark fill is removed so the big
//                           frames read as a ring and bolts, not a filled disc
// The arc is drawn in the game by GBLightningFx (Combat/GBProjectile.cpp) with the same tiling as renderArc below:
// half tiles one after another from the staff head, each sized to its distance from the camera so the bolt keeps the
// same thickness on screen, the last one a quarter when that fits better.
// Previews (Glyphbound/Saved/PixelVfx/Previews): lightning_clips.png, lightning_fp_staff.png, lightning_fp_spell.png
// Usage: node Tools/Jam/import_laptop_lightning.js
const fs = require('fs');
const path = require('path');
const { readPng, writePng } = require('./pnglite');

const root = path.resolve(__dirname, '..', '..');
const src = path.join(root, 'Incoming', 'LAPTOP', 'arte', '20261115_hechizos', 'vfx_jugador');
const vfxDir = path.join(root, 'Glyphbound', 'Content', 'Data', 'Vfx');
const outDir = path.join(root, 'Glyphbound', 'Saved', 'PixelVfx', 'Previews');
fs.mkdirSync(outDir, { recursive: true });

const DARK_FILL = [0x44, 0x19, 0x74];
const LIGHT_COLOR = '#B83ECC';

function sheet(name, fw, fh) {
  const im = readPng(path.join(src, name + '_sheet.png'));
  const n = im.w / fw;
  const frames = [];
  for (let k = 0; k < n; k++) {
    const f = Buffer.alloc(fw * fh * 4);
    for (let y = 0; y < fh; y++) im.rgba.copy(f, y * fw * 4, (y * im.w + k * fw) * 4, (y * im.w + (k + 1) * fw) * 4);
    frames.push(f);
  }
  return { fw, fh, frames };
}

function crop(s, x0, w) {
  return { fw: w, fh: s.fh, frames: s.frames.map(f => { const o = Buffer.alloc(w * s.fh * 4); for (let y = 0; y < s.fh; y++) f.copy(o, y * w * 4, (y * s.fw + x0) * 4, (y * s.fw + x0 + w) * 4); return o; }) };
}

// Drops the dark dither fill: a dark-fill pixel stays only when it touches a brighter colour (a halo along the bolts).
function cleanFill(s) {
  const isFill = (f, x, y) => { const o = (y * s.fw + x) * 4; return f[o + 3] && f[o] === DARK_FILL[0] && f[o + 1] === DARK_FILL[1] && f[o + 2] === DARK_FILL[2]; };
  for (const f of s.frames) {
    const kill = [];
    for (let y = 0; y < s.fh; y++) for (let x = 0; x < s.fw; x++) {
      if (!isFill(f, x, y)) continue;
      let keep = false;
      for (let dy = -1; dy <= 1 && !keep; dy++) for (let dx = -1; dx <= 1; dx++) {
        const nx = x + dx, ny = y + dy;
        if ((dx || dy) && nx >= 0 && ny >= 0 && nx < s.fw && ny < s.fh && f[(ny * s.fw + nx) * 4 + 3] && !isFill(f, nx, ny)) { keep = true; break; }
      }
      if (!keep) kill.push((y * s.fw + x) * 4);
    }
    for (const o of kill) f[o + 3] = 0, f[o] = f[o + 1] = f[o + 2] = 0;
  }
  return s;
}

function writeClip(id, s, opt) {
  const cols = Math.min(4, s.frames.length), rows = Math.ceil(s.frames.length / cols);
  const W = cols * s.fw, H = rows * s.fh, atlas = Buffer.alloc(W * H * 4);
  s.frames.forEach((f, k) => { const cx = (k % cols) * s.fw, cy = Math.floor(k / cols) * s.fh; for (let y = 0; y < s.fh; y++) f.copy(atlas, ((cy + y) * W + cx) * 4, y * s.fw * 4, (y + 1) * s.fw * 4); });
  writePng(path.join(vfxDir, id + '.png'), W, H, atlas);
  const light = opt.light || { Enabled: false, Intensity: 0, RadiusCm: 0, TrackMode: 'Constant', Track: [] };
  const json = {
    Id: id, Atlas: id + '.png', FrameWidth: s.fw, FrameHeight: s.fh, Columns: cols, FrameCount: s.frames.length, Fps: opt.fps, Loop: opt.loop,
    WorldWidthCm: s.fw, WorldHeightCm: s.fh, PivotU: opt.pivotU, PivotV: opt.pivotV, Facing: opt.facing, Glow: opt.glow, Lit: false,
    Light: { Enabled: light.Enabled, Color: LIGHT_COLOR, Intensity: light.Intensity, RadiusCm: light.RadiusCm, CastShadows: false, AnchorU: 0.5, AnchorV: 0.5, TrackMode: light.TrackMode, Track: light.Track },
    Source: { System: 'Tools/Jam/import_laptop_lightning.js (LAPTOP script-generated sprites, vfx_jugador, arcano)', DurationSeconds: s.frames.length / opt.fps, SimFps: opt.fps, EngineVersion: '', BakedUtc: '' }
  };
  fs.writeFileSync(path.join(vfxDir, id + '.json'), JSON.stringify(json, null, '\t') + '\n');
}

// ---- clips ----
const haz = sheet('vfx_fp_relampago_haz_arcano', 256, 48);
const arcOpt = { fps: 14, loop: true, pivotU: 0, pivotV: 0.5, facing: 'Fixed', glow: 2 };
const halves = [crop(haz, 0, 128), crop(haz, 128, 128)];
const quarters = [0, 1, 2, 3].map(q => crop(haz, q * 64, 64));
halves.forEach((h, i) => writeClip('LightningArcH' + i, h, arcOpt));
quarters.forEach((q, i) => writeClip('LightningArcQ' + i, q, arcOpt));
const spark = cleanFill(sheet('vfx_fp_relampago_carga_arcano', 64, 64));
writeClip('LightningSpark', spark, { fps: 20, loop: true, pivotU: 0.5, pivotV: 0.5, facing: 'Billboard', glow: 2.5,
  light: { Enabled: true, Intensity: 40, RadiusCm: 500, TrackMode: 'Baked', Track: [1, 0.6, 0.9, 0.5, 1, 0.7, 0.4, 0.8] } });
const impact = cleanFill(sheet('vfx_fp_relampago_impacto_arcano', 96, 96));
writeClip('LightningImpact', impact, { fps: 16, loop: false, pivotU: 0.5, pivotV: 0.5, facing: 'Billboard', glow: 2.5,
  light: { Enabled: true, Intensity: 120, RadiusCm: 800, TrackMode: 'Baked', Track: [1, 0.95, 0.8, 0.6, 0.45, 0.3, 0.12, 0] } });
console.log('clips written to', vfxDir);

// ---- previews ----
const PREVIEW_GLOW = 1.4;
function blank(W, H) { const c = Buffer.alloc(W * H * 4); for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const o = (y * W + x) * 4; const g = y > H / 2 ? 34 + (y - H / 2) * 30 / H : 22; c[o] = g * 0.8; c[o + 1] = g * 0.8; c[o + 2] = g; c[o + 3] = 255; } return c; }
function add(c, o, r, g, b) { c[o] = Math.min(255, c[o] + r * PREVIEW_GLOW); c[o + 1] = Math.min(255, c[o + 1] + g * PREVIEW_GLOW); c[o + 2] = Math.min(255, c[o + 2] + b * PREVIEW_GLOW); }

// Clip frames, enlarged x3 on a dark ground.
{
  const S = 3, rowsList = [['ribbon', haz], ['spark', spark], ['impact (cleaned)', impact]];
  const W = Math.max(...rowsList.map(([, s]) => s.fw * Math.min(s.frames.length, 4))) * S, rowH = rowsList.map(([, s]) => s.fh * 2 * S + 6);
  const H = rowH.reduce((a, b) => a + b, 0), c = blank(W, H);
  let oy = 0;
  rowsList.forEach(([, s], r) => {
    s.frames.forEach((f, k) => {
      const cx = (k % 4) * s.fw * S, cy = oy + Math.floor(k / 4) * s.fh * S;
      for (let y = 0; y < s.fh * S; y++) for (let x = 0; x < s.fw * S; x++) {
        const i = (Math.floor(y / S) * s.fw + Math.floor(x / S)) * 4;
        if (f[i + 3] && cx + x < W) add(c, ((cy + y) * W + cx + x) * 4, f[i], f[i + 1], f[i + 2]);
      }
    });
    oy += rowH[r];
  });
  writePng(path.join(outDir, 'lightning_clips.png'), W, H, c);
}

// First-person mock: camera at the origin looking down +X (Y right, Z up), 90 degree FOV, the staff head where the
// player's MuzzleOffset puts it. Quads are ray-cast exactly like the game draws them (Fixed facing: X along the bolt,
// normal toward the camera).
const V = { sub: (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]], add: (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]], mul: (a, s) => [a[0] * s, a[1] * s, a[2] * s],
  dot: (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2], cross: (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]],
  len: a => Math.hypot(a[0], a[1], a[2]), norm: a => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; } };
const FW = 1920, FH = 1080, FOCAL = FW / 2;
const MUZZLE = [40, 15, -12];

function drawQuad(c, P, X, Y, w, h, pivotU, pivotV, frame, fw, fh) {
  const n = V.cross(X, Y);
  const corners = [];
  for (const [u, v] of [[0, 0], [1, 0], [0, 1], [1, 1]]) {
    const q = V.add(P, V.add(V.mul(X, (u - pivotU) * w), V.mul(Y, (pivotV - v) * h)));
    if (q[0] < 1) return;
    corners.push([FW / 2 + FOCAL * q[1] / q[0], FH / 2 - FOCAL * q[2] / q[0]]);
  }
  const x0 = Math.max(0, Math.floor(Math.min(...corners.map(p => p[0])))), x1 = Math.min(FW - 1, Math.ceil(Math.max(...corners.map(p => p[0]))));
  const y0 = Math.max(0, Math.floor(Math.min(...corners.map(p => p[1])))), y1 = Math.min(FH - 1, Math.ceil(Math.max(...corners.map(p => p[1]))));
  const pn = V.dot(P, n);
  for (let py = y0; py <= y1; py++) for (let px = x0; px <= x1; px++) {
    const r = [1, (px - FW / 2) / FOCAL, -(py - FH / 2) / FOCAL];
    const rn = V.dot(r, n); if (Math.abs(rn) < 1e-6) continue;
    const t = pn / rn; if (t <= 0) continue;
    const d = V.sub(V.mul(r, t), P);
    const u = V.dot(d, X) / w + pivotU, v = pivotV - V.dot(d, Y) / h;
    if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
    const i = (Math.floor(v * fh) * fw + Math.floor(u * fw)) * 4;
    if (frame[i + 3]) add(c, (py * FW + px) * 4, frame[i], frame[i + 1], frame[i + 2]);
  }
}

// Same rule as GBLightningFx::BuildLayer in GBProjectile.cpp.
function arcTiles(start, end, cam, heightPer100, reverse) {
  const tiles = [];
  const a = reverse ? end : start, b = reverse ? start : end;
  const dir = V.norm(V.sub(b, a)), L = V.len(V.sub(b, a));
  let pos = 0, idx = 0;
  while (pos < L && tiles.length < 40) {
    const p = V.add(a, V.mul(dir, pos));
    const h = Math.max(3, heightPer100 * V.len(V.sub(p, cam)) / 100);
    const w = h * 128 / 48, rem = L - pos;
    if (rem >= w * 0.75) { tiles.push({ p, h, half: idx % 2, quarter: -1 }); pos += w; idx++; }
    else { if (rem >= w * 0.25) tiles.push({ p, h, half: idx % 2, quarter: (idx % 2) * 2 }); break; }
  }
  return { dir, tiles };
}

function drawArc(c, start, end, heightPer100, frameIndex, reverse) {
  const { dir, tiles } = arcTiles(start, end, [0, 0, 0], heightPer100, reverse);
  for (const t of tiles) {
    const toCam = V.mul(t.p, -1);
    let Z = V.norm(V.sub(toCam, V.mul(dir, V.dot(toCam, dir))));
    const Y = V.cross(Z, dir);
    const clip = t.quarter >= 0 ? quarters[t.quarter] : halves[t.half];
    drawQuad(c, t.p, dir, Y, clip.fw * t.h / 48, t.h, 0, 0.5, clip.frames[frameIndex % 8], clip.fw, clip.fh);
  }
}

function drawBillboard(c, p, size, s, k) {
  const fwd = V.norm(p), X = V.norm(V.cross([0, 0, 1], V.mul(fwd, -1))), Y = V.cross(V.mul(fwd, -1), X);
  drawQuad(c, p, X, Y, size, size, 0.5, 0.5, s.frames[k], s.fw, s.fh);
}

function crosshair(c) { for (let d = -10; d <= 10; d++) { for (const [x, y] of [[FW / 2 + d, FH / 2], [FW / 2, FH / 2 + d]]) { const o = (y * FW + x) * 4; c[o] = c[o + 1] = c[o + 2] = 230; } } }

// Same rule as GBLightningFx::ImpactWidth: at least MinCm, and growing with the distance so it reads far away.
const impactWidth = (p, minCm, per100) => Math.max(minCm, per100 * V.len(p) / 100);
// Staff: hit an enemy 12 m ahead. Spell: hit 25 m ahead, two neighbours get small arcs.
const staffHit = [1200, 0, 0], spellHit = [2500, 0, 0], neighbours = [[2600, 180, 20], [2450, -150, -30]];
for (const [name, frames] of [['staff', [0, 3]], ['spell', [1, 5]]]) {
  const strip = Buffer.alloc(FW * 2 * FH * 4);
  frames.forEach((k, col) => {
    const c = blank(FW, FH);
    if (name === 'staff') {
      drawArc(c, MUZZLE, staffHit, 8, k, false);
      drawArc(c, MUZZLE, staffHit, 8 * 0.6, k + 3, true);
      drawBillboard(c, MUZZLE, 6, spark, k);
      drawBillboard(c, staffHit, impactWidth(staffHit, 90, 8.8), impact, Math.min(7, k + 1));
    } else {
      drawArc(c, MUZZLE, spellHit, 13, k, false);
      drawArc(c, MUZZLE, spellHit, 13 * 0.6, k + 4, true);
      drawArc(c, MUZZLE, spellHit, 13 * 0.4, k + 2, false);
      drawBillboard(c, MUZZLE, 9, spark, k);
      drawBillboard(c, spellHit, impactWidth(spellHit, 160, 14.3), impact, Math.min(7, k + 1));
      for (const nb of neighbours) { drawArc(c, spellHit, nb, 6.5, k + 1, false); drawArc(c, nb, spellHit, 3.9, k + 4, false); drawBillboard(c, nb, impactWidth(nb, 60, 7.15), impact, Math.min(7, k + 2)); }
    }
    crosshair(c);
    for (let y = 0; y < FH; y++) c.copy(strip, (y * FW * 2 + col * FW) * 4, y * FW * 4, (y + 1) * FW * 4);
  });
  writePng(path.join(outDir, 'lightning_fp_' + name + '.png'), FW * 2, FH, strip);
}
console.log('previews written to', outDir);
