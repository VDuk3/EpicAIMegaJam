# retarget_reaper.py - the final boss (the plague reaper, the Entity's second phase) gets its clips from the ARPG Pack's
# halberd set (Fab, UE5 mannequin skeleton; owner 2026-11-15: "the halberd boss"): the scythe is a two-handed polearm.
# Builds IK_Reaper (auto retarget chains) and RTG_Manny_To_Reaper (auto map + align, Root Motion op off, as in
# retarget_enemies.py), then duplicate_and_retarget of the clip list into /Game/Glyphbound/Characters/Reaper/Anims as
# A_Reaper_<name>. Every clip gets Force Root Lock (the enemy code moves the capsule; attacks must not slide the body).
# Run in the Editor after import_enemies.py (stage Saved/reaper_stage, only Reaper):
#   node Tools/Jam/mcp_call.js py Tools/Jam/retarget_reaper.py
import unreal

ATOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
SRC = "/Game/ARPG_Pack/ARPG_Halberd/Animations_UE5/ARPG_Halberd_Anim_UE_"
BASE = "/Game/Glyphbound/Characters/Reaper"
# our name <- halberd clip
CLIPS = {
    "Idle": "Idle1",
    "Walk_Fwd": "Walk_Forward_IPC",
    "Walk_Bwd": "Walk_Backward_IPC",
    "Walk_Left": "Walk_Leftward_IPC",
    "Walk_Right": "Walk_Rightward_IPC",
    "Run_Fwd": "Run_Forward_IPC",
    "Combo1": "Attack_Combo1",
    "Combo2": "Attack_Combo2",
    "Combo3": "Attack_Combo3",
    "Combo4": "Attack_Combo4",
    "Heavy1": "Attack_Heavy1",
    "Heavy2": "Attack_Heavy2",
    "Sprint_Attack": "Attack_Sprint",
    "Evade_Back": "Evade_Backward",
    "Hit1": "Hit1",
    "Hit2": "Hit2",
    "Death": "Death",
    "Death2": "Death2",
    "Heavy1_Start": "Attack_Heavy1_Start",
    "Equip": "Equip_Idle1",
    "Stagger": "Guard_Break",
    "Knockdown": "Hit_knockdown",
}
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("retarget_reaper: " + m)


def ensure(path, cls, factory):
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    pkg, name = path.rsplit("/", 1)
    return ATOOLS.create_asset(name, pkg, cls, factory)


def ik_rig(path, mesh):
    rig = ensure(path, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(mesh)
    ok = c.apply_auto_generated_retarget_definition()
    log("%s: auto retarget definition %s, root %s" % (path, ok, c.get_retarget_root()))
    EAL.save_loaded_asset(rig)
    return rig


def main():
    manny = unreal.load_asset(MANNY)
    mesh = unreal.load_asset(BASE + "/SK_Reaper")
    if mesh is None:
        log("no SK_Reaper, run import_enemies.py first")
        return
    src_rig = unreal.load_asset("/Game/Glyphbound/Characters/Shared/IK_Manny") or ik_rig("/Game/Glyphbound/Characters/Shared/IK_Manny", manny)
    tgt_rig = ik_rig(BASE + "/IK_Reaper", mesh)
    rtg = ensure(BASE + "/RTG_Manny_To_Reaper", unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    try:
        rc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, manny)
        rc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, mesh)
    except Exception as e:
        log("preview mesh: %s" % e)
    if rc.get_num_retarget_ops() == 0:
        rc.add_default_ops()
    for fn, args in (("assign_ik_rig_to_all_ops", (unreal.RetargetSourceOrTarget.SOURCE, src_rig)),
                     ("assign_ik_rig_to_all_ops", (unreal.RetargetSourceOrTarget.TARGET, tgt_rig)),
                     ("auto_map_chains", (unreal.AutoMapChainType.EXACT, True)),
                     ("auto_align_all_bones", (unreal.RetargetSourceOrTarget.TARGET,))):
        if hasattr(rc, fn):
            try:
                getattr(rc, fn)(*args)
            except Exception as e:
                log("%s failed: %s" % (fn, e))
    for i in range(rc.get_num_retarget_ops()):
        if str(rc.get_op_name(i)) == "Root Motion":
            rc.set_retarget_op_enabled(i, False)
            log("Root Motion op disabled")
    EAL.save_loaded_asset(rtg)
    dest = BASE + "/Anims"
    for name, clip in CLIPS.items():
        target = "%s/A_Reaper_%s" % (dest, name)
        if not EAL.does_asset_exist(target):
            src = SRC + clip
            if not EAL.does_asset_exist(src):
                log("missing source %s" % src)
                continue
            made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                [EAL.find_asset_data(src)], manny, mesh, rtg, "", "", "RTG_Reaper_", "", dest, False, False, True)
            if not made:
                log("%s: retarget made nothing" % name)
                continue
            EAL.rename_asset(str(made[0].package_name), target)
        seq = unreal.load_asset(target)
        seq.set_editor_property("force_root_lock", True)
        EAL.save_asset(target, only_if_is_dirty=False)
        log("  %s (%.2f s) <- %s" % (target, seq.get_play_length(), clip))
    EAL.save_directory(BASE, only_if_is_dirty=True, recursive=True)


main()
"\n".join(LOG)
