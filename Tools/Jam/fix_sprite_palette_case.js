// fix_sprite_palette_case.js - Unreal's JSON maps and FString keys ignore case, so a sprite palette that uses
// both 's' and 'S' loses one of the two colors (the pixels came out magenta). This renames every palette key
// that collides with another one ignoring case to a free digit or symbol, in the palette and in the rows.
// Usage: node Tools/Jam/fix_sprite_palette_case.js Glyphbound/Content/Data/Sprites.json   (safe to re-run)
const fs = require('fs');
const file = process.argv[2];
const data = JSON.parse(fs.readFileSync(file, 'utf8'));
const pool = '0123456789#@%&*+=?!$'.split('');
let renamed = 0;
for (const sprite of data.Sprites) {
  const keys = Object.keys(sprite.Palette);
  const seen = new Set();
  const used = new Set(keys.map(k => k.toLowerCase()));
  for (const key of keys) {
    const lower = key.toLowerCase();
    if (!seen.has(lower)) { seen.add(lower); continue; }
    const fresh = pool.find(c => !used.has(c));
    if (!fresh) throw new Error(`no free palette key left for sprite ${sprite.Id}`);
    used.add(fresh);
    const palette = {};
    for (const [k, v] of Object.entries(sprite.Palette)) palette[k === key ? fresh : k] = v;
    sprite.Palette = palette;
    sprite.Rows = sprite.Rows.map(row => row.split(key).join(fresh));
    renamed++;
  }
  const width = sprite.Rows[0].length;
  if (!sprite.Rows.every(r => r.length === width)) throw new Error(`ragged rows in sprite ${sprite.Id}`);
}
fs.writeFileSync(file, JSON.stringify(data, null, '\t') + '\n');
console.log(`sprites: ${data.Sprites.length}, palette keys renamed: ${renamed}`);
