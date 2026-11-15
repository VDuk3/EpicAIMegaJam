// Builds a cartoon first-person hand layer from simple shapes, in art pixels of the 160x90 canvas (3x scale).
// usage: node hand_shapes.js <shapes.json> <out_rows.txt>
// shapes.json: { "shapes": [ { "type": "capsule", "a": [x,y], "b": [x,y], "r": 2, "mat": "skin", "group": "hand" },
//                            { "type": "ellipse", "c": [x,y], "rx": 4, "ry": 5, "mat": "sleeve", "group": "sleeve" } ] }
// Shapes are painted in order. Each is shaded by its own surface normal against a light from the top left, and outlined
// in black where its group ends (so a shape over another of a different group leaves a crease line, one of the same
// group merges with it). Output: the full 160x90 character map (Make-HandLayer.ps1, x 0, y 0).
const fs = require('fs');
const [specPath, outPath] = process.argv.slice(2);
const spec = JSON.parse(fs.readFileSync(specPath, 'utf8'));
const W = 160, H = 90;
// Material ramps: [shadow, mid, light, highlight] palette chars.
const ramps = { skin: ['S', 'M', 'L', 'H'], sleeve: ['A', 'B', 'V', 'W'], cuff: ['B', 'V', 'W', 'W'] };
const L = [-0.6, -0.8];
const grid = Array.from({ length: H }, () => Array(W).fill('.'));
const owner = Array.from({ length: H }, () => Array(W).fill(null)); // group painted last at each pixel
function sample(s, x, y) { // returns normal (nx, ny) with |n| <= 1 when inside, else null; x, y = pixel centre
  if (s.type === 'ellipse') {
    const nx = (x - s.c[0]) / s.rx, ny = (y - s.c[1]) / s.ry;
    return nx * nx + ny * ny <= 1 ? [nx, ny] : null;
  }
  const [ax, ay] = s.a, [bx, by] = s.b;
  const dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy || 1;
  const t = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / len2));
  const px = x - (ax + t * dx), py = y - (ay + t * dy);
  return px * px + py * py <= s.r * s.r ? [px / s.r, py / s.r] : null;
}
const masks = spec.shapes.map(s => {
  const m = [];
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const n = sample(s, x + 0.5, y + 0.5); if (n) m.push([x, y, n]); }
  return m;
});
for (let i = 0; i < spec.shapes.length; i++) {
  const s = spec.shapes[i], ramp = ramps[s.mat];
  for (const [x, y, n] of masks[i]) {
    const v = -(n[0] * L[0] + n[1] * L[1]); // > 0 faces away from the light
    const light = -v + (s.lift || 0);
    grid[y][x] = light > 0.75 ? ramp[3] : light > 0.3 ? ramp[2] : light > -0.35 ? ramp[1] : ramp[0];
    owner[y][x] = s.group;
  }
}
const order = {}; // a group's paint order: the index of its last shape
spec.shapes.forEach((s, i) => { order[s.group] = i; });
// Outline: a pixel is black when a 4-neighbour belongs to another group or is empty (outline drawn inside the shape).
const out = grid.map(r => r.slice());
for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
  const g = owner[y][x];
  if (!g) continue;
  for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
    const xx = x + dx, yy = y + dy;
    if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue; // the screen edge is not outlined
    if (owner[yy][xx] !== g) { // only the upper shape draws the crease: the one painted later at this boundary
      const other = owner[yy][xx];
      if (!other || order[g] > order[other]) { out[y][x] = 'K'; break; }
    }
  }
}
fs.writeFileSync(outPath, out.map(r => r.join('')).join('\n') + '\n');
let minX = W, minY = H, maxX = -1, maxY = -1;
for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (out[y][x] !== '.') { minX = Math.min(minX, x); maxX = Math.max(maxX, x); minY = Math.min(minY, y); maxY = Math.max(maxY, y); }
console.log(`hand layer: bounds x ${minX}..${maxX}, y ${minY}..${maxY}`);
