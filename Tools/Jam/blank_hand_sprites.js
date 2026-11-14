// Usage: node blank_hand_sprites.js <Sprites.json in> <Sprites.json out>
// Makes hand_right, hand_left and hand_cast fully transparent (same size), so screenshots show the gun alone.
const fs = require('fs');
const [, , src, dst] = process.argv;
const data = JSON.parse(fs.readFileSync(src, 'utf8'));
let n = 0;
for (const s of data.Sprites) {
  if (['hand_right', 'hand_left', 'hand_cast'].includes(s.Id)) {
    s.Rows = s.Rows.map(r => '.'.repeat(r.length));
    n++;
  }
}
if (n !== 3) { console.error('expected 3 hand sprites, found ' + n); process.exit(1); }
fs.writeFileSync(dst, JSON.stringify(data, null, '\t'));
console.log('blanked ' + n + ' hand sprites -> ' + dst);
