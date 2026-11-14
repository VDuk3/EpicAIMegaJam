# Orc and its modular armor

- `Base_Rigged_Orc.glb` SHA-256 7e30cc12bbf51bcb4a6666b25b4a08168fb65c33e20d8b53087607b9d8d01726 (body, 80 bones, UE5 mannequin names)
- `Orc_Armor_1.glb` SHA-256 771baa2b50f8394f0c92b0c428de1a00a93f0f9fb86c7061eb3e5d49668726e8 (line-up: helmet, chest, one gauntlet, one boot x 4 sets)
- `Orc_Armor_2.glb` SHA-256 624ad51702a26950a8e6a3c89f8b0a6e88f97ce500a316a18c3de52eb6b9af88 (belts, thigh plates, upper-arm plates, shoulder pads, greaves x 4 sets)
- Body scaled to 2.10 m. Its textures get a dark undersuit painted in (only the head stays skin). Under armor the orc wears `SK_Orc_Armored`: the same mesh and skin, slimmer, without hands.
- Armor, refitted on 2026-11-14 (`Tools/Blender/orc_armor_fit.py`): every piece keeps its shape, is turned onto its bone and bound to that one bone; gauntlets are registered onto the orc's hand and replace it. Three sets are made (A, C, D), eight slots each: `SK_OrcArmor_<A|C|D>_<Helmet|Chest|Pelvis|Thigh|UpperArm|Shoulder|Hand|Foot>`, worn with Leader Pose. Set B and the tall boot shafts of every set are in the files but not fitted (dropped by the owner).
- Who wears what: `Tools/Jam/set_orc_armor.js` writes it into `Content/Data/Enemies.json`.

Exported by `Tools/Blender/enemies_export_fbx.py`, imported by `Tools/Jam/import_enemies.py` (one shared master material, `/Game/Glyphbound/Characters/Shared/M_GBEnemy`). Intermediates (FBX, PNG) live in a temporary stage folder that is deleted after import. Delivered by the ASSETS machine on 2026-11-13 (Tripo; provenance notes in `provenance/`). Moved here from `Incoming/ASSETS` on 2026-11-14 with a SHA-256 check.
