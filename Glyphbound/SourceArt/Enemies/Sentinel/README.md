# Sentinel (future floating turret)

- `Centinel.glb` SHA-256 0a6f06aa647b0b70dac1d7a100eb4954b8ec9d28d8d41d623d9da78c22c0acc5
- Split into SM_Sentinel_Shell and SM_Sentinel_Core (the blue orb and its octagonal cage; pivot 12 cm behind its face so it rolls like an eyeball). Shell socket Core. 1 m sphere, front +X. The core glows through the shared material's emissive mask.

Exported by `Tools/Blender/enemies_export_fbx.py`, imported by `Tools/Jam/import_enemies.py` (one shared master material, `/Game/Glyphbound/Characters/Shared/M_GBEnemy`). Intermediates (FBX, PNG) live in a temporary stage folder that is deleted after import. Delivered by the ASSETS machine on 2026-11-13 (Tripo; provenance notes in `provenance/`). Moved here from `Incoming/ASSETS` on 2026-11-14 with a SHA-256 check.
