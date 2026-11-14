# Robot (future soldier)

- `Robot.glb` SHA-256 3ffacb9c63873340c69be93033ece7485c6908cabdc32a50b8b3c335ca72eae3
- Rigged by Tripo on the UE5 mannequin bone names (80 bones), no clips. Scaled to 1.70 m, feet at 0.
- Animations: the UE template mannequin's rifle set, retargeted with an IK retargeter (`Tools/Jam/retarget_enemies.py`).

Exported by `Tools/Blender/enemies_export_fbx.py`, imported by `Tools/Jam/import_enemies.py` (one shared master material, `/Game/Glyphbound/Characters/Shared/M_GBEnemy`). Intermediates (FBX, PNG) live in a temporary stage folder that is deleted after import. Delivered by the ASSETS machine on 2026-11-13 (Tripo; provenance notes in `provenance/`). Moved here from `Incoming/ASSETS` on 2026-11-14 with a SHA-256 check.
