# Assault rifle (source art)

The source file for the assault rifle ("carabina roja"). The owner handed it over on 2026-11-12 as `carabina-roja.zip`, which held `model.glb` (a Tripo mesh, node `tripo_node_37918fc6`; SHA-256 `46E9206D59D058C79C0553A40A22DBC3571D2B0D9FE53ED628D1C775139A0C34`) and a preview. The zip was not copied into the project.

- **The GLB as it comes:** a low-poly export.
  - 5,498 triangles, split into about 1,000 loose panels, 1.0 m long, muzzle towards -Y, up +Z.
  - One 4096 albedo with baked light, smears and soft edges, and a `neon_mask`. No roughness, metallic, normal or AO map.
  - Top rail teeth of uneven length and spacing.
- **`Rifle.blend` is rebuilt from it** by `Tools/Blender/rifle_build_blend.py` (the first version was the plain GLB import, and the rifle read soft in the game):
  - SHA-256 `E2F459181DDB57C07CE9E0DC09FF4576E61DDBE59C95FCE168D8A4188216AE45` (Blender 5.3.0);
  - vertices welded, Tripo's normals dropped, hard edges above 30°, face-area weighted normals;
  - `T_Rifle_BaseColor`: one flat tone per panel (white polymer, two greys, dark metal) and the small drawn details re-cut crisp; the barrel, front sight, eyepiece and the band beside the rail are forced to dark metal as whole parts, because their panels came out as patches of different tones;
  - `T_Rifle_ORM`: AO baked in Cycles, roughness and metallic by tone;
  - `T_Rifle_Normal`: baked in Cycles (OpenGL): rounded edges and the drawn details pressed in;
  - `T_Rifle_EmissiveMask`: the neon mask with its edges re-cut, and without the GLB's ragged side strip on the handguard;
  - modelled by the script, in the flat metal slot `Rifle_Rail`:
    - the top rail: the old one deleted, 23 equal teeth (10 mm, 6.4 mm gaps), chamfered;
    - a raised white panel over the whole upper sloped face of the handguard on each side (atlas slot): the face has a stair-stepped groove 5 to 8 mm deep, the channel of the GLB's neon, and a metal block fills the notch it leaves in the ledge above;
    - an energy bar on each panel: a dark bezel 238 mm long with a straight glowing strip (the strip uses the atlas slot);
    - the ammo hologram's emitter at the foot of the sight's eyepiece: a low block with a glowing slit (the slit uses the atlas slot);
  - also modelled by the script: the hologram pane (38 × 19 mm, floating over the emitter, tilted 14° up towards the player) and the magazine (the GLB has none: a curved box, base plate and a glowing slit on each side);
  - objects `Rifle_Body`, `Rifle_Trigger`, `Rifle_Lens`, `Rifle_AmmoDisplay`, `Rifle_Magazine`. Pivots, the magazine's slide-out vector and the strip's span are custom properties on them, read by the export.
  - To rebuild: `blender -b --factory-startup --python Tools/Blender/rifle_build_blend.py -- <model.glb> Rifle.blend` (about 4 minutes, most of it the two bakes).
- **One rifle, one source** (owner's rule): `Rifle.blend` is the only source and `/Game/Glyphbound/Weapons/Rifle` the only result. `Tools/Jam/Import-Rifle.ps1` (Editor closed, pistol imported first):
  - exports the five parts and the four textures (`rifle_export_fbx.py`) into a temporary folder under `Saved/`;
  - imports them with their materials (`import_rifle.py`);
  - deletes the folder.
- The parts:
  - body, with the modelled rail, side panels, energy bars and hologram emitter;
  - trigger, picked by the box of its panels, origin at its hinge;
  - magazine, modelled;
  - weak-point light: the two oval lenses of the sight;
  - ammo display: a 2:1 hologram pane floating behind the sight's eyepiece, with its own material `M_RifleHolo`.
- Energy strip: in the body's second UV map (`ImbueU`):
  - U runs from 0 at the muzzle end of the energy bars' strips to 1 at their rear end;
  - in Blender, V = 1 on the strip and 0 off it, flipped by the import;
  - `M_RifleAtlas` cuts the strip into 13 cells that drain like the pistol's bar.
- `export_report.json` is written by each import: the offsets and motion the game code uses (`AGBGun`).
