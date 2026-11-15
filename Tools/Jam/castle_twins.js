// castle_twins.js - makes the pixel-art twin of each castle-pack mesh: the same piece, same pivot and size, rebuilt on a
// grid of texels (default 6.25 cm, 16 to the metre) out of flat faces that carry drawn tiles. The pack's sculpted surface
// lumps go; its silhouette (teeth, arches, window and door openings, steps, vaults) stays, stepped on the grid.
//   node Tools/Jam/castle_twins.js <src dir> <out dir> [name ...]
// <src dir> holds what castle_twins_export.py wrote from the Editor; <out dir> gets one <name>.json per twin, read by
// castle_twins_import.py. Settings per mesh: Tools/Jam/castle_twins_cfg.json.
//
// How a twin is made:
//  1. Solid or empty, cell by cell. Rays along X, Y and Z through every cell's middle; along each ray the hits are taken
//     in pairs (in, out). A cell is solid when two of the three axes say so: one axis alone is often wrong on a mesh
//     that is not closed. A plate thinner than a cell still keeps one cell.
//  2. Surface lumps go. Along each axis, the places where solid starts or stops are counted by depth; the depths where
//     many columns start or stop are the piece's real faces (the front of a wall, its back, a floor's top). A start or
//     stop within `snap` cm of one of them is moved onto it: a stone that bulges 8 cm out of the wall is cut back to
//     the wall's face, a window sill that stands 25 cm out stays.
//  3. Lone cells go: a cell whose six neighbours mostly disagree with it is flipped.
//  4. Faces: each exposed cell face takes the material of the pack's surface nearest to it (wall stone, dressed stone,
//     wood, iron ...) and becomes a tile kind. Faces of one kind in one plane are merged into rectangles.
//  5. UVs at a fixed density in the piece's frame (1 UV unit = 1 m); the material decides the metres per tile.
//  6. Collision: the solid cells merged into boxes.
const fs = require('fs');
const path = require('path');

const [, , SRC, OUT, ...ONLY] = process.argv;
fs.mkdirSync(OUT, { recursive: true });
const CFG_FILE = path.join(__dirname, 'castle_twins_cfg.json');
const CFG = fs.existsSync(CFG_FILE) ? JSON.parse(fs.readFileSync(CFG_FILE, 'utf8')) : {};
const DEF = Object.assign({ grid: 6.25, snap: 12, minPeak: 0.25, block: 100, planeRound: 12.5, clean: 2, specks: 2, close: 1, maxBoxes: 400 }, CFG._default || {});

// The pack's material -> the tile kind it becomes (the same reading as castle_pixel_test.py's tile_for).
function tileFor(name) {
  const n = (name || '').toLowerCase();
  if (/glass/.test(n)) return 'glass';
  if (/wood|plank|varnish|herring|cart|crate|barrel|keg|table|chair|bench|door|shelf|cabinet|bed|stool/.test(n)) return 'wood';
  if (/metal|iron|chain|portcullis|torch|lantern|chandelier/.test(n)) return 'iron';
  if (/cobble/.test(n)) return 'cobble';
  if (/cloth|sack|rope|pillow|fabric|banner|flag|rug/.test(n)) return 'cloth';
  if (/roof|shingle|tiles_roof/.test(n)) return 'roof';
  if (/ground|landscape|basicshape|worldgrid|dirt/.test(n)) return 'earth';
  if (/brickkit|slab|stairs|floor|tile|stonefence/.test(n)) return 'dressed';
  if (/rock|firepit/.test(n)) return 'dark';
  return 'wall';
}

// ---- geometry ---------------------------------------------------------------------------------------------------
function load(name) {
  const d = JSON.parse(fs.readFileSync(path.join(SRC, name + '.json'), 'utf8'));
  return d;
}

// Rotate the piece about Z into its working frame (a tower's wall runs at 45 degrees in its own frame).
function frame(v, yawDeg) {
  if (!yawDeg) return v.slice();
  const a = (yawDeg * Math.PI) / 180, c = Math.cos(a), s = Math.sin(a);
  const o = new Float64Array(v.length);
  for (let i = 0; i < v.length; i += 3) { o[i] = v[i] * c + v[i + 1] * s; o[i + 1] = -v[i] * s + v[i + 1] * c; o[i + 2] = v[i + 2]; }
  return o;
}

// All hits of the rays along one axis, one ray per cell (u, v) of the plane across it.
// Returns hits[cellIndex] = sorted list of [depth, material].
function castAxis(V, T, M, axis, g, lo, n) {
  const ua = (axis + 1) % 3, va = (axis + 2) % 3;
  const nu = n[ua], nv = n[va];
  const bins = Array.from({ length: nu * nv }, () => []);
  const J = 0.0173;          // rays never run exactly along a modelled edge on a round number
  const tri = T.length / 3;
  for (let k = 0; k < tri; k++) {
    const a = T[k * 3] * 3, b = T[k * 3 + 1] * 3, c = T[k * 3 + 2] * 3;
    const u0 = Math.min(V[a + ua], V[b + ua], V[c + ua]), u1 = Math.max(V[a + ua], V[b + ua], V[c + ua]);
    const v0 = Math.min(V[a + va], V[b + va], V[c + va]), v1 = Math.max(V[a + va], V[b + va], V[c + va]);
    const i0 = Math.max(0, Math.floor((u0 - lo[ua]) / g - 0.5)), i1 = Math.min(nu - 1, Math.ceil((u1 - lo[ua]) / g - 0.5));
    const j0 = Math.max(0, Math.floor((v0 - lo[va]) / g - 0.5)), j1 = Math.min(nv - 1, Math.ceil((v1 - lo[va]) / g - 0.5));
    for (let i = i0; i <= i1; i++) for (let j = j0; j <= j1; j++) bins[i * nv + j].push(k);
  }
  const hits = new Array(nu * nv);
  for (let i = 0; i < nu; i++) {
    const pu = lo[ua] + (i + 0.5) * g + J;
    for (let j = 0; j < nv; j++) {
      const pv = lo[va] + (j + 0.5) * g + J * 0.61;
      const out = [];
      for (const k of bins[i * nv + j]) {
        const a = T[k * 3] * 3, b = T[k * 3 + 1] * 3, c = T[k * 3 + 2] * 3;
        const ax = V[a + ua], ay = V[a + va], bx = V[b + ua], by = V[b + va], cx = V[c + ua], cy = V[c + va];
        const d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
        if (Math.abs(d) < 1e-9) continue;
        const w0 = ((by - cy) * (pu - cx) + (cx - bx) * (pv - cy)) / d;
        const w1 = ((cy - ay) * (pu - cx) + (ax - cx) * (pv - cy)) / d;
        const w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        out.push([w0 * V[a + axis] + w1 * V[b + axis] + w2 * V[c + axis], M[k]]);
      }
      out.sort((p, q) => p[0] - q[0]);
      // two triangles that meet on the ray's path give the same depth twice
      const clean = [];
      for (const h of out) if (!clean.length || h[0] - clean[clean.length - 1][0] > 0.05) clean.push(h);
      hits[i * nv + j] = clean;
    }
  }
  return hits;
}

const NB6 = [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]];

// Triangle against box (separating axes, Akenine-Moller). Box: centre c, half size h.
function triBox(c, h, p0, p1, p2) {
  const v = [p0, p1, p2].map((p) => [p[0] - c[0], p[1] - c[1], p[2] - c[2]]);
  const e = [0, 1, 2].map((i) => { const a = v[i], b = v[(i + 1) % 3]; return [b[0] - a[0], b[1] - a[1], b[2] - a[2]]; });
  for (let a = 0; a < 3; a++) {
    const mn = Math.min(v[0][a], v[1][a], v[2][a]), mx = Math.max(v[0][a], v[1][a], v[2][a]);
    if (mn > h[a] || mx < -h[a]) return false;
  }
  const nrm = [e[0][1] * e[1][2] - e[0][2] * e[1][1], e[0][2] * e[1][0] - e[0][0] * e[1][2], e[0][0] * e[1][1] - e[0][1] * e[1][0]];
  const axes = [nrm];
  for (let i = 0; i < 3; i++) for (let a = 0; a < 3; a++) { const u = [0, 0, 0]; u[a] = 1; const ed = e[i]; axes.push([u[1] * ed[2] - u[2] * ed[1], u[2] * ed[0] - u[0] * ed[2], u[0] * ed[1] - u[1] * ed[0]]); }
  for (const ax of axes) {
    if (Math.abs(ax[0]) + Math.abs(ax[1]) + Math.abs(ax[2]) < 1e-12) continue;
    const p = v.map((q) => q[0] * ax[0] + q[1] * ax[1] + q[2] * ax[2]);
    const r = h[0] * Math.abs(ax[0]) + h[1] * Math.abs(ax[1]) + h[2] * Math.abs(ax[2]);
    if (Math.min(...p) > r || Math.max(...p) < -r) return false;
  }
  return true;
}

function surfaceCells(V, T, n, lo, g, idx) {
  const surf = new Uint8Array(n[0] * n[1] * n[2]);
  const h = [g / 2 + 0.01, g / 2 + 0.01, g / 2 + 0.01];   // a face on a cell boundary marks both cells; the rays decide the outer one
  for (let k = 0; k < T.length / 3; k++) {
    const P = [0, 1, 2].map((q) => { const i = T[k * 3 + q] * 3; return [V[i], V[i + 1], V[i + 2]]; });
    const c0 = [0, 1, 2].map((a) => Math.max(0, Math.floor((Math.min(P[0][a], P[1][a], P[2][a]) - lo[a]) / g)));
    const c1 = [0, 1, 2].map((a) => Math.min(n[a] - 1, Math.floor((Math.max(P[0][a], P[1][a], P[2][a]) - lo[a]) / g)));
    for (let x = c0[0]; x <= c1[0]; x++) for (let y = c0[1]; y <= c1[1]; y++) for (let z = c0[2]; z <= c1[2]; z++) {
      const i = idx(x, y, z);
      if (surf[i]) continue;
      if (triBox([lo[0] + (x + 0.5) * g, lo[1] + (y + 0.5) * g, lo[2] + (z + 0.5) * g], h, P[0], P[1], P[2])) surf[i] = 1;
    }
  }
  return surf;
}

// The space reachable from the grid's border without crossing the surface. close = how many cells a gap in the
// surface must be wider than to let the outside in (the surface is grown by that much for the fill, then the outside
// is grown back).
function floodOutside(surf, n, idx, close, seal) {
  const N = surf.length;
  let wall = surf;
  for (let r = 0; r < close; r++) wall = grow(wall, n, idx);
  wall = Uint8Array.from(wall);
  for (let i = 0; i < N; i++) if (seal[i]) wall[i] = 1;
  const out = new Uint8Array(N);
  const stack = [];
  for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) for (let z = 0; z < n[2]; z++) {
    if (x && y && z && x < n[0] - 1 && y < n[1] - 1 && z < n[2] - 1) continue;
    const i = idx(x, y, z);
    if (!wall[i] && !out[i]) { out[i] = 1; stack.push(x, y, z); }
  }
  while (stack.length) {
    const z = stack.pop(), y = stack.pop(), x = stack.pop();
    for (const [dx, dy, dz] of NB6) {
      const X = x + dx, Y = y + dy, Z = z + dz;
      if (X < 0 || Y < 0 || Z < 0 || X >= n[0] || Y >= n[1] || Z >= n[2]) continue;
      const j = idx(X, Y, Z);
      if (!wall[j] && !out[j]) { out[j] = 1; stack.push(X, Y, Z); }
    }
  }
  let o = out;
  for (let r = 0; r < close; r++) { o = grow(o, n, idx); for (let i = 0; i < N; i++) if (surf[i] || seal[i]) o[i] = out[i]; }
  return o;
}

function grow(m, n, idx) {
  const o = Uint8Array.from(m);
  for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) for (let z = 0; z < n[2]; z++) {
    if (!m[idx(x, y, z)]) continue;
    for (const [dx, dy, dz] of NB6) {
      const X = x + dx, Y = y + dy, Z = z + dz;
      if (X < 0 || Y < 0 || Z < 0 || X >= n[0] || Y >= n[1] || Z >= n[2]) continue;
      o[idx(X, Y, Z)] = 1;
    }
  }
  return o;
}

function voxelize(src, opt, base) {
  const V = frame(src.v, opt.yaw || 0), T = src.t, M = src.m;
  const g = opt.grid;
  const mn = [Infinity, Infinity, Infinity], mx = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i < V.length; i += 3) for (let a = 0; a < 3; a++) { mn[a] = Math.min(mn[a], V[i + a]); mx[a] = Math.max(mx[a], V[i + a]); }
  // the grid is fixed to the pivot: cell k covers [k g, (k + 1) g]
  const A = g * (opt.coarse || 1);       // the grid is aligned to the coarse cell, so the coarse cells fall on round numbers
  const lo = mn.map((x) => Math.floor(x / A - 0.001) * A - A), hi = mx.map((x) => Math.ceil(x / A + 0.001) * A + A);
  const n = lo.map((x, a) => Math.round((hi[a] - x) / g));
  const N = n[0] * n[1] * n[2];
  const idx = (x, y, z) => (x * n[1] + y) * n[2] + z;
  const votes = new Uint8Array(N), thin = new Uint8Array(N);
  const hitsBy = [];
  if (base && opt.yaw) {
    // a part made in a turned frame takes its cells from the solid found in the mesh's own frame (where the module's
    // open ends can be sealed); the turned grid only gives its faces their plane
    const solid = new Uint8Array(N);
    const ya = (opt.yaw * Math.PI) / 180, c = Math.cos(ya), sn = Math.sin(ya);
    for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) {
      const fx = lo[0] + (x + 0.5) * g, fy = lo[1] + (y + 0.5) * g;
      const bx = Math.floor((fx * c - fy * sn - base.lo[0]) / base.g), by = Math.floor((fx * sn + fy * c - base.lo[1]) / base.g);
      if (bx < 0 || by < 0 || bx >= base.n[0] || by >= base.n[1]) continue;
      for (let z = 0; z < n[2]; z++) {
        const bz = Math.floor((lo[2] + (z + 0.5) * g - base.lo[2]) / base.g);
        if (bz >= 0 && bz < base.n[2] && base.solid[base.idx(bx, by, bz)]) solid[idx(x, y, z)] = 1;
      }
    }
    for (let axis = 0; axis < 3; axis++) hitsBy.push(castAxis(V, T, M, axis, g, lo, n));
    return { solid, n, lo, g, idx, hitsBy, V };
  }
  for (let axis = 0; axis < 3; axis++) {
    const hits = castAxis(V, T, M, axis, g, lo, n);
    hitsBy.push(hits);
    const ua = (axis + 1) % 3, va = (axis + 2) % 3;
    for (let i = 0; i < n[ua]; i++) for (let j = 0; j < n[va]; j++) {
      const h = hits[i * n[va] + j];
      for (let p = 0; p < h.length; p += 2) {
        const a = h[p][0], b = p + 1 < h.length ? h[p + 1][0] : h[p][0];
        let k0 = Math.ceil((a - lo[axis]) / g - 0.5), k1 = Math.floor((b - lo[axis]) / g - 0.5);
        let isThin = false;
        if (k1 < k0) { k0 = k1 = Math.floor(((a + b) / 2 - lo[axis]) / g); isThin = true; }
        for (let k = Math.max(0, k0); k <= Math.min(n[axis] - 1, k1); k++) {
          const c = [0, 0, 0]; c[axis] = k; c[ua] = i; c[va] = j;
          const id = idx(c[0], c[1], c[2]);
          votes[id]++;
          if (isThin) thin[id] = 1;
        }
      }
    }
  }
  // Cells the surface passes through, and the space that cannot be reached from outside through them: the pack's
  // walls are often shells of sculpted stones around nothing, which the rays alone read as empty.
  const surf = surfaceCells(V, T, n, lo, g, idx);
  // The pack's modules are often open where they meet the next one (a wall's two faces, with nothing at its ends, top
  // or bottom). seal = the sides of the mesh's own box that count as closed: ["x-", "x+", "y-", "y+", "z-", "z+"].
  // seal = ["x+:y", ...]: the side x+ of the mesh's own box is closed, row by row along y, between the first and the
  // last cell the surface touches there (two cells deep). Only in the mesh's own frame.
  if (opt.seal && opt.seal.length && !opt.yaw) {
    const b = src.bounds;
    for (const sd of opt.seal) {
      const a = 'xyz'.indexOf(sd[0]), side = sd[1], f = 'xyz'.indexOf(sd[3]), t = 3 - a - f;
      const edge = side === '-' ? Math.floor((b[0][a] - lo[a]) / g) : Math.floor((b[1][a] - lo[a]) / g);
      for (let L = 0; L < 2; L++) {
        const k = side === '-' ? edge + L : edge - L;
        if (k < 0 || k >= n[a]) continue;
        for (let q = 0; q < n[t]; q++) {
          let first = -1, last = -1;
          const id = (r) => { const c = [0, 0, 0]; c[a] = k; c[f] = r; c[t] = q; return idx(c[0], c[1], c[2]); };
          for (let r = 0; r < n[f]; r++) if (surf[id(r)]) { if (first < 0) first = r; last = r; }
          for (let r = first; r >= 0 && r <= last; r++) surf[id(r)] = 1;
        }
      }
    }
  }
  const seal = new Uint8Array(N);
  const outside = floodOutside(surf, n, idx, opt.close, seal);
  const solid = new Uint8Array(N);
  for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) for (let z = 0; z < n[2]; z++) {
    const i = idx(x, y, z);
    if (votes[i] >= 2 || thin[i]) { solid[i] = 1; continue; }
    if (outside[i]) continue;
    if (!surf[i]) { solid[i] = 1; continue; }
    // a surface cell that touches enclosed space belongs to the body
    for (const [dx, dy, dz] of NB6) {
      const X = x + dx, Y = y + dy, Z = z + dz;
      if (X < 0 || Y < 0 || Z < 0 || X >= n[0] || Y >= n[1] || Z >= n[2]) continue;
      const j = idx(X, Y, Z);
      if (!outside[j] && !surf[j]) { solid[i] = 1; break; }
    }
  }
  return { solid, n, lo, g, idx, hitsBy, V };
}

// Step 2: real faces by depth along each axis, and lumps cut back (or filled) onto them.
function snapFaces(vx, opt) {
  const { solid, n, g, idx } = vx;
  const snapCells = Math.round(opt.snap / g);
  if (snapCells < 1) return 0;
  let changed = 0;
  for (const axis of opt.snapAxes || [0, 1, 2]) {
    const ua = (axis + 1) % 3, va = (axis + 2) % 3;
    // count, per depth boundary k (between cell k-1 and k), the columns that start solid there and that stop there
    // Counted over the whole piece and, separately, around each metre of its face (so that a small face, the top of a
    // parapet or one side of a tower, finds its own plane too).
    const B = Math.max(4, Math.round(opt.block / g));
    const nbu = Math.ceil(n[ua] / B), nbv = Math.ceil(n[va] / B);
    const L = n[axis] + 1;
    const bStarts = Array.from({ length: nbu * nbv }, () => new Float64Array(L)), bStops = Array.from({ length: nbu * nbv }, () => new Float64Array(L));
    const bCols = new Float64Array(nbu * nbv);
    const starts = new Float64Array(L), stops = new Float64Array(L);
    let cols = 0;
    const at = (i, j, k) => { const c = [0, 0, 0]; c[axis] = k; c[ua] = i; c[va] = j; return idx(c[0], c[1], c[2]); };
    for (let i = 0; i < n[ua]; i++) for (let j = 0; j < n[va]; j++) {
      let any = false;
      const bb = Math.floor(i / B) * nbv + Math.floor(j / B);
      for (let k = 0; k < n[axis]; k++) {
        const s = solid[at(i, j, k)], p = k > 0 ? solid[at(i, j, k - 1)] : 0;
        if (s && !p) { starts[k]++; bStarts[bb][k]++; any = true; }
        if (!s && p) { stops[k]++; bStops[bb][k]++; }
      }
      if (solid[at(i, j, n[axis] - 1)]) { stops[n[axis]]++; bStops[bb][n[axis]]++; }
      if (any) { cols++; bCols[bb]++; }
    }
    // a real face: the depth with the most starts (or stops) within the snap's reach of it, holding a good share of
    // all the columns; a sculpted face spreads its starts over several depths, so they are summed over that reach
    const peaks = (hist, cols) => {
      const out = [];
      const L = hist.length;
      const sum = new Float64Array(L);
      for (let k = 0; k < L; k++) for (let d = -snapCells; d <= snapCells; d++) if (k + d >= 0 && k + d < L) sum[k] += hist[k + d] * (1 - Math.abs(d) / (snapCells + 1));
      for (let k = 0; k < L; k++) {
        if (!hist[k]) continue;
        let best = true;
        for (let d = -snapCells; d <= snapCells; d++) {
          const q = k + d;
          if (q < 0 || q >= L || q === k) continue;
          if (sum[q] > sum[k] || (sum[q] === sum[k] && q < k)) { best = false; break; }
        }
        if (best && sum[k] >= Math.max(2, opt.minPeak * cols)) {
          // faces of different pieces meet on the same planes: a real face is put on the nearest 12.5 cm of the
          // piece's own frame (the pack's modules are laid out on round numbers)
          const p = vx.lo[axis] + k * g, r = opt.planeRound ? Math.round(p / opt.planeRound) * opt.planeRound : p;
          out.push(Math.max(0, Math.min(L - 1, Math.round((r - vx.lo[axis]) / g))));
        }
      }
      return out;
    };
    const gStart = peaks(starts, cols), gStop = peaks(stops, cols);
    const local = new Array(nbu * nbv);
    for (let bu = 0; bu < nbu; bu++) for (let bv = 0; bv < nbv; bv++) {
      const hs = new Float64Array(L), hp = new Float64Array(L);
      let c = 0;
      for (let du = -1; du <= 1; du++) for (let dv = -1; dv <= 1; dv++) {
        const u = bu + du, v = bv + dv;
        if (u < 0 || v < 0 || u >= nbu || v >= nbv) continue;
        const q = u * nbv + v;
        for (let k = 0; k < L; k++) { hs[k] += bStarts[q][k]; hp[k] += bStops[q][k]; }
        c += bCols[q];
      }
      local[bu * nbv + bv] = [gStart.concat(peaks(hs, c)), gStop.concat(peaks(hp, c))];
    }
    const nearest = (ps, k) => { let b = null; for (const p of ps) if (Math.abs(p - k) <= snapCells && (b === null || Math.abs(p - k) < Math.abs(b - k))) b = p; return b; };
    for (let i = 0; i < n[ua]; i++) for (let j = 0; j < n[va]; j++) {
      const [pStart, pStop] = local[Math.floor(i / B) * nbv + Math.floor(j / B)];
      // the column's solid runs
      const runs = [];
      let s0 = -1;
      for (let k = 0; k <= n[axis]; k++) {
        const s = k < n[axis] ? solid[at(i, j, k)] : 0;
        if (s && s0 < 0) s0 = k;
        if (!s && s0 >= 0) { runs.push([s0, k]); s0 = -1; }
      }
      for (const r of runs) {
        const a = nearest(pStart, r[0]), b = nearest(pStop, r[1]);
        let na = a === null ? r[0] : a, nb = b === null ? r[1] : b;
        if (nb <= na) continue;                      // a run thinner than the snap: leave it as it is
        for (let k = Math.min(r[0], na); k < Math.max(r[1], nb); k++) {
          const want = k >= na && k < nb ? 1 : 0;
          const id = at(i, j, k);
          if (solid[id] !== want) {
            // only cut back or fill within the run's own reach: never fill across empty space beyond the snap
            if (want && !(k >= r[0] - snapCells && k < r[1] + snapCells)) continue;
            solid[id] = want; changed++;
          }
        }
      }
    }
  }
  return changed;
}

// Step 3: a cell that at least five of its six neighbours disagree with is flipped (a lone lump or pit).
function cleanLone(vx, passes, limit) {
  limit = limit || 5;
  const { solid, n, idx } = vx;
  let changed = 0;
  for (let p = 0; p < passes; p++) {
    const flip = [];
    for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) for (let z = 0; z < n[2]; z++) {
      const s = solid[idx(x, y, z)];
      let dis = 0;
      for (const [dx, dy, dz] of [[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]]) {
        const X = x + dx, Y = y + dy, Z = z + dz;
        const t = X < 0 || Y < 0 || Z < 0 || X >= n[0] || Y >= n[1] || Z >= n[2] ? 0 : solid[idx(X, Y, Z)];
        if (t !== s) dis++;
      }
      if (dis >= limit) flip.push(idx(x, y, z));
    }
    for (const i of flip) solid[i] ^= 1;
    changed += flip.length;
    if (!flip.length) break;
  }
  return changed;
}

// A speck: a cell or two standing out of a face (what the snap left of a big stone). It sits on a cell that is part
// of a face (most of that cell's side neighbours are solid) and has at most one solid side neighbour itself. A bar or a
// mullion is not a speck: what it stands on is not a face.
function cleanSpecks(vx, passes) {
  const { solid, n, idx } = vx;
  const at = (x, y, z) => (x < 0 || y < 0 || z < 0 || x >= n[0] || y >= n[1] || z >= n[2] ? 0 : solid[idx(x, y, z)]);
  let changed = 0;
  for (let p = 0; p < passes; p++) {
    const flip = [];
    for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) for (let z = 0; z < n[2]; z++) {
      if (!solid[idx(x, y, z)]) continue;
      for (const d of NB6) {
        if (at(x + d[0], y + d[1], z + d[2])) continue;
        const lat = (px, py, pz) => { let c = 0; for (const e of NB6) if (e[0] * d[0] + e[1] * d[1] + e[2] * d[2] === 0) c += at(px + e[0], py + e[1], pz + e[2]); return c; };
        // a stick up to three cells long, standing on a face
        const col = [];
        let ok = false;
        for (let k = 0; k <= 3; k++) {
          const px = x - k * d[0], py = y - k * d[1], pz = z - k * d[2];
          if (!at(px, py, pz)) break;
          const l = lat(px, py, pz);
          if (k > 0 && l >= 3) { ok = true; break; }
          if (l > 1) break;
          col.push(idx(px, py, pz));
        }
        if (ok && col.length) { flip.push(...col); break; }
      }
    }
    for (const i of flip) solid[i] = 0;
    changed += flip.length;
    if (!flip.length) break;
  }
  return changed;
}

// A comb: a row of teeth one block wide standing out of a face (the corbels under a parapet, read on a 25 cm grid).
// A cell one block wide along one horizontal axis that also stands free along the other is taken away.
function cleanCombs(vx) {
  const { solid, n, idx } = vx;
  const at = (x, y, z) => (x < 0 || y < 0 || z < 0 || x >= n[0] || y >= n[1] || z >= n[2] ? 0 : solid[idx(x, y, z)]);
  const flip = [];
  for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) for (let z = 0; z < n[2]; z++) {
    if (!solid[idx(x, y, z)]) continue;
    const xs = at(x - 1, y, z) + at(x + 1, y, z), ys = at(x, y - 1, z) + at(x, y + 1, z);
    if ((xs === 0 && ys === 1) || (ys === 0 && xs === 1)) flip.push(idx(x, y, z));
  }
  for (const i of flip) solid[i] = 0;
  return flip.length;
}

// Step 4: exposed faces, their material, merged into rectangles.
function faces(vx, src, opt) {
  const { solid, n, lo, g, idx, hitsBy } = vx;
  const kinds = src.mats.map((m) => (opt.tiles && opt.tiles[m]) || opt.tile || tileFor(m));
  const fallback = (() => { const c = {}; for (const m of src.m) c[kinds[m]] = (c[kinds[m]] || 0) + 1; return Object.entries(c).sort((a, b) => b[1] - a[1])[0][0]; })();
  const quads = [];        // [axis, dir, k (boundary), u0, u1, v0, v1, kind]
  for (let axis = 0; axis < 3; axis++) {
    const ua = (axis + 1) % 3, va = (axis + 2) % 3;
    const at = (i, j, k) => { if (k < 0 || k >= n[axis]) return 0; const c = [0, 0, 0]; c[axis] = k; c[ua] = i; c[va] = j; return solid[idx(c[0], c[1], c[2])]; };
    const hits = hitsBy[axis];
    for (const dir of [1, -1]) {
      for (let k = 0; k < n[axis]; k++) {
        // the face between cell k and its neighbour in direction dir
        const mask = new Array(n[ua] * n[va]).fill(null);
        let any = false;
        for (let i = 0; i < n[ua]; i++) for (let j = 0; j < n[va]; j++) {
          if (at(i, j, k) && !at(i, j, k + dir)) {
            const depth = lo[axis] + (k + (dir > 0 ? 1 : 0)) * g;
            let best = null, bd = 3 * g;
            for (const h of hits[i * n[va] + j]) { const d = Math.abs(h[0] - depth); if (d < bd) { bd = d; best = h[1]; } }
            mask[i * n[va] + j] = best === null ? fallback : kinds[best];
            any = true;
          }
        }
        if (!any) continue;
        // A face is one tile, not a patchwork: a strip of another tile thinner than three cells (the edge of a quoin,
        // the end of a sill that the snap cut back) takes the tile around it.
        for (let pass = 0; pass < 2; pass++) {
          const W = n[ua], Hh = n[va], next = mask.slice();
          for (let i = 0; i < W; i++) for (let j = 0; j < Hh; j++) {
            const kd = mask[i * Hh + j];
            if (!kd) continue;
            let a = i, b = i, c = j, d = j;
            while (a > 0 && mask[(a - 1) * Hh + j] === kd) a--;
            while (b < W - 1 && mask[(b + 1) * Hh + j] === kd) b++;
            while (c > 0 && mask[i * Hh + c - 1] === kd) c--;
            while (d < Hh - 1 && mask[i * Hh + d + 1] === kd) d++;
            if (Math.min(b - a + 1, d - c + 1) >= (g >= 20 ? 2 : 3)) continue;
            const cnt = {};
            for (let p = -2; p <= 2; p++) for (let q = -2; q <= 2; q++) {
              const x = i + p, y = j + q;
              if (x < 0 || y < 0 || x >= W || y >= Hh) continue;
              const o = mask[x * Hh + y];
              if (o && o !== kd) cnt[o] = (cnt[o] || 0) + 1;
            }
            const best = Object.entries(cnt).sort((u, v) => v[1] - u[1])[0];
            if (best && best[1] >= 6) next[i * Hh + j] = best[0];
          }
          for (let q = 0; q < mask.length; q++) mask[q] = next[q];
        }
        // On plain blocks a face is one tile, whole: each connected flat region takes the tile most of it has.
        if (g >= 20) {
          // stone walls are one coursed wall, stone tops one dressed paving: no stripes where two modules meet
          for (let q = 0; q < mask.length; q++) {
            if (!mask[q]) continue;
            if (axis !== 2 && (mask[q] === 'dressed' || mask[q] === 'dark')) mask[q] = 'wall';
            if (axis === 2 && (mask[q] === 'wall' || mask[q] === 'dark')) mask[q] = 'dressed';
          }
          const W = n[ua], Hh = n[va], seen = new Uint8Array(W * Hh);
          for (let st = 0; st < W * Hh; st++) {
            if (!mask[st] || seen[st]) continue;
            const comp = [st], cnt = {};
            seen[st] = 1;
            for (let q = 0; q < comp.length; q++) {
              const c = comp[q], i = Math.floor(c / Hh), j = c % Hh;
              cnt[mask[c]] = (cnt[mask[c]] || 0) + 1;
              for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
                const x = i + di, y = j + dj;
                if (x < 0 || y < 0 || x >= W || y >= Hh) continue;
                const o = x * Hh + y;
                if (mask[o] && !seen[o]) { seen[o] = 1; comp.push(o); }
              }
            }
            const best = Object.entries(cnt).sort((u, v) => v[1] - u[1])[0][0];
            for (const c of comp) mask[c] = best;
          }
        }
        // greedy rectangles
        for (let i = 0; i < n[ua]; i++) for (let j = 0; j < n[va]; j++) {
          const kind = mask[i * n[va] + j];
          if (!kind) continue;
          let j1 = j;
          while (j1 + 1 < n[va] && mask[i * n[va] + j1 + 1] === kind) j1++;
          let i1 = i, ok = true;
          while (ok && i1 + 1 < n[ua]) {
            for (let q = j; q <= j1; q++) if (mask[(i1 + 1) * n[va] + q] !== kind) { ok = false; break; }
            if (ok) i1++;
          }
          for (let p = i; p <= i1; p++) for (let q = j; q <= j1; q++) mask[p * n[va] + q] = null;
          quads.push([axis, dir, k + (dir > 0 ? 1 : 0), i, i1 + 1, j, j1 + 1, kind]);
        }
      }
    }
  }
  return quads;
}

// Step 6: solid cells merged into boxes for collision (greedy along z, then y, then x).
function boxes(vx) {
  const { solid, n, idx } = vx;
  const used = new Uint8Array(solid.length);
  const out = [];
  const free = (x, y, z) => solid[idx(x, y, z)] && !used[idx(x, y, z)];
  for (let x = 0; x < n[0]; x++) for (let y = 0; y < n[1]; y++) for (let z = 0; z < n[2]; z++) {
    if (!free(x, y, z)) continue;
    let z1 = z; while (z1 + 1 < n[2] && free(x, y, z1 + 1)) z1++;
    let y1 = y, ok = true;
    while (ok && y1 + 1 < n[1]) { for (let q = z; q <= z1; q++) if (!free(x, y1 + 1, q)) { ok = false; break; } if (ok) y1++; }
    let x1 = x; ok = true;
    while (ok && x1 + 1 < n[0]) {
      for (let p = y; p <= y1 && ok; p++) for (let q = z; q <= z1; q++) if (!free(x1 + 1, p, q)) { ok = false; break; }
      if (ok) x1++;
    }
    for (let a = x; a <= x1; a++) for (let b = y; b <= y1; b++) for (let c = z; c <= z1; c++) used[idx(a, b, c)] = 1;
    out.push([x, y, z, x1 + 1, y1 + 1, z1 + 1]);
  }
  return out;
}

// A plain box [x0, y0, z0, x1, y1, z1] in place of the pack's piece (a cobbled road becomes a flat paved slab).
function boxMesh(b, src, tile) {
  const [x0, y0, z0, x1, y1, z1] = b;
  const v = [x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1];
  const t = [0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7];
  return Object.assign({}, src, { v, t, m: new Array(12).fill(0), mats: [tile], bounds: [[x0, y0, z0], [x1, y1, z1]] });
}

function build(name) {
  let src = load(name);
  // Props (kegs, barrels, sacks, the cart) are rounded things, not walls: a finer grid and no snapping to faces.
  const prop = src.path.includes('/Props/') || src.path.includes('/Rock_Kit/');
  const stair = /Stair/.test(name);
  // a thin piece (a coping, an edging, a door frame) on 12.5 cm blocks, so it keeps its shape
  const minExt = Math.min(...[0, 1, 2].map((q) => src.bounds[1][q] - src.bounds[0][q]));
  const opt = Object.assign({}, DEF, prop ? { grid: 3.125, snap: 0, specks: 0 } : stair ? {} : { coarse: minExt < 80 ? 2 : 4 }, CFG[name] || {});
  // A kit module is usually two sculpted faces with nothing at its ends: its thinnest axis is its thickness, and the
  // four sides across it are closed for the fill (only between the faces).
  if (!prop && !opt.seal && !opt.box) {
    const b = src.bounds, ext = [0, 1, 2].map((a) => b[1][a] - b[0][a]);
    const thin = [0, 1, 2].reduce((m, a) => (ext[a] < ext[m] ? a : m), 0);
    if ([0, 1, 2].every((a) => a === thin || ext[thin] < 0.5 * ext[a])) {
      const t = 'xyz'[thin];
      opt.seal = [];
      for (const a of [0, 1, 2]) if (a !== thin) opt.seal.push('xyz'[a] + '-:' + t, 'xyz'[a] + '+:' + t);
    }
  }
  if (opt.box) { src = boxMesh(opt.box, src, opt.tile || 'stone'); opt.snap = 0; }
  // Pieces that are already plain (the crypt's set, the arcades, the spiral stairs) and small things (cups, candles,
  // tools, a chandelier: the grid would lose them) keep their own triangles with the tiles.
  const maxExt = Math.max(...[0, 1, 2].map((q) => src.bounds[1][q] - src.bounds[0][q]));
  if (opt.keep === undefined && (src.path.includes('/Candle/') || /Arch_Walkway|Spiral_Stairs|Chandelier|ChainLink/.test(name) || (prop && maxExt < 60))) opt.keep = true;
  if (/Window/.test(name) && (CFG[name] || {}).close === undefined) opt.close = 0;   // a lancet is narrower than the fill's closing
  if (opt.hull) return hullTwin(name, src, opt);
  if (opt.keep) return keepSource(name, src, opt);
  // A window's iron lattice fills its opening: it is left out (the map's builder sets an unseen pane in every opening).
  const drop = opt.drop || (/Window/.test(name) ? ['iron', 'glass'] : ['glass']);
  if (drop.length) {
    const kinds = src.mats.map((m) => (opt.tiles && opt.tiles[m]) || opt.tile || tileFor(m));
    const t = [], m = [];
    for (let k = 0; k < src.m.length; k++) if (!drop.includes(kinds[src.m[k]])) { t.push(src.t[k * 3], src.t[k * 3 + 1], src.t[k * 3 + 2]); m.push(src.m[k]); }
    src = Object.assign({}, src, { t, m });
  }
  const parts = opt.parts || [{}];
  const sections = {};
  let boxList = [], cells = 0, snapped = 0, lone = 0;
  const turned = parts.some((p) => p.yaw);
  const base0 = turned ? voxelize(src, Object.assign({}, opt, { yaw: 0 })) : null;
  for (const part of parts) {
    const po = Object.assign({}, opt, part);
    let s = src;
    if (false) {
      // only the triangles whose middle lies on the given side of a vertical plane: [nx, ny, d] keeps nx x + ny y >= d
      const [nx, ny, d] = part.keep;
      const t = [], m = [];
      for (let k = 0; k < src.t.length / 3; k++) {
        const a = src.t[k * 3] * 3, b = src.t[k * 3 + 1] * 3, c = src.t[k * 3 + 2] * 3;
        const x = (src.v[a] + src.v[b] + src.v[c]) / 3, y = (src.v[a + 1] + src.v[b + 1] + src.v[c + 1]) / 3;
        if (nx * x + ny * y >= d) { t.push(src.t[k * 3], src.t[k * 3 + 1], src.t[k * 3 + 2]); m.push(src.m[k]); }
      }
      s = Object.assign({}, src, { t, m });
    }
    const vx = voxelize(s, po, base0);
    snapped += snapFaces(vx, po);
    lone += cleanLone(vx, po.clean);
    lone += cleanSpecks(vx, po.specks);
    // An opening stays open: where a ray across the wall's thickness meets nothing of the piece (a window, a door, an
    // arrow slit), the column is emptied again, whatever the snapping and the fill did around it. The thickness axis is
    // the piece's thinnest (or the part's "through" axis); hits outside a part's planes do not count.
    {
      let ax = po.through;
      if (ax === undefined && !part.planes) {
        const b = s.bounds, ext = [0, 1, 2].map((a) => b[1][a] - b[0][a]);
        const thin = [0, 1, 2].reduce((m, a) => (ext[a] < ext[m] ? a : m), 0);
        if ([0, 1, 2].every((a) => a === thin || ext[thin] < 0.5 * ext[a])) ax = thin;
      }
      vx.throughAxis = ax;
      if (ax !== undefined && ax !== null && /Window/.test(name) && po.windowsOpen !== false) {
        // a window here is a recess with a pane at the back; in plain blocks it is a real opening (the map's builder
        // sets an unseen pane in it): a column much thinner than the wall is emptied
        const ua = (ax + 1) % 3, va = (ax + 2) % 3, hist = {};
        const cnt = (i, j) => { let c = 0; for (let k = 0; k < vx.n[ax]; k++) { const cc = [0, 0, 0]; cc[ax] = k; cc[ua] = i; cc[va] = j; c += vx.solid[vx.idx(cc[0], cc[1], cc[2])]; } return c; };
        for (let i = 0; i < vx.n[ua]; i++) for (let j = 0; j < vx.n[va]; j++) { const c = cnt(i, j); if (c) hist[c] = (hist[c] || 0) + 1; }
        const T = +Object.entries(hist).sort((a, b) => b[1] - a[1])[0][0];
        for (let i = 0; i < vx.n[ua]; i++) for (let j = 0; j < vx.n[va]; j++) {
          const c = cnt(i, j);
          if (c > 0 && c < 0.65 * T) for (let k = 0; k < vx.n[ax]; k++) { const cc = [0, 0, 0]; cc[ax] = k; cc[ua] = i; cc[va] = j; vx.solid[vx.idx(cc[0], cc[1], cc[2])] = 0; }
        }
      }
      if (ax !== undefined && ax !== null) {
        const ua = (ax + 1) % 3, va = (ax + 2) % 3, hits = vx.hitsBy[ax];
        const ya = ((po.yaw || 0) * Math.PI) / 180, c = Math.cos(ya), sn = Math.sin(ya);
        const inPart = (p) => !part.planes || part.planes.every(([nx, ny, d]) => nx * (p[0] * c - p[1] * sn) + ny * (p[0] * sn + p[1] * c) >= d - vx.g);
        // the piece's own extent across the opening's plane: rays outside it are not openings
        let any = 0;
        for (let i = 0; i < vx.n[ua]; i++) for (let j = 0; j < vx.n[va]; j++) {
          const p = [0, 0, 0]; p[ua] = vx.lo[ua] + (i + 0.5) * vx.g; p[va] = vx.lo[va] + (j + 0.5) * vx.g;
          let blocked = false, solidCol = false;
          for (const h of hits[i * vx.n[va] + j]) { p[ax] = h[0]; if (inPart(p)) { blocked = true; break; } }
          if (blocked) continue;
          for (let k = 0; k < vx.n[ax]; k++) {
            const cc = [0, 0, 0]; cc[ax] = k; cc[ua] = i; cc[va] = j;
            const id = vx.idx(cc[0], cc[1], cc[2]);
            if (vx.solid[id]) { p[ax] = vx.lo[ax] + (k + 0.5) * vx.g; if (inPart(p)) { vx.solid[id] = 0; solidCol = true; } }
          }
          if (solidCol) any++;
        }
        if (any) lone += any;
      }
    }
    // A module ends where its neighbour begins: the pack's stones overhang its nominal box by a few centimetres, and two
    // twins that overlap would show two coplanar faces fighting. Bounds within 8 cm of a multiple of 25 cm are that
    // multiple; nothing of the twin is left beyond them.
    {
      const nb = src.bounds.map((v) => v.map((x) => { const r = Math.round(x / 25) * 25; return Math.abs(r - x) <= 8 ? r : x; }));
      const ya = ((po.yaw || 0) * Math.PI) / 180, c = Math.cos(ya), sn = Math.sin(ya);
      for (let x = 0; x < vx.n[0]; x++) for (let y = 0; y < vx.n[1]; y++) {
        const fx = vx.lo[0] + (x + 0.5) * vx.g, fy = vx.lo[1] + (y + 0.5) * vx.g;
        const mxp = fx * c - fy * sn, myp = fx * sn + fy * c;
        const outXY = mxp < nb[0][0] || mxp > nb[1][0] || myp < nb[0][1] || myp > nb[1][1];
        for (let z = 0; z < vx.n[2]; z++) {
          const fz = vx.lo[2] + (z + 0.5) * vx.g;
          if (outXY || fz < nb[0][2] || fz > nb[1][2]) vx.solid[vx.idx(x, y, z)] = 0;
        }
      }
    }
    // Plain blocks (owner, 2026-11-14: "muros lisos ... rectangulos"): the fine cells are merged into coarse ones
    // (25 cm), each solid when most of its volume is. Nothing smaller than a coarse cell survives on a face.
    if (po.coarse && po.coarse > 1) {
      const F = po.coarse, nC = vx.n.map((x) => Math.round(x / F)), G = vx.g * F;
      // a fine column the wall's thickness leaves empty is an opening (a lancet window 30 cm wide is narrower than two
      // blocks): a block a third of whose columns are open stays open
      let openCol = null;
      const tax = vx.throughAxis;
      if (tax !== undefined && tax !== null) {
        const ua = (tax + 1) % 3, va = (tax + 2) % 3;
        openCol = new Uint8Array(vx.n[ua] * vx.n[va]);
        let anyAt = new Uint8Array(vx.n[ua] * vx.n[va]);
        for (let i = 0; i < vx.n[ua]; i++) for (let j = 0; j < vx.n[va]; j++) {
          let c = 0;
          for (let k = 0; k < vx.n[tax]; k++) { const cc = [0, 0, 0]; cc[tax] = k; cc[ua] = i; cc[va] = j; c += vx.solid[vx.idx(cc[0], cc[1], cc[2])]; }
          openCol[i * vx.n[va] + j] = c === 0 ? 1 : 0;
        }
        // only openings inside the piece count: an empty column outside its outline is not a window
        const W = vx.n[ua], Hh = vx.n[va], inside = new Uint8Array(W * Hh);
        for (let i = 0; i < W; i++) { let a = -1, b = -1; for (let j = 0; j < Hh; j++) if (!openCol[i * Hh + j]) { if (a < 0) a = j; b = j; } for (let j = a; j >= 0 && j <= b; j++) inside[i * Hh + j]++; }
        for (let j = 0; j < Hh; j++) { let a = -1, b = -1; for (let i = 0; i < W; i++) if (!openCol[i * Hh + j]) { if (a < 0) a = i; b = i; } for (let i = a; i >= 0 && i <= b; i++) inside[i * Hh + j]++; }
        for (let q = 0; q < W * Hh; q++) openCol[q] = openCol[q] && inside[q] === 2 ? 1 : 0;
      }
      const idxC = (x, y, z) => (x * nC[1] + y) * nC[2] + z;
      const sc = new Uint8Array(nC[0] * nC[1] * nC[2]);
      for (let x = 0; x < nC[0]; x++) for (let y = 0; y < nC[1]; y++) for (let z = 0; z < nC[2]; z++) {
        let c = 0;
        for (let a = 0; a < F; a++) for (let b = 0; b < F; b++) for (let d = 0; d < F; d++) {
          const X = x * F + a, Y = y * F + b, Z = z * F + d;
          if (X < vx.n[0] && Y < vx.n[1] && Z < vx.n[2]) c += vx.solid[vx.idx(X, Y, Z)];
        }
        if (c * 2 >= F * F * F) sc[idxC(x, y, z)] = 1;
        if (openCol && sc[idxC(x, y, z)]) {
          const ua = (tax + 1) % 3, va = (tax + 2) % 3, cc = [x, y, z];
          let o = 0;
          for (let a = 0; a < F; a++) for (let b = 0; b < F; b++) {
            const i = cc[ua] * F + a, j = cc[va] * F + b;
            if (i < vx.n[ua] && j < vx.n[va]) o += openCol[i * vx.n[va] + j];
          }
          if (o * 8 >= F * F) sc[idxC(x, y, z)] = 0;      // an arrow slit 10-15 cm wide becomes a block-wide opening
        }
      }
      // a window is a clean rectangle: each opening through the wall is cut to its whole bounding box
      if (openCol && /Window/.test(name)) {
        const ua = (tax + 1) % 3, va = (tax + 2) % 3, W = nC[ua], Hh = nC[va];
        const at = (i, j, k) => { const c = [0, 0, 0]; c[tax] = k; c[ua] = i; c[va] = j; return idxC(c[0], c[1], c[2]); };
        const col = new Uint8Array(W * Hh), has = new Uint8Array(W * Hh);
        for (let i = 0; i < W; i++) for (let j = 0; j < Hh; j++) { let c = 0; for (let k = 0; k < nC[tax]; k++) c += sc[at(i, j, k)]; has[i * Hh + j] = c ? 1 : 0; }
        // empty columns inside the outline (solid somewhere left and right, and above and below)
        for (let i = 0; i < W; i++) for (let j = 0; j < Hh; j++) {
          if (has[i * Hh + j]) continue;
          let l = 0, r = 0, d = 0, u = 0;
          for (let q = 0; q < i; q++) l |= has[q * Hh + j];
          for (let q = i + 1; q < W; q++) r |= has[q * Hh + j];
          for (let q = 0; q < j; q++) d |= has[i * Hh + q];
          for (let q = j + 1; q < Hh; q++) u |= has[i * Hh + q];
          col[i * Hh + j] = l && r && d && u ? 1 : 0;
        }
        const seen = new Uint8Array(W * Hh);
        for (let st = 0; st < W * Hh; st++) {
          if (!col[st] || seen[st]) continue;
          const comp = [st]; seen[st] = 1;
          let i0 = 1e9, i1 = -1, j0 = 1e9, j1 = -1;
          for (let q = 0; q < comp.length; q++) {
            const c = comp[q], i = Math.floor(c / Hh), j = c % Hh;
            i0 = Math.min(i0, i); i1 = Math.max(i1, i); j0 = Math.min(j0, j); j1 = Math.max(j1, j);
            for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const x = i + di, y = j + dj; if (x >= 0 && y >= 0 && x < W && y < Hh && col[x * Hh + y] && !seen[x * Hh + y]) { seen[x * Hh + y] = 1; comp.push(x * Hh + y); } }
          }
          for (let i = i0; i <= i1; i++) for (let j = j0; j <= j1; j++) for (let k = 0; k < nC[tax]; k++) sc[at(i, j, k)] = 0;
        }
      }
      const hitsBy = [0, 1, 2].map((axis) => castAxis(vx.V, s.t, s.m, axis, G, vx.lo, nC));
      Object.assign(vx, { solid: sc, n: nC, g: G, idx: idxC, hitsBy });
      // a notch or a tooth one block deep (four of its six neighbours disagree) goes too
      if (F >= 4) {          // on the thin pieces' finer blocks these would cut the piece itself
        lone += cleanLone(vx, 3, 4);
        lone += cleanSpecks(vx, 1);
        lone += cleanCombs(vx);
        lone += cleanLone(vx, 1, 4);
      } else {
        lone += cleanLone(vx, 2);
      }
    }
    // The faces of a part are cut by its planes exactly (as polygons), so two parts meet on the plane itself; its cells
    // (for collision) are cut by their middles.
    const faceSolid = part.planes ? Uint8Array.from(vx.solid) : null;
    if (part.planes) {
      // a part of a piece made in its own frame (a tower's wall at 45 degrees): keep the cells whose middle, in the
      // mesh's frame, lies on the kept side of every vertical plane [nx, ny, d]: nx x + ny y >= d
      const ya = ((po.yaw || 0) * Math.PI) / 180, c = Math.cos(ya), sn = Math.sin(ya);
      for (let x = 0; x < vx.n[0]; x++) for (let y = 0; y < vx.n[1]; y++) {
        const fx = vx.lo[0] + (x + 0.5) * vx.g, fy = vx.lo[1] + (y + 0.5) * vx.g;
        const mxp = fx * c - fy * sn, myp = fx * sn + fy * c;
        if (part.planes.every(([nx, ny, d]) => nx * mxp + ny * myp >= d)) continue;
        for (let z = 0; z < vx.n[2]; z++) vx.solid[vx.idx(x, y, z)] = 0;
      }
    }
    for (let i = 0; i < vx.solid.length; i++) cells += vx.solid[i];
    const cellSolid = vx.solid;
    if (faceSolid) vx.solid = faceSolid;
    const quads = faces(vx, s, po);
    vx.solid = cellSolid;
    const yaw = ((po.yaw || 0) * Math.PI) / 180, cs = Math.cos(yaw), sn = Math.sin(yaw);
    const back = (p) => [p[0] * cs - p[1] * sn, p[0] * sn + p[1] * cs, p[2]];   // frame -> mesh
    for (const [axis, dir, k, i0, i1, j0, j1, kind] of quads) {
      const ua = (axis + 1) % 3, va = (axis + 2) % 3;
      const sec = (sections[kind] = sections[kind] || { v: [], n: [], uv: [], t: [] });
      const corner = (i, j) => { const p = [0, 0, 0]; p[axis] = vx.lo[axis] + k * vx.g; p[ua] = vx.lo[ua] + i * vx.g; p[va] = vx.lo[va] + j * vx.g; return p; };
      let ps = [corner(i0, j0), corner(i1, j0), corner(i1, j1), corner(i0, j1)];
      const nrm = [0, 0, 0]; nrm[axis] = dir;
      const a0 = ps[1].map((x, q) => x - ps[0][q]), b0 = ps[2].map((x, q) => x - ps[0][q]);
      const cr0 = [a0[1] * b0[2] - a0[2] * b0[1], a0[2] * b0[0] - a0[0] * b0[2], a0[0] * b0[1] - a0[1] * b0[0]];
      if (cr0[axis] * dir <= 0) ps = [ps[0], ps[3], ps[2], ps[1]];      // wound counter-clockwise seen from outside
      if (part.planes) {
        // clip the face by each plane of the part, in the mesh's frame
        for (const [nx, ny, d] of part.planes) {
          const out = [];
          for (let q = 0; q < ps.length; q++) {
            const A = ps[q], B = ps[(q + 1) % ps.length];
            const ma = back(A), mb = back(B);
            const da = nx * ma[0] + ny * ma[1] - d, db = nx * mb[0] + ny * mb[1] - d;
            if (da >= 0) out.push(A);
            if ((da >= 0) !== (db >= 0)) { const t = da / (da - db); out.push(A.map((x, r) => x + (B[r] - x) * t)); }
          }
          ps = out;
          if (ps.length < 3) break;
        }
        if (ps.length < 3) continue;
      }
      const base = sec.v.length / 3;
      for (const p of ps) {
        // UV in metres of the working frame: walls read across and up (V down), floors and ceilings in plan
        let u, v;
        if (axis === 2) { u = p[0] / 100; v = -p[1] / 100 * dir; }
        else if (axis === 0) { u = -p[1] / 100 * dir; v = -p[2] / 100; }
        else { u = p[0] / 100 * dir; v = -p[2] / 100; }
        sec.v.push(...back(p).map((x) => Math.round(x * 1000) / 1000));
        sec.uv.push(Math.round(u * 10000) / 10000, Math.round(v * 10000) / 10000);
        sec.n.push(...back(nrm).map((x) => Math.round(x * 10000) / 10000));
      }
      for (let q = 1; q + 1 < ps.length; q++) sec.t.push(base, base + q, base + q + 1);
    }
    for (const [x0, y0, z0, x1, y1, z1] of boxes(vx)) {
      const c = [vx.lo[0] + (x0 + x1) / 2 * vx.g, vx.lo[1] + (y0 + y1) / 2 * vx.g, vx.lo[2] + (z0 + z1) / 2 * vx.g];
      boxList.push({ c: back(c), s: [(x1 - x0) * vx.g, (y1 - y0) * vx.g, (z1 - z0) * vx.g], yaw: po.yaw || 0 });
    }
  }
  const tris = Object.values(sections).reduce((a, s) => a + s.t.length / 3, 0);
  // too many boxes for simple collision: the importer uses the twin's own faces (few, flat) instead
  const out = { name, path: src.path, grid: opt.grid, sections, boxes: boxList.length <= opt.maxBoxes ? boxList : null, nboxes: boxList.length, tris };
  fs.writeFileSync(path.join(OUT, name + '.json'), JSON.stringify(out));
  return { name, srcTris: src.t.length / 3, tris, cells, snapped, lone, boxes: boxList.length, kinds: Object.keys(sections).join('+'), coarse: opt.coarse };
}

// A piece that is already plain and light (the crypt's vaults, the spiral stairs: a few hundred smooth triangles) keeps
// its own triangles; it only gets the tiles, laid on each triangle by the axis it faces most.
// A roof is a plain solid: the convex hull of a few points (a gable, a hip, a cone), in place of the pack's shingles.
// hull = [[x, y, z], ...] or {"cone": [cx, cy, z0, radius, z1, sides]}.
function hullTwin(name, src, opt) {
  let P = opt.hull;
  if (P.cone) {
    const [cx, cy, z0, r, z1, n] = P.cone;
    P = [[cx, cy, z1]];
    for (let i = 0; i < n; i++) { const a = (i + 0.5) / n * 2 * Math.PI; P.push([cx + r * Math.cos(a), cy + r * Math.sin(a), z0]); }
  }
  const kind = opt.tile || 'roof';
  const sec = { v: [], n: [], uv: [], t: [] };
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const planes = [];
  for (let i = 0; i < P.length; i++) for (let j = i + 1; j < P.length; j++) for (let k = j + 1; k < P.length; k++) {
    let nr = cross(sub(P[j], P[i]), sub(P[k], P[i]));
    const ln = Math.hypot(...nr);
    if (ln < 1e-6) continue;
    nr = nr.map((x) => x / ln);
    let pos = 0, neg = 0;
    for (const q of P) { const d = dot(nr, sub(q, P[i])); if (d > 0.5) pos++; else if (d < -0.5) neg++; }
    if (pos && neg) continue;
    if (pos) nr = nr.map((x) => -x);
    const d = dot(nr, P[i]);
    if (planes.some((p) => dot(p[0], nr) > 0.9999 && Math.abs(p[1] - d) < 0.5)) continue;
    planes.push([nr, d]);
  }
  for (const [nr, d] of planes) {
    const on = P.filter((q) => Math.abs(dot(nr, q) - d) < 0.5);
    const c = on.reduce((a, q) => a.map((x, i) => x + q[i] / on.length), [0, 0, 0]);
    const u0 = sub(on[0], c), u = u0.map((x) => x / Math.hypot(...u0)), w = cross(nr, u);
    on.sort((a, b) => Math.atan2(dot(sub(a, c), w), dot(sub(a, c), u)) - Math.atan2(dot(sub(b, c), w), dot(sub(b, c), u)));
    // tile laid on the face's own plane: across along its horizontal, down its slope
    let hx = cross([0, 0, 1], nr); if (Math.hypot(...hx) < 1e-3) hx = [1, 0, 0]; hx = hx.map((x) => x / Math.hypot(...hx));
    const dn = cross(nr, hx);
    const base = sec.v.length / 3;
    for (const q of on) {
      sec.v.push(...q.map((x) => Math.round(x * 1000) / 1000));
      sec.uv.push(Math.round(dot(q, hx) / 100 * 10000) / 10000, Math.round(-dot(q, dn) / 100 * 10000) / 10000);
      sec.n.push(...nr);
    }
    for (let q = 1; q + 1 < on.length; q++) sec.t.push(base, base + q, base + q + 1);
  }
  const sections = { [kind]: sec };
  const tris = sec.t.length / 3;
  fs.writeFileSync(path.join(OUT, name + '.json'), JSON.stringify({ name, path: src.path, grid: 0, sections, boxes: null, nboxes: 0, tris }));
  return { name, srcTris: src.t.length / 3, tris, cells: 0, snapped: 0, lone: 0, boxes: 0, kinds: kind + ' (hull)' };
}

// Each triangle turned to face out: a ray from its middle along its normal that crosses the piece an odd number of
// times started inside. The pack's meshes mix both windings (drawn two-sided, it never showed); lit with normals from
// the winding, the two halves of one wall face lit differently, a diagonal line across the stone (owner).
function orientOutward(V, T) {
  const out = T.slice();
  const n = T.length / 3;
  const P = (i) => [V[i * 3], V[i * 3 + 1], V[i * 3 + 2]];
  const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const tris = [];
  for (let k = 0; k < n; k++) tris.push([P(T[k * 3]), P(T[k * 3 + 1]), P(T[k * 3 + 2])]);
  const hits = (o, d, skip) => {
    let c = 0;
    for (let k = 0; k < n; k++) {
      if (k === skip) continue;
      const [a, b, cc] = tris[k];
      const e1 = sub(b, a), e2 = sub(cc, a), p = cross(d, e2), det = dot(e1, p);
      if (Math.abs(det) < 1e-9) continue;
      const iv = 1 / det, tv = sub(o, a), u = dot(tv, p) * iv;
      if (u < 0 || u > 1) continue;
      const q = cross(tv, e1), v = dot(d, q) * iv;
      if (v < 0 || u + v > 1) continue;
      if (dot(e2, q) * iv > 0.01) c++;
    }
    return c;
  };
  for (let k = 0; k < n; k++) {
    const [a, b, c] = tris[k];
    let nr = cross(sub(b, a), sub(c, a));
    const ln = Math.hypot(...nr);
    if (ln < 1e-9) continue;
    nr = nr.map((x) => x / ln);
    const m = [(a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3];
    // three slightly different rays vote (one may graze an edge)
    let inside = 0;
    for (const j of [[0.013, 0.007, 0.011], [-0.009, 0.012, -0.005], [0.004, -0.011, 0.014]]) {
      const d = [nr[0] + j[0], nr[1] + j[1], nr[2] + j[2]];
      if (hits([m[0] + nr[0] * 0.05, m[1] + nr[1] * 0.05, m[2] + nr[2] * 0.05], d, k) % 2 === 1) inside++;
    }
    if (inside >= 2) { out[k * 3 + 1] = T[k * 3 + 2]; out[k * 3 + 2] = T[k * 3 + 1]; }
  }
  return out;
}

function keepSource(name, src, opt) {
  const kinds = src.mats.map((m) => (opt.tiles && opt.tiles[m]) || opt.tile || tileFor(m));
  const sections = {};
  const V = src.v, T = src.t.length / 3 <= 12000 ? orientOutward(src.v, src.t) : src.t;
  for (let k = 0; k < T.length / 3; k++) {
    const P = [0, 1, 2].map((q) => { const i = T[k * 3 + q] * 3; return [V[i], V[i + 1], V[i + 2]]; });
    const e1 = P[1].map((x, i) => x - P[0][i]), e2 = P[2].map((x, i) => x - P[0][i]);
    const nr = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]];
    const ln = Math.hypot(...nr);
    if (ln < 1e-6) continue;
    const a = [0, 1, 2].reduce((b, i) => (Math.abs(nr[i]) > Math.abs(nr[b]) ? i : b), 0), dir = nr[a] > 0 ? 1 : -1;
    const kind = kinds[src.m[k]];
    const sec = (sections[kind] = sections[kind] || { v: [], n: [], uv: [], t: [] });
    const base = sec.v.length / 3;
    for (const p of P) {
      let u, v;
      if (a === 2) { u = p[0] / 100; v = -p[1] / 100 * dir; } else if (a === 0) { u = -p[1] / 100 * dir; v = -p[2] / 100; } else { u = p[0] / 100 * dir; v = -p[2] / 100; }
      sec.v.push(...p.map((x) => Math.round(x * 1000) / 1000));
      sec.uv.push(Math.round(u * 10000) / 10000, Math.round(v * 10000) / 10000);
      sec.n.push(...nr.map((x) => Math.round((x / ln) * 10000) / 10000));
    }
    sec.t.push(base, base + 1, base + 2);   // (turned to face out above, in this tool's winding)
  }
  const tris = Object.values(sections).reduce((x, q) => x + q.t.length / 3, 0);
  // collision: boxes merged from the piece's solid on the fine grid (the faces' own complex collision answered from one
  // side only once the winding was turned, and the player fell through the crypt's floors)
  let boxList = null;
  try {
    const vx = voxelize(src, Object.assign({}, DEF, { yaw: 0, grid: 6.25 }));
    cleanLone(vx, 1);
    const bx = boxes(vx);
    if (bx.length <= 3000) boxList = bx.map(([x0, y0, z0, x1, y1, z1]) => ({ c: [vx.lo[0] + (x0 + x1) / 2 * vx.g, vx.lo[1] + (y0 + y1) / 2 * vx.g, vx.lo[2] + (z0 + z1) / 2 * vx.g], s: [(x1 - x0) * vx.g, (y1 - y0) * vx.g, (z1 - z0) * vx.g], yaw: 0 }));
  } catch (e) { boxList = null; }
  fs.writeFileSync(path.join(OUT, name + '.json'), JSON.stringify({ name, path: src.path, grid: 0, sections, boxes: boxList, nboxes: boxList ? boxList.length : 0, tris, kept: true }));
  return { name, srcTris: T.length / 3, tris, cells: 0, snapped: 0, lone: 0, boxes: 0, kinds: Object.keys(sections).join('+') + ' (kept)' };
}

const names = ONLY.length ? ONLY : fs.readdirSync(SRC).filter((f) => f.endsWith('.json') && !f.startsWith('_')).map((f) => f.slice(0, -5));
for (const name of names) {
  if (CFG[name] && CFG[name].skip) continue;
  const t0 = Date.now();
  try {
    let r = build(name);
    // nothing gained: a plain piece of the pack that the grid would only make heavier keeps its own triangles
    if (!r.tris && !(CFG[name] && CFG[name].box)) r = keepSource(name, load(name), Object.assign({}, DEF, CFG[name] || {}));   // too thin for the grid (a lid, a floor tile): its own faces
    if (!r.kinds.includes('(kept)') && !r.coarse && r.tris > r.srcTris && r.srcTris <= 3000 && !(CFG[name] && (CFG[name].voxel || CFG[name].box))) {
      r = keepSource(name, load(name), Object.assign({}, DEF, CFG[name] || {}));
    }
    console.log(`${r.name.padEnd(44)} ${String(r.srcTris).padStart(7)} -> ${String(r.tris).padStart(5)} tris  boxes ${String(r.boxes).padStart(4)}  snapped ${r.snapped} lone ${r.lone}  ${r.kinds}  ${Date.now() - t0} ms`);
  } catch (e) {
    console.log(`${name}: FAILED ${e.stack}`);
  }
}
