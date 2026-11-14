# Orc and its modular armor

- `Base_Rigged_Orc.glb` SHA-256 7e30cc12bbf51bcb4a6666b25b4a08168fb65c33e20d8b53087607b9d8d01726 (body, 80 bones, UE5 mannequin names)
- `Orc_Armor_1.glb` SHA-256 771baa2b50f8394f0c92b0c428de1a00a93f0f9fb86c7061eb3e5d49668726e8 (line-up: helmet, chest, one gauntlet, one boot x 4 sets)
- `Orc_Armor_2.glb` SHA-256 624ad51702a26950a8e6a3c89f8b0a6e88f97ce500a316a18c3de52eb6b9af88 (belts, thigh plates, upper-arm plates, shoulder pads, greaves x 4 sets)
- Body scaled to 2.10 m. Each armor piece is fitted automatically to its body region (bone frame, per-axis scale, front alignment for plates) and skinned by copying the nearest body weights: `SK_OrcArmor_<A..D>_<Slot>` on the orc's skeleton, worn with Leader Pose.

Exported by `Tools/Blender/enemies_export_fbx.py`, imported by `Tools/Jam/import_enemies.py` (one shared master material, `/Game/Glyphbound/Characters/Shared/M_GBEnemy`). Intermediates (FBX, PNG) live in a temporary stage folder that is deleted after import. Delivered by the ASSETS machine on 2026-11-13 (Tripo; provenance notes in `provenance/`). Moved here from `Incoming/ASSETS` on 2026-11-14 with a SHA-256 check.
