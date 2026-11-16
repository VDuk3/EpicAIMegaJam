// Adds the audio rows to Devlog/SUBMISSION.md (one-off, idempotent).
const fs = require('fs'), p = 'Devlog/SUBMISSION.md';
let t = fs.readFileSync(p, 'utf8');
if (t.includes('Soundsnap')) process.exit(0);
const lines = t.split('\n');
let out = [];
for (const l of lines) {
  if (l.startsWith('| **fal**: ElevenLabs Music')) {
    out.push('| **fal**: ElevenLabs Music (used), Lyria 2 (failed with code 422), MiniMax Music 2 | Four boss music tracks (generated on ASSETS, 2026-11-13; prompts in `Devlog/raw/ASSETS/2026-11-13/cursor-66d9f2ce-*`). Terms of the generator: **owner to confirm** | **Yes, from 2026-11-15**: boss fights and the final arena (`music_boss_past`, `music_boss_future`, `music_entity` in `Content/Data/Audio.json`) | `Devlog/days/2026-11-15_audio_block.md` |');
  } else out.push(l);
}
t = out.join('\n');
const marker = '\n## 3. Prior work declared';
const row = `
### Sound effects and music (2026-11-15)

| Asset | Source | License | Where used | Evidence |
|---|---|---|---|---|
| 190 sound effects ("Medie bits" 60 WAV, "SciFiCoreSounds" 130 WAV; about 118 files used, converted to 44.1 kHz) | **Soundsnap** (https://soundsnap.com), confirmed by the team member who downloaded them. No per-file IDs, authors or license files were supplied | Soundsnap license: **owner to confirm the exact terms** (not audited) | Weapons, spells, hits, enemies, UI, doors, chests, footsteps, ambience: \`Content/Data/Audio.json\` | \`Devlog/days/2026-11-15_audio_block.md\`; \`Incoming/ASSETS/20261116-002002_Bibliotecas_Sonidos/MANIFEST_SHA256.json\` |
| Main theme ("la canción principal"): original acoustic-guitar composition by the team, 118 s | Made by hand by a team member: six guitar parts programmed in Guitar Pro 6, exported, effects added in Audacity; a couple of sound effects recorded on a phone from an acoustic guitar. Guitars only | Original work, made during the jam | Main menu and the PastCastle (game start) | \`Devlog/days/2026-11-15_audio_block.md\` |
`;
t = t.replace(marker, row + marker);
const tools = 'Blender 5.2 / 5.3,';
t = t.replace(tools, 'Guitar Pro 6, Audacity, Soundsnap (sound effects library), ' + tools);
fs.writeFileSync(p, t);
