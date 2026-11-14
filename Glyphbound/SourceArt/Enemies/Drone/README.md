# Drone (future swarm enemy)

- `Drone.glb` SHA-256 68ba9d7ed6cc6350f9f0ac65201d3c933d4e94a173bddd296ee6fe8cd0a8fb9e
- One fused mesh; split by connected islands into SM_Drone_Body, SM_Drone_Eye (the red-lens ball, pivot at its centre) and SM_Drone_Rotor_0..3 (hub + blades, pivot at the hub). Body sockets Rotor_0..3 and Eye. 80 cm rotor to rotor. Front turned to +X.

Exported by `Tools/Blender/enemies_export_fbx.py`, imported by `Tools/Jam/import_enemies.py` (one shared master material, `/Game/Glyphbound/Characters/Shared/M_GBEnemy`). Intermediates (FBX, PNG) live in a temporary stage folder that is deleted after import. Delivered by the ASSETS machine on 2026-11-13 (Tripo; provenance notes in `provenance/`). Moved here from `Incoming/ASSETS` on 2026-11-14 with a SHA-256 check.
