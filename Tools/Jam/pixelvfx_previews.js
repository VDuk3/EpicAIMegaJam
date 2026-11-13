// pixelvfx_previews.js - Makes an animated GIF of every baked pixel VFX clip, for people to look at
// (the dev log, the owner's review). Reads Glyphbound/Content/Data/Vfx/<Id>.json + .png, writes
// <outDir>/pixelvfx_<Id>.gif: the clip at its own frame rate, upscaled with nearest filtering on a dark background.
// Needs ffmpeg on PATH. Usage: node Tools/Jam/pixelvfx_previews.js [outDir] [scale]
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '..', '..');
const vfxDir = path.join(root, 'Glyphbound', 'Content', 'Data', 'Vfx');
const outDir = path.resolve(process.argv[2] || path.join(root, 'Glyphbound', 'Saved', 'PixelVfx', 'Previews'));
const scale = parseInt(process.argv[3] || '6', 10);
fs.mkdirSync(outDir, { recursive: true });

for (const file of fs.readdirSync(vfxDir).filter(f => f.endsWith('.json')).sort()) {
  const clip = JSON.parse(fs.readFileSync(path.join(vfxDir, file), 'utf8'));
  const atlas = path.join(vfxDir, clip.Atlas);
  if (!fs.existsSync(atlas)) { console.log('skip ' + clip.Id + ': no atlas'); continue; }
  const rows = Math.ceil(clip.FrameCount / clip.Columns);
  const w = clip.FrameWidth * scale, h = clip.FrameHeight * scale;
  const out = path.join(outDir, 'pixelvfx_' + clip.Id + '.gif');
  // untile cuts the atlas into frames; the dark backdrop replaces transparency (GIF alpha is ugly);
  // palettegen/paletteuse keep the exact palette colors (no dithering).
  const filter =
    `[0:v]untile=${clip.Columns}x${rows},setpts=N/(${clip.Fps}*TB),scale=${w}:${h}:flags=neighbor[fg];` +
    `color=c=0x16161C:s=${w}x${h}:r=${clip.Fps}[bg];[bg][fg]overlay=shortest=1,trim=end_frame=${clip.FrameCount},split[a][b];` +
    `[a]palettegen=max_colors=32:reserve_transparent=0[p];[b][p]paletteuse=dither=none`;
  execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-i', atlas, '-filter_complex', filter, '-r', String(clip.Fps), '-loop', '0', out]);
  console.log(clip.Id + ' -> ' + out + ' (' + clip.FrameCount + ' frames, ' + fs.statSync(out).size + ' bytes)');
}
