// png_to_sprite.js - turns hand-drawn pixel PNGs (alpha 0/255) into Sprites.json entries (Palette + Rows), cropped to
// their opaque pixels. Usage: node Tools/Jam/png_to_sprite.js <ffmpeg.exe> <Sprites.json> id=file.png [id=file.png ...]
// An entry with the same Id is replaced; new ones are appended.
const fs = require('fs');
const { execFileSync } = require('child_process');
const [ff, spritesPath, ...pairs] = process.argv.slice(2);
const CHARS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
const doc = JSON.parse(fs.readFileSync(spritesPath, 'utf8'));
const list = doc.Sprites;
for (const pair of pairs) {
  const [id, file] = pair.split('=');
  const probe = execFileSync(ff, ['-i', file, '-f', 'null', '-'], { stdio: ['ignore', 'pipe', 'pipe'] }).toString();
  const info = execFileSync(ff.replace(/ffmpeg\.exe$/, 'ffprobe.exe'), ['-v', 'error', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', file]).toString().trim().split(',');
  const w = +info[0], h = +info[1];
  const raw = execFileSync(ff, ['-v', 'error', '-i', file, '-f', 'rawvideo', '-pix_fmt', 'rgba', '-']);
  let x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) if (raw[(y * w + x) * 4 + 3] > 127) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
  const pal = {}, rows = [];
  let n = 0;
  for (let y = y0; y <= y1; y++) {
    let r = '';
    for (let x = x0; x <= x1; x++) {
      const i = (y * w + x) * 4;
      if (raw[i + 3] <= 127) { r += '.'; continue; }
      const hex = '#' + [raw[i], raw[i + 1], raw[i + 2]].map(v => v.toString(16).padStart(2, '0').toUpperCase()).join('');
      if (!(hex in pal)) pal[hex] = CHARS[n++];
      r += pal[hex];
    }
    rows.push(r);
  }
  const Palette = {};
  for (const [hex, c] of Object.entries(pal)) Palette[c] = hex;
  const entry = { Id: id, Palette, Rows: rows };
  const k = list.findIndex(s => s.Id === id);
  if (k >= 0) list[k] = entry; else list.push(entry);
  console.log(id, (x1 - x0 + 1) + 'x' + (y1 - y0 + 1), n, 'colours');
}
fs.writeFileSync(spritesPath, JSON.stringify(doc, null, '\t') + '\n');
