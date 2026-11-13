# Shotgun (source art)

Two files: `model.glb`, the Tripo model as the owner handed it over, and `Shotgun.blend`, built from it. Nothing is imported into Unreal yet: the owner sees the renders first.

## `model.glb`
Handed over on 2026-11-12 as `futuristic rifle 3d model.glb` (SHA-256 `E00DD166500378A4E4C40E6D40AC782527A4748D9FF96A6410FBF2E9E7F17622`, 4.8 MB), renamed here. It replaces a first hand-over of the same model converted to FBX (`tripo_convert_8f45a6e1-07ca-492e-88d4-435d4c21e797.zip`, SHA-256 `9937173D2878063CC12DEBECE4AC8EA387A4D19B2708CCE99DA0B3AA7DA0C66D`), which had the same mesh and a 2048 JPG; that copy is deleted from the project.

As it comes (`Tools/Blender/shotgun_inspect.py`, Blender 5.3.0, and the GLB's own JSON):
- one mesh, node `f2d360de_86b5_4ec0_a2ea_095a1dc4ec87`: 20,957 triangles; 25,482 vertices split along UV seams, 11,094 once welded at 0.01 mm; 58 loose pieces; 1,392 open edges;
- 1.03 m long, 13 cm wide, 39 cm tall, muzzle towards -Y, up +Z, and **turned 11.3° about the vertical axis**;
- one material with **one 4096 × 4096 JPG colour texture** and nothing else: `metallicFactor` 0, `roughnessFactor` 0.9, no roughness, metallic, normal, AO or emissive map;
- the texture: 47.7% of the atlas used, 2,616 UV islands, an even 31 texels per cm; baked light, a frost-like painted wear, patches where projections meet;
- the cables (`Tools/Blender/shotgun_pieces.py`): real geometry, about 7,000 triangles, fused strands about 3 mm thick with 4 to 6 sides, with no colour of their own, and their imprint painted on the body behind them.

## `Shotgun.blend`
Built by `Tools/Blender/shotgun_build_blend.py` (80 to 90 s, most of it the two bakes):

`blender -b --factory-startup --python Tools/Blender/shotgun_build_blend.py -- model.glb Shotgun.blend`

SHA-256 `03104765D9E5BAF4EDB8917C912D813E0FE91FB8C5AC583C82525B081DCAE93E` (Blender 5.3.0). Frame: muzzle -Y, up +Z, X = 0 on the gun's centre line, metres.

- **Rebuilt on the same shape:** vertices welded, Tripo's normals dropped, hard edges above 30°, face-area weighted normals; one navy taken from the gun's own texels, with the frost, the baked light and the cables' imprint gone; small drawn details re-cut; the carbon panels found by their texture and their weave redrawn by position; a glow mask cut from the cyan; AO and a normal map baked in Cycles.
- **Cables:** split from the body, one colour per piece (black, copper orange, dark red, blue), smooth and subdivided once, 16,081 triangles.
- **Side plates:** the slab on each side of the receiver carried a painted data screen. Its face (102 × 65 mm, tilted 9° to 11°) is redrawn by position: an engraved border, a bolt in each corner, an abstract sigil inlaid with the glow (a diamond, a smaller one inside, a bar on each side) and three slanted slots.
- **Textures** (4096, packed in the file): `T_Shotgun_BaseColor`, `T_Shotgun_ORM`, `T_Shotgun_Normal` (OpenGL), `T_Shotgun_EmissiveMask`.
- **Modelled by the script** (measured with `Tools/Blender/shotgun_measure.py`):
  - the cell: the GLB's own lumpy cell is deleted (949 faces) and a canister sits in its bay on the same axis: a metal collar at each end sunk into the bay's leaning walls, a glass body 25.8 mm in radius that glows evenly, two metal bands, four guard rods, a saddle up to the barrel and a foot under each band;
  - the energy bar: a dark bezel 70 mm long with a straight glowing strip on the receiver block's top, behind the sight. The strip runs from y = 80.3 to 141.8 mm; the game cuts it into 13 cells;
  - the loading port: a framed dark opening 68 × 27 mm on the sloped underside in front of the trigger guard;
  - the pump: a ribbed U-shaped sleeve 62 mm long around the forend, which slides 42 mm back along the forend's slope (12.1°) and returns;
  - the ammo display: a 2:1 hologram pane, 50 × 25 mm, in the sight's rear window, tilted 8° up;
  - the weak-point light: a lens 34 × 7 mm on the sight's rear lower panel;
  - the shell: 19 mm across and 58 mm long, base to the rear, at rest inside the port.
- **Objects:** `Shotgun_Body` (31,536 triangles, with the cables, the cell, the energy bar and the port joined in), `Shotgun_Trigger` (111), `Shotgun_Pump` (5,688), `Shotgun_AmmoDisplay` (2), `Shotgun_WeakPointLight` (108), `Shotgun_Magazine` (the shell, 316; slots hull, base, glow).
- **Custom properties, for the export:** `pivot` on every object (the body's is the top of the grip; the trigger's its hinge; the pump's and the shell's their rest places), `slide` on the pump, `out` on the shell (its way out of the port), and on the scene `strip_y`, `strip_half_w`, `strip_zmin`.

**Review renders:** `Tools/Blender/shotgun_render_review.py` draws the ammo hologram the way the game does and can pose the pump and the shell. The ones kept are in `Devlog/media/2026-11-12/17a…17z_shotgun_*.png`.

**Known limits:** the carbon panels' outlines are rounded, not the source's sharp trapezoids; the small panel on the left of the receiver, near the stock, still shows one of the source's invented signs; the circuit board is mostly navy; the mesh still has open edges (a gap in the stock's comb is the model's own).

**Not written yet:** the export and import scripts (the rifle's are the template), and the game code for a pump and a shell-by-shell reload.
