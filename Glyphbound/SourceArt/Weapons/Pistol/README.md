# Pistol (source art) — v5

Master Blender file for the cyberpunk pistol. It came from ASSETS as v4 (Tripo task `50e44d0b`, edited there with
Codex and Cursor) and was finished on CENTRAL with the scripts in `Tools/Blender/` (see `Devlog/days/2026-11-12.md`,
ninth block). Older versions were deleted on CENTRAL; ASSETS keeps its own originals.

- `Pistol.blend` — SHA-256 `D571FE3E7EE13AEC8F69868F209489ED674A5AB4974AF218416A14803C5D3F81` (Blender 5.3.0).
  52,893 triangles. 0.97 m long in Blender units: scale on export. Muzzle points to -X; the energy bar is on -Y.
  Trigger: `Trigger_pivot_trial`, frames 1-16. Magazine: `TRIAL_Magazine_Control`, frames 24-72.
- `Textures/` — baked for Unreal by `pistol_prepare_ue.py`: `T_Pistol_BaseColor` (sRGB), `T_Pistol_ORM`
  (R = AO, G = roughness, B = metallic), `T_Pistol_Normal` (DirectX), `T_Pistol_EmissiveMask` (where the body glows
  in the element colour).
- Energy bar `V4_energy_cells`: UV map `ImbueU`, U = 0 at the muzzle end, 1 at the grip end; light cells where
  U < ImbueRemaining.
- Element colour (one mesh, one material parameter): green `(0.02, 1.0, 0.07)`, purple `(0.32, 0.015, 1.0)`
  (linear). The purple .blend was the same file with the other value and was not kept.
- `clean_log.txt` — the regions removed from the atlas and the donor offset of each.

- `Export/` — `pistol_export_fbx.py` writes `SM_Pistol_{Body,Trigger,Magazine,EnergyBar}.fbx` (rest pose, muzzle +X,
  bar on -Y in Unreal, body origin at the hand point, trigger and magazine at their pivots) and `export_report.json`
  (offsets and motion the game code uses). The old lens faces (`Optic_OriginalLens_Invisible`) are dropped.
- Into Unreal: `Tools/Jam/Import-Pistol.ps1` (Editor closed) imports them to `/Game/Glyphbound/Weapons/Pistol` with
  their textures and materials. Re-run it after any export.

Still to do: lens and reticle rebuild; check the moving parts from the real game camera (the trigger's saw teeth are
accepted only if the player cannot see them).
