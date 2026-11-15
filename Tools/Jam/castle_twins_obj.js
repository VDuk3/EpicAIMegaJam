// castle_twins_obj.js - writes each twin of castle_twins.js (<out dir>/<name>.json) as a Wavefront OBJ with its
// normals, for the Editor's mesh importer: a static mesh built from a description in Editor Python got no normals at
// all (every one zero), and its lighting then depended on the view (dark wedges, walls lit from behind).
//   node Tools/Jam/castle_twins_obj.js <out dir> [name ...]
// Axes: the importer reads OBJ as right-handed and turns it into Unreal's frame; Unreal (x, y, z) is written as
// OBJ (x, -y, z), see AXES. One group and one material name per tile kind.
const fs = require('fs');
const path = require('path');
const [, , OUT, ...ONLY] = process.argv;
// measured: the importer reads OBJ (right-handed) into Unreal's left-handed frame by negating Y, so Y is negated here
// and each triangle's order is turned, which leaves Unreal with the same faces, the same winding and our normals
const AXES = [0, 1, 2, 1, -1, 1];
const names = ONLY.length ? ONLY : fs.readdirSync(OUT).filter((f) => f.endsWith('.json')).map((f) => f.slice(0, -5));
let n = 0;
for (const name of names) {
  const d = JSON.parse(fs.readFileSync(path.join(OUT, name + '.json'), 'utf8'));
  const map = (p) => [AXES[3] * p[AXES[0]], AXES[4] * p[AXES[1]], AXES[5] * p[AXES[2]]];
  const lines = ['# ' + name + ' (castle_twins.js)'];
  let vBase = 1, vtBase = 1, vnBase = 1;
  const body = [];
  for (const [kind, s] of Object.entries(d.sections)) {
    const nv = s.v.length / 3;
    for (let i = 0; i < nv; i++) {
      const p = map([s.v[i * 3], s.v[i * 3 + 1], s.v[i * 3 + 2]]);
      lines.push('v ' + p.map((x) => +x.toFixed(3)).join(' '));
    }
    for (let i = 0; i < nv; i++) lines.push('vt ' + s.uv[i * 2] + ' ' + (1 - s.uv[i * 2 + 1]));
    for (let i = 0; i < nv; i++) {
      const q = map([s.n[i * 3], s.n[i * 3 + 1], s.n[i * 3 + 2]]);
      lines.push('vn ' + q.map((x) => +x.toFixed(4)).join(' '));
    }
    body.push('g ' + kind, 'usemtl ' + kind);
    for (let k = 0; k < s.t.length; k += 3) {
      const tri = [s.t[k], s.t[k + 1], s.t[k + 2]];
      tri.reverse();
      body.push('f ' + tri.map((i) => (vBase + i) + '/' + (vtBase + i) + '/' + (vnBase + i)).join(' '));
    }
    vBase += nv; vtBase += nv; vnBase += nv;
  }
  fs.writeFileSync(path.join(OUT, name + '.obj'), lines.concat(body).join('\n') + '\n');
  n++;
}
console.log('obj: ' + n + ' twins');
