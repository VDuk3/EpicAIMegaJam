# Shotgun (source art)

The source file for the shotgun. The owner handed it over on 2026-11-12 as `tripo_convert_8f45a6e1-07ca-492e-88d4-435d4c21e797.zip` (SHA-256 `9937173D2878063CC12DEBECE4AC8EA387A4D19B2708CCE99DA0B3AA7DA0C66D`), a Tripo model converted to FBX. The zip held two files, unpacked here:

- `tripo_convert_8f45a6e1-….fbx`, SHA-256 `74276EDC2704D24829DCC533B4B10367794F02E4D55396E9D6CB76D5DCA3D3FB`;
- `tripo_convert_8f45a6e1-….fbm/tripo_image_8f45a6e1-…_0_0.jpg`, SHA-256 `4303A2AF0E1D6AC0CB144BFBA44CEFE9E68745E8AB5F29C4B3ED57DFDC800B12`.

**The FBX as it comes** (`Tools/Blender/shotgun_inspect.py`, Blender 5.3.0):
- one mesh, node `tripo_node_8f45a6e1`: 11,094 vertices, 20,957 triangles, 58 loose pieces (already welded: merging at 0.01 mm changes nothing), 1,391 open edges and 134 non-manifold ones;
- 1.03 m long, 13 cm wide, 39 cm tall, muzzle towards -Y, up +Z, and **turned 11.3° about the vertical axis** (the bounding box reads 27 cm wide until it is turned back);
- one material and **one 2048 × 2048 JPG colour texture**, with baked light and a painted frost-like wear. No roughness, metallic, normal, AO or emissive map; the material is a flat roughness 0.9, metallic 0;
- what the model shows: an over-under front (barrel over a tube), a ribbed forend under the muzzle, a glowing blue cell between the forend and the receiver, a screen on each side of the receiver, exposed wires and a circuit board on the left, a holographic sight with a rear window, a trigger inside its guard, a thumbhole stock, and cyan glow strips on top of the handguard, on the stock's comb and inside the thumbhole.

**Review renders of the untouched source:** `Tools/Blender/shotgun_render_review.py` (it turns the gun back the 11.3° and nothing else). Four are in `Devlog/media/2026-11-12/17a…17d_shotgun_source_*.png`.

Nothing is imported into Unreal yet.
