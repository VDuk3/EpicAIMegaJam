// castle_plan_svg.js - draws a level written by castle_read.py as a top-down plan (SVG): every static mesh as its
// footprint (the mesh's own bounding box, turned and scaled as the piece is placed), coloured by the height of its
// base, with level instances as labelled crosses. It is a reading aid: it shows how the pack's author put the
// castle together, to scale, before anything of ours is drawn over it.
//
//   node Tools/Jam/castle_plan_svg.js <level.json> <pieces.json> <out.svg> [--zmin n] [--zmax n] [--scale cm-per-px]
//        [--labels] [--match text]
//   --zmin/--zmax  keep only pieces whose base is inside this height range (cm): one storey at a time
//   --labels       write each piece's name at its centre (use with a z range, or the plan is unreadable)
const fs = require('fs');

const args = process.argv.slice(2);
function opt(name, def) {
  const i = args.indexOf(name);
  if (i < 0) return def;
  const v = args[i + 1];
  args.splice(i, 2);
  return v;
}
function flag(name) {
  const i = args.indexOf(name);
  if (i < 0) return false;
  args.splice(i, 1);
  return true;
}
const zmin = parseFloat(opt('--zmin', '-1e9'));
const zmax = parseFloat(opt('--zmax', '1e9'));
const scale = parseFloat(opt('--scale', '10'));
const match = opt('--match', '');
const labels = flag('--labels');
const [levelFile, piecesFile, outFile] = args;
const level = JSON.parse(fs.readFileSync(levelFile, 'utf8'));
const pieces = {};
for (const p of JSON.parse(fs.readFileSync(piecesFile, 'utf8'))) pieces[p.mesh] = p;

// Unreal: X forward, Y right, yaw turns X towards Y. Drawn with +X up and +Y right, like a map with north at +X.
function footprint(it) {
  const p = pieces[it.mesh];
  if (!p) return null;
  const yaw = (it.rot[1] * Math.PI) / 180;
  const c = Math.cos(yaw), s = Math.sin(yaw);
  const corners = [[p.min[0], p.min[1]], [p.max[0], p.min[1]], [p.max[0], p.max[1]], [p.min[0], p.max[1]]];
  const pts = corners.map(([x, y]) => {
    const sx = x * it.scale[0], sy = y * it.scale[1];
    return [it.loc[0] + sx * c - sy * s, it.loc[1] + sx * s + sy * c];
  });
  const z0 = it.loc[2] + Math.min(p.min[2] * it.scale[2], p.max[2] * it.scale[2]);
  const z1 = it.loc[2] + Math.max(p.min[2] * it.scale[2], p.max[2] * it.scale[2]);
  return { pts, z0, z1, name: it.mesh.split('/').pop() };
}

const shapes = [];
const unknown = {};
for (const it of level.items) {
  if (it.kind !== 'mesh') continue;
  if (match && !it.mesh.includes(match)) continue;
  const f = footprint(it);
  if (!f) { unknown[it.mesh] = (unknown[it.mesh] || 0) + 1; continue; }
  if (f.z0 < zmin || f.z0 > zmax) continue;
  shapes.push(f);
}
const lis = level.items.filter((it) => it.kind === 'level_instance');
let x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9, zl = 1e9, zh = -1e9;
for (const f of shapes) {
  for (const [x, y] of f.pts) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
  zl = Math.min(zl, f.z0); zh = Math.max(zh, f.z0);
}
for (const it of lis) { x0 = Math.min(x0, it.loc[0]); x1 = Math.max(x1, it.loc[0]); y0 = Math.min(y0, it.loc[1]); y1 = Math.max(y1, it.loc[1]); }
const pad = 300;
x0 -= pad; x1 += pad; y0 -= pad; y1 += pad;
const W = (y1 - y0) / scale, H = (x1 - x0) / scale;
const px = (x, y) => [((y - y0) / scale).toFixed(1), ((x1 - x) / scale).toFixed(1)];
function colour(z) {
  const t = zh > zl ? (z - zl) / (zh - zl) : 0;
  const hue = 220 - 220 * t; // low = blue, high = red
  return `hsl(${hue.toFixed(0)},70%,55%)`;
}
let svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${W.toFixed(0)}" height="${(H + 40).toFixed(0)}" viewBox="0 0 ${W.toFixed(0)} ${(H + 40).toFixed(0)}" font-family="Consolas,monospace">\n`;
svg += `<rect width="100%" height="100%" fill="#181818"/>\n`;
const grid = 1000;
for (let gx = Math.ceil(x0 / grid) * grid; gx <= x1; gx += grid) {
  const [, y] = px(gx, 0);
  svg += `<line x1="0" y1="${y}" x2="${W.toFixed(0)}" y2="${y}" stroke="#333" stroke-width="${gx === 0 ? 2 : 1}"/><text x="2" y="${(+y - 2).toFixed(0)}" fill="#666" font-size="10">x ${gx / 100} m</text>\n`;
}
for (let gy = Math.ceil(y0 / grid) * grid; gy <= y1; gy += grid) {
  const [x] = px(0, gy);
  svg += `<line x1="${x}" y1="0" x2="${x}" y2="${H.toFixed(0)}" stroke="#333" stroke-width="${gy === 0 ? 2 : 1}"/><text x="${(+x + 2).toFixed(0)}" y="${(H - 2).toFixed(0)}" fill="#666" font-size="10">y ${gy / 100} m</text>\n`;
}
shapes.sort((a, b) => a.z0 - b.z0);
for (const f of shapes) {
  svg += `<polygon points="${f.pts.map(([x, y]) => px(x, y).join(',')).join(' ')}" fill="${colour(f.z0)}" fill-opacity="0.35" stroke="${colour(f.z0)}" stroke-width="1"><title>${f.name} z ${f.z0.toFixed(0)}..${f.z1.toFixed(0)}</title></polygon>\n`;
  if (labels) {
    const cx = f.pts.reduce((a, p) => a + p[0], 0) / 4, cy = f.pts.reduce((a, p) => a + p[1], 0) / 4;
    const [tx, ty] = px(cx, cy);
    svg += `<text x="${tx}" y="${ty}" fill="#eee" font-size="9" text-anchor="middle">${f.name.replace(/^SM_/, '')} ${f.z0.toFixed(0)}</text>\n`;
  }
}
for (const it of lis) {
  const [x, y] = px(it.loc[0], it.loc[1]);
  const n = (it.level || '?').split('/').pop().split('.')[0];
  svg += `<path d="M${+x - 8} ${y}H${+x + 8}M${x} ${+y - 8}V${+y + 8}" stroke="#fff" stroke-width="2"/><text x="${+x + 10}" y="${+y + 4}" fill="#fff" font-size="12">${n} z ${it.loc[2]} yaw ${it.rot[1]}</text>\n`;
}
svg += `<text x="6" y="${(H + 26).toFixed(0)}" fill="#ccc" font-size="13">${level.level.split('/').pop()}  |  ${shapes.length} pieces  |  base height ${zl.toFixed(0)} (blue) .. ${zh.toFixed(0)} cm (red)  |  grid 10 m  |  1 px = ${scale} cm</text>\n</svg>\n`;
fs.writeFileSync(outFile, svg);
console.log(`${outFile}: ${shapes.length} pieces, ${lis.length} level instances, x ${x0.toFixed(0)}..${x1.toFixed(0)}, y ${y0.toFixed(0)}..${y1.toFixed(0)}, z ${zl.toFixed(0)}..${zh.toFixed(0)}`);
const u = Object.keys(unknown);
if (u.length) console.log('not in pieces.json: ' + u.map((k) => `${k.split('/').pop()} x${unknown[k]}`).join(', '));
