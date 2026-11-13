# Shotgun (source art)

The source file for the shotgun: `model.glb`, a Tripo model. The owner handed it over on 2026-11-12 as `futuristic rifle 3d model.glb` (SHA-256 `E00DD166500378A4E4C40E6D40AC782527A4748D9FF96A6410FBF2E9E7F17622`, 4.8 MB), renamed here.

It replaces a first hand-over of the same model converted to FBX (`tripo_convert_8f45a6e1-07ca-492e-88d4-435d4c21e797.zip`, SHA-256 `9937173D2878063CC12DEBECE4AC8EA387A4D19B2708CCE99DA0B3AA7DA0C66D`), which had the same mesh and a 2048 JPG. That copy is deleted from the project (one copy per asset).

**The GLB as it comes** (`Tools/Blender/shotgun_inspect.py`, Blender 5.3.0, and the GLB's own JSON):
- one mesh, node `f2d360de_86b5_4ec0_a2ea_095a1dc4ec87`: 20,957 triangles; 25,482 vertices split along UV seams, 11,094 once welded at 0.01 mm; 58 loose pieces; 1,392 open edges and 133 non-manifold ones;
- 1.03 m long, 13 cm wide, 39 cm tall, muzzle towards -Y, up +Z, and **turned 11.3° about the vertical axis** (the bounding box reads 27 cm wide until it is turned back);
- one material with **one 4096 × 4096 JPG colour texture** (`baseColorTexture`) and nothing else: `metallicFactor` 0, `roughnessFactor` 0.9, no roughness, metallic, normal, AO or emissive map;
- the texture: 47.7% of the atlas used, 2,616 UV islands, an even 31 texels per cm; baked light, a frost-like painted wear, patches of different tone where projections meet; 2.6% of the gun's texels are glowing cyan (strips, screens, the cell);
- what the model shows: an over-under front (barrel over a tube), a ribbed forend under the muzzle, a glowing blue cell between the forend and the receiver, a screen on each side of the receiver, a circuit board on the left, a holographic sight with a rear window, a trigger inside its guard, a thumbhole stock, cyan glow strips on the handguard, on the stock's comb and inside the thumbhole, and cables.

**Pieces** (`Tools/Blender/shotgun_pieces.py`; numbers are the script's, sorted by vertex count; straightened frame, gun centred on X = 0):
- 0: the main body, which includes the cell and the forend (they have to be cut out of it);
- 1: the sight; 33: the trigger;
- cables, about 7,000 triangles in all: 3, 4, 5 (left of the receiver); 2, 10, 12, 23, 42 (left of the handguard); 6, 9, 13, 17, 20, 21 (right of the handguard); 14, 15, 18 (short loops beside the side screens).
- The cables are fused strands about 3 mm thick with 4 to 6 sides. They have no colour of their own: the body's grey-blue with copper-orange on 3% to 7% of their texels, in 176 to 241 UV islands per bundle; the body behind them carries their imprint.

**Review renders:** `Tools/Blender/shotgun_render_review.py` (it turns the gun back the 11.3° and nothing else). The ones kept are in `Devlog/media/2026-11-12/17a…17i_shotgun_*.png` (17a to 17d are of the FBX hand-over, the same mesh).

Nothing is imported into Unreal yet.
