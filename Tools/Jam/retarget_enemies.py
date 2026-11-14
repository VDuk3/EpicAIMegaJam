# retarget_enemies.py - retargets the UE template mannequin animations (Content/Characters/Mannequins, copied from
# the engine's Templates/TemplateResources/High/Characters) onto the Tripo characters (Robot, Orc, Valkyrie), which
# have the UE5 mannequin's bone names but their own proportions and an A pose.
# Per character: IK rig with the auto-generated retarget chains, one IK retargeter Manny -> character (auto chain map,
# auto align), then duplicate_and_retarget of the clip list into /Game/Glyphbound/Characters/<C>/Anims as
# A_<C>_<clip name without MM_/MF_>. Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/retarget_enemies.py
import unreal, os, json

ATOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
ANIMS = "/Game/Characters/Mannequins/Anims/"
CLIPS = {
    "rifle": ["Rifle/MF_Rifle_Idle_ADS", "Rifle/Walk/MF_Rifle_Walk_Fwd", "Rifle/Walk/MF_Rifle_Walk_Bwd",
              "Rifle/Walk/MF_Rifle_Walk_Left", "Rifle/Walk/MF_Rifle_Walk_Right", "Rifle/Jog/MF_Rifle_Jog_Fwd",
              "Rifle/MM_Rifle_Fire", "Rifle/MM_Rifle_Reload", "Rifle/HitReact/MM_HitReact_Front_Lgt_01",
              "Rifle/HitReact/MM_HitReact_Front_Med_01", "Death/MM_Death_Front_01", "Death/MM_Death_Back_01"],
    "unarmed": ["Unarmed/MM_Idle", "Unarmed/Walk/MF_Unarmed_Walk_Fwd", "Unarmed/Walk/MF_Unarmed_Walk_Bwd",
                "Unarmed/Walk/MF_Unarmed_Walk_Left", "Unarmed/Walk/MF_Unarmed_Walk_Right", "Unarmed/Jog/MF_Unarmed_Jog_Fwd",
                "Unarmed/Attack/MM_Attack_01", "Unarmed/Attack/MM_Attack_02", "Unarmed/Attack/MM_Attack_03",
                "Unarmed/Attack/MM_ChargedAttack", "Rifle/HitReact/MM_HitReact_Front_Lgt_01", "Death/MM_Death_Front_01",
                "Death/MM_Death_Back_01"],
}
CHARS = {"Robot": ["rifle"], "Orc": ["rifle", "unarmed"], "Valkyrie": ["unarmed"]}
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("retarget_enemies: " + m)


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
    chains = [ch.get_editor_property("chain_name") if hasattr(ch, "get_editor_property") else str(ch)
              for ch in c.get_retarget_chains()]
    log("%s: auto retarget definition %s, %d chains, root %s" % (path, ok, len(chains), c.get_retarget_root()))
    EAL.save_loaded_asset(rig)
    return rig


def main():
    manny = unreal.load_asset(MANNY)
    src_rig = ik_rig("/Game/Glyphbound/Characters/Shared/IK_Manny", manny)
    for char, sets in CHARS.items():
        mesh = unreal.load_asset("/Game/Glyphbound/Characters/%s/SK_%s" % (char, char))
        if mesh is None:
            log("%s: no mesh, skipped" % char)
            continue
        tgt_rig = ik_rig("/Game/Glyphbound/Characters/%s/IK_%s" % (char, char), mesh)
        rtg = ensure("/Game/Glyphbound/Characters/%s/RTG_Manny_To_%s" % (char, char), unreal.IKRetargeter,
                     unreal.IKRetargetFactory())
        rc = unreal.IKRetargeterController.get_controller(rtg)
        rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
        rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
        try:
            rc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, manny)
            rc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, mesh)
        except Exception as e:
            log("preview mesh: %s" % e)
        if rc.get_num_retarget_ops() == 0:
            rc.add_default_ops()          # 5.9: a new retargeter has an empty op stack (pelvis, FK, IK, solve, root)
        for fn, args in (("assign_ik_rig_to_all_ops", (unreal.RetargetSourceOrTarget.SOURCE, src_rig)),
                         ("assign_ik_rig_to_all_ops", (unreal.RetargetSourceOrTarget.TARGET, tgt_rig)),
                         ("auto_map_chains", (unreal.AutoMapChainType.EXACT, True)),
                         ("auto_align_all_bones", (unreal.RetargetSourceOrTarget.TARGET,))):
            if hasattr(rc, fn):
                try:
                    getattr(rc, fn)(*args)
                    log("%s: %s ok" % (char, fn))
                except Exception as e:
                    log("%s: %s failed: %s" % (char, fn, e))
        # The Root Motion op copies Manny's root rotation onto a root bone that has its own (Blender) orientation: the
        # characters came out lying on their backs. Without it the target root keeps its reference pose.
        for i in range(rc.get_num_retarget_ops()):
            if str(rc.get_op_name(i)) == "Root Motion":
                rc.set_retarget_op_enabled(i, False)
                log("%s: Root Motion op disabled" % char)
        EAL.save_loaded_asset(rtg)
        clips = []
        for s in sets:
            for c in CLIPS[s]:
                if c not in clips:
                    clips.append(c)
        assets = []
        for c in clips:
            a = unreal.load_asset(ANIMS + c)
            if a is None:
                log("missing clip %s" % c)
            else:
                assets.append(unreal.AssetData(object_path=ANIMS + c + "." + c.split("/")[-1]) if False else
                              unreal.EditorAssetLibrary.find_asset_data(ANIMS + c))
        dest = "/Game/Glyphbound/Characters/%s/Anims" % char
        made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
            assets, manny, mesh, rtg, "", "", "RTG_%s_" % char, "", dest, False, False, True)
        log("%s: retargeted %d clips" % (char, len(made)))
        for ad in made:
            p = str(ad.package_name)
            name = p.rsplit("/", 1)[1].replace("RTG_%s_" % char, "")
            for pre in ("MF_", "MM_"):
                if name.startswith(pre):
                    name = name[len(pre):]
            target = "%s/A_%s_%s" % (dest, char, name)
            if EAL.does_asset_exist(target):
                EAL.delete_asset(target)
            EAL.rename_asset(p, target)
            log("  %s" % target)
    EAL.save_directory("/Game/Glyphbound/Characters", only_if_is_dirty=True, recursive=True)


main()
"\n".join(LOG)
