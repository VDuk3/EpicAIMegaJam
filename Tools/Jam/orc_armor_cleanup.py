# orc_armor_cleanup.py - deletes the orc armor assets that are no longer made: set B (dropped by the owner on
# 2026-11-14) and the boot shafts (slot Shin) of every set. Run in the Editor (node Tools/Jam/mcp_call.js py <this>)
# AFTER Enemies.json no longer names them (Tools/Jam/set_orc_armor.js). Content is not in git; this script is the record.
import unreal

EAL = unreal.EditorAssetLibrary
DIR = "/Game/Glyphbound/Characters/Orc/Armor"
gone, kept = [], []
for path in EAL.list_assets(DIR, recursive=False):
    name = path.split("/")[-1].split(".")[0]
    if not name.startswith("SK_OrcArmor_"):
        continue
    parts = name.split("_")          # SK, OrcArmor, <Set>, <Slot>[, PhysicsAsset]
    if parts[2] == "B" or parts[3] == "Shin":
        refs = [r for r in EAL.find_package_referencers_for_asset(path.split(".")[0], False)
                if not r.split("/")[-1].startswith(name.replace("_PhysicsAsset", ""))]
        if refs:
            kept.append((name, refs))
            continue
        if EAL.delete_asset(path.split(".")[0]):
            gone.append(name)
        else:
            kept.append((name, "delete failed"))
{"deleted": len(gone), "kept": kept}
