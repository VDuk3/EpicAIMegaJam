# Robot spider (future swarm enemy)

- `Robot_Spider.glb` SHA-256 092a0c933bd5685910cd656c6d442cecb9f962f1dd1cd229e0bcfa3a90adf55f (no skeleton, on a display base)
- The base is dropped. Rig made by the export script: root, body and two bones per leg (six legs), hard weights by leg angle and nearest segment (rigid machine segments). Clips keyed by the script: Idle, Walk (alternating tripods), Attack (rear and strike), Hit, Death (flips onto its back).

Exported by `Tools/Blender/enemies_export_fbx.py`, imported by `Tools/Jam/import_enemies.py` (one shared master material, `/Game/Glyphbound/Characters/Shared/M_GBEnemy`). Intermediates (FBX, PNG) live in a temporary stage folder that is deleted after import. Delivered by the ASSETS machine on 2026-11-13 (Tripo; provenance notes in `provenance/`). Moved here from `Incoming/ASSETS` on 2026-11-14 with a SHA-256 check.
