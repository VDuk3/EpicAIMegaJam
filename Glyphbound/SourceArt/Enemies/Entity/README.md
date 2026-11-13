# The Entity, phase 1 "the machine" (source art)

The owner generated several Tripo variants of the prompt "un ente mecánico futurista tipo villano…" ("a futuristic mechanical entity, villain type") and handed them over on 2026-11-12 (`tripo_convert_020f04d5….zip`, then `nasry kum.zip` with seven more zips: variants #13, #26, #36, #53, #71, #78, two of them duplicates). The owner picked **#26**. `Tools/Blender/entity_import_fbx.py` turned its FBX into `Entity.blend`: the textures relinked (the zip had broken the accents in their file names), packed inside, and Tripo's Y-up rotation applied. The zips were not copied into the project.

- `Entity.blend`:
  - one fused mesh, 19,879 triangles, 9,935 vertices, about 1 m tall; eye towards -Y, up +Z;
  - textures: a 4096 albedo and a 2048 normal map (OpenGL).
- **One enemy, one source:** `Entity.blend` is the only source and `/Game/Glyphbound/Enemies/Entity` the only result. `Tools/Jam/Import-Entity.ps1` (Editor closed):
  - splits and exports the parts and the textures (`Tools/Blender/entity_export_fbx.py`) into a temporary folder under `Saved/`;
  - imports them and builds the materials (`Tools/Jam/import_entity.py`);
  - deletes the folder.
- **The split** (owner's direction: metal shell, eyes as electronic lights, simplified hologram tentacles):
  - body: faces inside a tilted ellipsoid fitted to the shell. Then every UV chart that is at least 30% outside the shell goes to the tentacles whole, so each tentacle keeps its root (owner: the first version left about 5% of every tentacle in the metal). Islands under 150 faces go to the other side;
  - eye: body faces whose UVs fall on the eye's islands of the albedo, kept only at the front of the shell. It is a separate mesh so it can be the weak point;
  - tentacles: the rest, decimated to 30% (11,094 to 3,328 triangles). UV 1 `Sway` is U = 0 at the root and 1 at the tip, V = a random phase per tentacle (11 tentacles).
- **Export frame:** scaled x3 (about 2.9 m tall, provisional), turned so the eye looks to +X, origin at the centre of the shell.
- **Materials:**
  - `M_EntityBody`: the albedo desaturated into gunmetal, metallic 0.9, polished plates and rougher panel lines, normal map. The albedo's blue lights (`T_Entity_GlowMask`) glow with a slow pulse and a slight flicker. The parameter `Flash` is for weak-point hits.
  - `MI_EntityEye`: the same material, about three times brighter.
  - `M_EntityHologram` and `MI_EntityTentacles`: the tentacles, using the owner's Fab "Hologram Shader". `M_EntityHologram` is a copy of the pack's `M_Hologram` with a sway added as world position offset. `MI_EntityTentacles` is a copy of `MI_Hologram_7` recoloured green, with Hologram Intensity raised from 0.65 to 2.5. The pack itself is not modified.
- `export_report.json` is written by each import: triangle counts, bounds in cm, and the eye's centre and radius in cm (actor frame) for the weak-point code.
