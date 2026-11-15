# retarget_pack_clips.py - retargets clips from Fab animation packs made for the UE5 mannequin (Combat Magic
# Animations, Zombie Animation Pack) onto our characters, with the retargeters retarget_enemies.py made (read only).
# Overnight points 1 and 5: the Valkyrie's spell and melee clips; the castle orcs' strikes and casts (baked to pixel art).
# Clips that already exist are left alone. Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/retarget_pack_clips.py
import unreal

EAL = unreal.EditorAssetLibrary
CM = "/Game/CombatMagicAnims/"
CM_MESH = CM + "Demo/Mannequins/Meshes/SKM_Manny_Simple"
ZB = "/Game/ZombieAnimationPack/"
ZB_MESH = ZB + "Demo/EpicContent/Mannequin_UE5/Meshes/SK_Manny_Simple"
JOBS = {
    "Valkyrie": [(CM_MESH, CM, ["AS_CastSpell", "AS_LevitatingMagicStrike", "AS_Levitating", "AS_GroundManaShot",
                               "AS_MagicStrike", "AS_MagicStrike2", "AS_Ascension", "AS_SpellAttack"])],
    "Orc": [(CM_MESH, CM, ["AS_StaffStrike", "AS_StaffShot", "AS_CastSpell", "AS_StaffBlast", "AS_MagicStrike"]),
            (ZB_MESH, ZB, ["anim_Attack_A", "anim_Attack_B", "anim_Walk_A", "anim_Run_A", "anim_Idle_A",
                           "anim_Taking_Damage_A"])],
}
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("retarget_pack_clips: " + m)


def find(root, name):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in reg.get_assets_by_path(root, recursive=True):
        if str(a.asset_name) == name and str(a.asset_class_path.asset_name) == "AnimSequence":
            # the Zombie pack has each clip twice (UE4 and UE5 mannequin): take the UE5 one
            if root == ZB and "UE4" in str(a.package_path):
                continue
            return a
    return None


def main():
    for char, jobs in JOBS.items():
        base = "/Game/Glyphbound/Characters/%s" % char
        mesh = unreal.load_asset("%s/SK_%s" % (base, char))
        rtg = unreal.load_asset("%s/RTG_Manny_To_%s" % (base, char))
        dest = base + "/Anims"
        for src_mesh_path, root, names in jobs:
            src_mesh = unreal.load_asset(src_mesh_path)
            todo = []
            for n in names:
                short = n.replace("AS_", "").replace("anim_", "Zombie_")
                if EAL.does_asset_exist("%s/A_%s_%s" % (dest, char, short)):
                    continue
                a = find(root, n)
                if a is None:
                    log("%s: no clip %s" % (char, n))
                    continue
                todo.append((a, short))
            if not todo:
                continue
            made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                [a for a, _ in todo], src_mesh, mesh, rtg, "", "", "RTGP_%s_" % char, "", dest, False, False, True)
            by_src = {str(a.asset_name): s for a, s in todo}
            for ad in made:
                p = str(ad.package_name)
                raw = p.rsplit("/", 1)[1].replace("RTGP_%s_" % char, "")
                short = by_src.get(raw, raw)
                target = "%s/A_%s_%s" % (dest, char, short)
                EAL.rename_asset(p, target)
                seq = unreal.load_asset(target)
                log("  %s (%.2f s)" % (target, seq.get_play_length() if seq else -1))
    EAL.save_directory("/Game/Glyphbound/Characters", only_if_is_dirty=True, recursive=True)


main()
"\n".join(LOG)
