// set_orc_armor.js - points the armed orcs of Enemies.json at the body worn under armor and at their armor set.
//   node Tools/Jam/set_orc_armor.js [--check]
// One orc type = body mesh + a list of armor pieces (SK_OrcArmor_<Set>_<Slot>, all on the orc's skeleton).
// Sets fitted by Tools/Blender/orc_armor_fit.py: A (plain military), C (bone and gold), D (green scales); set B was
// dropped by the owner on 2026-11-14, and so were the boot shafts (slot Shin).
// Who wears what (provisional, the architect's choice after set B, the chief's, was dropped):
const WEARS = {
  Orc_Future: 'A',              // the regular armed orc: the one sci-fi set
  Orc_Future_GunCarrier: 'A',   // the same orc, carrying the first gun
  Orc_Future_Enforcer: 'D',     // elite
  Orc_Future_Siege: 'D',        // the slow tank
  Orc_Future_Warlord: 'C',      // the horde chief: the set with the halo, worn by nobody else
};
const SLOTS = ['Helmet', 'Chest', 'Pelvis', 'Thigh', 'UpperArm', 'Shoulder', 'Hand', 'Foot'];
const BODY = '/Game/Glyphbound/Characters/Orc/SK_Orc_Armored';   // slimmer, no hands: the gauntlets are the hands
const ARMOR = '/Game/Glyphbound/Characters/Orc/Armor/SK_OrcArmor_';

const fs = require('fs');
const path = require('path');
const file = path.join(__dirname, '..', '..', 'Glyphbound', 'Content', 'Data', 'Enemies.json');
const text = fs.readFileSync(file, 'utf8');
// Only the two values of each orc are replaced, in the text: the file is hand-formatted and does not survive a
// JSON.parse / JSON.stringify round trip unchanged.
let out = text;
let n = 0;
const setValue = (from, to, key, value) => {
  const k = out.indexOf('"' + key + '": "', from);
  if (k < 0 || k > to) throw new Error('no ' + key + ' in the block at ' + from);
  const v0 = k + key.length + 5;
  const v1 = out.indexOf('"', v0);
  out = out.slice(0, v0) + value + out.slice(v1);
};
for (const [id, set] of Object.entries(WEARS)) {
  for (const [key, value] of [['Mesh', BODY], ['Armor', SLOTS.map(sl => ARMOR + set + '_' + sl).join(';')]]) {
    const at = out.indexOf('"Id": "' + id + '"');
    if (at < 0) throw new Error('no enemy ' + id);
    let end = out.indexOf('"Id": "', at + 5);
    if (end < 0) end = out.length;
    setValue(at, end, key, value);
  }
  n++;
}
JSON.parse(out);   // still valid JSON
if (process.argv.includes('--check')) { console.log('check ok:', n, 'orcs would be set'); process.exit(0); }
fs.writeFileSync(file, out);
console.log('set', n, 'orcs');
