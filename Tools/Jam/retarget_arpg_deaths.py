# retarget_arpg_deaths.py - full death falls from the ARPG Pack (Fab, UE5 mannequin skeleton) onto the Orc, Robot and
# Valkyrie, one per 3D humanoid enemy (overnight point 3). The template mannequin's death clips are only the stagger
# before a ragdoll (the pelvis stays at 85 cm); these end lying on the floor (pelvis 8 to 23 cm).
# The ARPG pack's four weapon sets share two in-place falls (back, forward); the other deaths (executed, knockdown)
# carry the body 2 to 4 m with the root bone, so every retargeted clip gets Force Root Lock: the body falls where the
# enemy stood, the capsule does not move, nothing slides into a wall.
# Uses the retargeters retarget_enemies.py made (only read). Clips that already exist are left alone.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/retarget_arpg_deaths.py
import unreal

EAL = unreal.EditorAssetLibrary
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
ARPG = "/Game/ARPG_Pack/"
# (character, source clip under ARPG_Pack, our name)
JOBS = [
    ("Orc", "ARPG_Warrior/Animations_UE5/ARPG_Warrior_Anim_UE_Death", "FallBack"),
    ("Orc", "ARPG_Halberd/Animations_UE5/ARPG_Halberd_Anim_UE_Death2", "FallForward"),
    ("Orc", "ARPG_Dual_Wield/Animations_UE5/ARPG_Dual_Wield_Anim_UE_Hit_knockdown_Death", "Knockdown"),
    ("Orc", "ARPG_Halberd/Animations_UE5/ARPG_Halberd_Anim_UE_Executed_Death3", "Collapse"),
    ("Orc", "ARPG_Warrior/Animations_UE5/ARPG_Warrior_Anim_UE_Executed_Death", "Kneel"),
    ("Robot", "ARPG_Dual_Wield/Animations_UE5/ARPG_Dual_Wield_Anim_UE_Executed_Death", "Slump"),
    ("Robot", "ARPG_Warrior/Animations_UE5/ARPG_Warrior_Anim_UE_Death", "FallBack"),
    ("Valkyrie", "ARPG_Samurai/Animations_UE5/ARPG_Samurai_Anim_UE_Death2", "FallForward"),
    ("Valkyrie", "ARPG_Samurai/Animations_UE5/ARPG_Samurai_Anim_UE_Death", "FallBack"),
]
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("retarget_arpg_deaths: " + m)


def main():
    manny = unreal.load_asset(MANNY)
    for char, src, name in JOBS:
        base = "/Game/Glyphbound/Characters/%s" % char
        dest = base + "/Anims"
        target = "%s/A_%s_Death_%s" % (dest, char, name)
        if not EAL.does_asset_exist(target):
            mesh = unreal.load_asset("%s/SK_%s" % (base, char))
            rtg = unreal.load_asset("%s/RTG_Manny_To_%s" % (base, char))
            if mesh is None or rtg is None or not EAL.does_asset_exist(ARPG + src):
                log("%s %s: mesh, retargeter or source missing, skipped" % (char, name))
                continue
            made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                [EAL.find_asset_data(ARPG + src)], manny, mesh, rtg, "", "", "RTG_%s_" % char, "", dest, False, False, True)
            if not made:
                log("%s %s: retarget made nothing" % (char, name))
                continue
            EAL.rename_asset(str(made[0].package_name), target)
        seq = unreal.load_asset(target)
        seq.set_editor_property("force_root_lock", True)
        EAL.save_asset(target, only_if_is_dirty=False)
        log("  %s (%.2f s) <- %s" % (target, seq.get_play_length(), src.rsplit("/", 1)[1]))


main()
"\n".join(LOG)
