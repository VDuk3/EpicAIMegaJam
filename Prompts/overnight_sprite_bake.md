# Overnight block B: the pixel-art sprite baker (owner's overnight point 5, castle enemies)

## The owner's words (Spanish, translated)
"Orcs: give them combat animations, even simple ones. No static sprites walking at the player like cardboard cut-outs.
New enemies, typical of first-person medieval-fantasy pixel-art games and 90s RPGs: varied but not too many, big and
small, some tougher than others, some attacking from a distance, each with fitting animations and VFX. Final boss of
the castle: the pixel-art Valkyrie." And standing rules: **pixel art = clean plain blocks, no specks or faults; clean
without mercy**; "we do not want a game that looks cheap".

## The decision (architect)
The castle's characters are **rendered from animated 3D models and reduced to pixel art**, the way many 90s games made
their sprites (pre-rendered, 8 view directions), and like this project already bakes Niagara fire into pixel flipbooks
(`Glyphbound/Plugins/GBPixelVfx/README.md`: read its "Pixel" pipeline: downsample, palette ramps, hard alpha,
hysteresis, island/hole/speck cleanup, outline). You write the baker in **Blender** (headless, Python + numpy):
`"C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe" -b --python <script> -- <args>`.

Read: `Prompts/sprite_sheet_format.md` (the output contract, binding), `Design/VisualStyleGuide.md` (outline, light
from the top left, ramps, master palette, colour budget: characters up to 24 colours), the hand-drawn Valkyrie
`Devlog/media/2026-11-12/16_pixelart_valki.png` (75 x 93 px; the boss's look and the scale reference), and
`Tools/Blender/enemies_export_fbx.py` (how this project loads the same characters in Blender; frames, cm).

## Inputs (already exported from Unreal)
`Glyphbound/Saved/bake_src/` with `manifest.json`: per character the skeletal mesh FBX files (Unreal export, cm), one
FBX per clip (skeleton + animation, same bone names as the mesh), and each material slot's base-colour PNG.
- `Orc/`: `SK_Orc` (2.10 m; its texture paints a dark undersuit on the body, only the head is skin) and clips
  `A_Orc_*` (idle, walks, jog, staff strike, zombie attack, punches, charged attack, staff shot, cast spell, magic
  strike, hit react, three deaths).
- `Valkyrie/`: `SK_Valkyrie` (1.95 m, big mechanical wings now rigid on the chest bone) and clips `A_Valkyrie_*`.
- `Barghest/`: a demon hound (body, mane, tail meshes) and its clips.
- `Centaur/`: the centaur body and its gear meshes (armor, shoulder pads, hair pieces, bow idle/action, arrow) and
  bow clips.
- `Weapons/`: static meshes `SM_Mace`, `SM_Axe`, `SM_Staff`, `SM_Sword_1`, `SM_Shield_1`, `SM_Spear`, `SM_Bow`,
  `SM_ProjectileArrowMesh`, `SM_ValkyrieSword` with textures.
If something you need is missing, say so in your report; do not invent assets.

## Sheets to make (provisional design, architect; adjust clip choices by what looks right)
| SheetId | From | Weapon | Clips (sheet name: source) | Look |
|---|---|---|---|---|
| `OrcWarrior` | SK_Orc, 2.10 m | mace in the right hand | Idle, Walk, Run, Attack (a heavy swing: StaffStrike or Attack_01, pick the one that reads), Attack2, Hurt, Death | classic green fantasy orc: green skin ramp (add one, hue-shifted), the dark undersuit read as leather armour (brown ramp), iron mace |
| `OrcBrute` | SK_Orc scaled 1.35 | axe | Idle, Walk, Attack (ChargedAttack or a slow overhead), Hurt, Death | darker green, red war-paint accent, bigger and slower; the castle's tank |
| `OrcShaman` | SK_Orc | staff | Idle, Walk, Cast (StaffShot / CastSpell), Attack (MagicStrike, close range), Hurt, Death | robe-like brown/purple, arcane accent ramp on the staff head |
| `Barghest` | Barghest meshes | - | Idle, Walk, Run, Attack (bite), Leap (jump bite), Hurt, Death | a black-and-ember hellhound: fast and fragile |
| `CentaurArcher` | Centaur + armour + bow | its bow (action mesh) | Idle, Walk, Run (gallop), Shoot (shoot arrow, with `HitFrame` = arrow release and `Origin`), Attack (leg kick), Hurt, Death | big ranged enemy, chestnut horse body, iron and leather gear |
| `ValkyriePixel` | SK_Valkyrie | her sword (`SM_ValkyrieSword` on `hand_r`; the game's offset is `-0.9 -10.2 0.1` cm and pitch -36, yaw 163, roll 96.7, Unreal frame: convert carefully and check it in a render) | Idle, Walk, Run, Attack, Attack2, Cast (CastSpell), Leap (LevitatingMagicStrike or Ascension), Hurt, Death, plus `Dormant` (1 frame, a still pose) | **match the hand-drawn `valki` sprite**: red hair, deep-blue armour (`Valki's armor` ramp), bone/ivory wings, skin ramp; about 93 px tall like the drawing at the same pixel density as the others |

Frames: 6 to 10 fps; walk cycles 6 to 8 frames, attacks 5 to 7, hurt 2, death 5 to 7 (last frame = lying still).
Death and attack clips may carry root motion: keep the character over its pivot (cancel horizontal root/pelvis travel)
except where a lunge must read.

## The pixel pipeline (what "good" means)
- Render each frame from the 8 directions with an orthographic camera at a slight downward pitch (about 10 to 15
  degrees: the player looks at them from eye height), key light from the **top left of the view** (the same for every
  direction: the light is part of the drawing), soft fill so the shadow side is not black.
- Render at 4x or more and reduce to the target size (PixelsPerMeter 44 for all; a 2.10 m orc is about 92 px tall).
- Colours: each sheet uses at most 24 colours, built from the master palette's ramps (style guide section 6) plus the
  ramps you must add (orc skin green, hound black-ember, horse chestnut): write the added ramps in your report so the
  architect adds them to the style guide. Map by material/region where you can (an ID pass by material slot, or by
  bone region / mesh island for single-material meshes such as the Valkyrie: wings, hair, skin, armour), with the
  shading luminance choosing the tone inside the ramp; this gives clean flat bands, not dithered photo noise.
- Hard alpha; 1 px black outer outline; inner outlines with the ramp's darkest tone where parts overlap (optional).
- Cleanup: no isolated single pixels, no 1-pixel holes, no speckle; stable from frame to frame (no flicker of
  tones on a still part).
- Weapons must read at this size (a mace head is a clear block of iron, not 2 grey pixels).

## Review yourself before reporting (mandatory)
For each sheet write `Glyphbound/Saved/SpriteBake/<SheetId>/contact.png` (every frame of every clip, all 8 directions,
scaled up 4x with nearest neighbour, on a mid-grey background, labelled) and an animated GIF of each clip at d=0
and d=2 (ffmpeg is installed). **Look at them with your image reader** and fix what is wrong: muddy colours, broken
silhouettes, a weapon that vanishes, a direction that does not match its column, feet that slide off the pivot,
flicker. Iterate until each sheet would pass for hand-made 90s sprites. Report what you fixed.

## Deliverables
- `Tools/Blender/sprite_bake.py` (+ helpers) and one job file per sheet in `Tools/Blender/sprite_jobs/<SheetId>.json`
  (everything a sheet needs: sources, scale, weapon attach, clip list and frame sampling, palette ramps, region rules).
  One command bakes one sheet or all: `blender -b --python Tools/Blender/sprite_bake.py -- <SheetId|All>`.
- `Glyphbound/Content/Data/Sprites/<SheetId>.png` + `.json` for the six sheets.
- Order: `OrcWarrior` end to end first (it proves the pipeline), then `ValkyriePixel`, `Barghest`, `CentaurArcher`,
  `OrcShaman`, `OrcBrute`. If time runs short, a finished subset beats six rough ones.

## Rules
- Do not touch Unreal (no Editor, no build): another agent builds the game. Do not edit files outside
  `Tools/Blender/sprite_*`, `Tools/Blender/sprite_jobs/`, `Glyphbound/Content/Data/Sprites/`, `Glyphbound/Saved/SpriteBake/`.
- Do not commit. Blender runs are fine at any time (they do not need the machine lock), but keep one Blender process
  at a time.
- English in code and comments, ASCII only.
- Final report: files; per sheet: cell size, clips and frames, colour count, the ramps you added; what you fixed in
  self-review; what still looks weak.
