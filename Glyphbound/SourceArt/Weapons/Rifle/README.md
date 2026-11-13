# Assault rifle (source art)

The source file for the assault rifle ("carabina roja"). The owner handed it over on 2026-11-12 as `carabina-roja.zip`, which held `model.glb` (a Tripo mesh, node `tripo_node_37918fc6`) and a preview. `Tools/Blender/rifle_import_glb.py` turned the GLB into a `.blend`, with its images packed inside. The zip was not copied into the project.

- `Rifle.blend`:
  - SHA-256 `6C7E671DC054EEDD4928F981E72DC48EAEC59B87AD035F6EBAD4A0852AD8D116` (Blender 5.3.0);
  - one mesh of about 1,000 loose panels, 5,498 triangles, 1.0 m long;
  - muzzle towards -Y, up +Z;
  - textures: a 4096 albedo (`Color_37918fc6...`) and `neon_mask`, which marks where the body glows.
- The source had no magazine. `rifle_export_fbx.py` models one (a curved box, base plate and a glowing slit on each side) in front of the trigger guard, under the magazine well.
- **One rifle, one source** (owner's rule): `Rifle.blend` is the only source and `/Game/Glyphbound/Weapons/Rifle` the only result. `Tools/Jam/Import-Rifle.ps1` (Editor closed, pistol imported first):
  - exports the five parts and the two textures (`rifle_export_fbx.py`) into a temporary folder under `Saved/`;
  - imports them with their materials (`import_rifle.py`);
  - deletes the folder.
- The parts:
  - body;
  - trigger, picked by the box of its panels, origin at its hinge;
  - magazine, new geometry;
  - weak-point light: the two oval lenses of the sight;
  - ammo display: a 2:1 hologram pane behind the sight's eyepiece.
- Energy strip: in the body's second UV map (`ImbueU`):
  - U runs from 0 at the muzzle end of the side neon strip to 1 at its rear end;
  - in Blender, V = 1 on the strip and 0 off it, flipped by the import;
  - `M_RifleAtlas` cuts the strip into 13 cells that drain like the pistol's bar.
- `export_report.json` is written by each import: the offsets and motion the game code uses (`AGBGun`).
