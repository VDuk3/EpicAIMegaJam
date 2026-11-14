# import_kingboars.py - a few clips of "Pistol and Rifle Locomotion Animations 1700" (Kingboars, Fab, the owner's
# library; third-party asset, Fab Standard License) onto our humanoid enemies.
# The pack is 1,688 FBX clips on the UEFN mannequin (its mesh is in the zip). The owner asked for it (2026-11-14) so
# the enemies can crouch behind cover, sprint and patrol; the Fab plugin's own import walks all 1,688 files and
# imports none (animation-only FBX without a skeleton), so the zip is unpacked by hand to Glyphbound/Saved/kingboars
# and only the clips named below are imported: the mannequin (skeleton), the clips on it, then the same IK retarget
# as retarget_enemies.py onto the Robot, the Orc and the Valkyrie: A_<Char>_KB_<name>.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/import_kingboars.py
import unreal, os

SRC = "C:/GameJam/Glyphbound/Saved/kingboars/GaspFix/"
DST = "/Game/Glyphbound/Characters/Shared/Kingboars"
ATOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
# (file under SRC without .FBX, short name)
RIFLE = [
    ("_FixedRifle/Idle/M_Neutral_Stand_Idle_Loop_Rifle", "Rifle_RelaxedIdle"),
    ("_FixedRifle/Idle/M_Neutral_Crouch_Idle_Loop_Rifle", "Rifle_CrouchIdle"),
    ("_FixedRifle/Walk/M_Neutral_Walk_Loop_F_Rifle", "Rifle_PatrolWalk"),
    ("_FixedRifle/Run/M_Neutral_Run_Loop_F_Rifle", "Rifle_Run"),
    ("_FixedRifle/Sprint/M_Neutral_Sprint_Loop_F_Rifle", "Rifle_Sprint"),
    ("_FixedRifle/Crouch/M_Neutral_Crouch_Loop_F_Rifle", "Rifle_CrouchWalk"),
]
PISTOL = [
    ("_FixedPistol/Idle/M_Neutral_Stand_Idle_Loop_Pistol", "Pistol_Idle"),
    ("_FixedPistol/Walk/M_Neutral_Walk_Loop_F_Pistol", "Pistol_Walk_Fwd"),
    ("_FixedPistol/Walk/M_Neutral_Walk_Loop_B_Pistol", "Pistol_Walk_Bwd"),
    ("_FixedPistol/Walk/M_Neutral_Walk_Loop_LL_Pistol", "Pistol_Walk_Left"),
    ("_FixedPistol/Walk/M_Neutral_Walk_Loop_RR_Pistol", "Pistol_Walk_Right"),
    ("_FixedPistol/Run/M_Neutral_Run_Loop_F_Pistol", "Pistol_Run"),
]
CHARS = {"Robot": RIFLE, "Orc": RIFLE + PISTOL, "Valkyrie": []}
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("import_kingboars: " + m)


def task(filename, dest, name, options):
    t = unreal.AssetImportTask()
    t.filename = filename
    t.destination_path = dest
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = options
    ATOOLS.import_asset_tasks([t])
    return list(t.imported_object_paths)


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
    log("%s: auto retarget definition %s, %d chains, root %s" % (path, ok, len(c.get_retarget_chains()), c.get_retarget_root()))
    EAL.save_loaded_asset(rig)
    return rig


unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")

# ---- the mannequin
mesh_path = DST + "/SKM_UEFN_Mannequin"
if not EAL.does_asset_exist(mesh_path):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    made = task(SRC + "Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.FBX", DST, "SKM_UEFN_Mannequin", ui)
    log("mannequin imported: %s" % made)
uefn = unreal.load_asset(mesh_path)
skeleton = uefn.get_editor_property("skeleton")
b = uefn.get_bounds()
log("mannequin: skeleton %s, bounds extent (%.0f, %.0f, %.0f)" % (skeleton.get_name(), b.box_extent.x, b.box_extent.y, b.box_extent.z))

# ---- the clips on the mannequin
wanted = []
for clips in CHARS.values():
    for c in clips:
        if c not in wanted:
            wanted.append(c)
for rel, short in wanted:
    name = "A_KB_" + short
    if EAL.does_asset_exist(DST + "/" + name):
        continue
    fbx = SRC + rel + ".FBX"
    if not os.path.exists(fbx):
        log("MISSING %s" % fbx)
        continue
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("skeleton", skeleton)
    made = task(fbx, DST, name, ui)
    log("clip %s: %s" % (name, made))

# ---- retarget onto our characters
src_rig = ik_rig(DST + "/IK_UEFN", uefn)
for char, clips in CHARS.items():
    if not clips:
        continue
    mesh = unreal.load_asset("/Game/Glyphbound/Characters/%s/SK_%s" % (char, char))
    tgt_rig = unreal.load_asset("/Game/Glyphbound/Characters/%s/IK_%s" % (char, char))
    if mesh is None or tgt_rig is None:
        log("%s: no mesh or IK rig, skipped" % char)
        continue
    rtg = ensure("/Game/Glyphbound/Characters/%s/RTG_UEFN_To_%s" % (char, char), unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
    rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt_rig)
    try:
        rc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, uefn)
        rc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, mesh)
    except Exception as e:
        log("preview mesh: %s" % e)
    if rc.get_num_retarget_ops() == 0:
        rc.add_default_ops()
    for fn, args in (("assign_ik_rig_to_all_ops", (unreal.RetargetSourceOrTarget.SOURCE, src_rig)),
                     ("assign_ik_rig_to_all_ops", (unreal.RetargetSourceOrTarget.TARGET, tgt_rig)),
                     ("auto_map_chains", (unreal.AutoMapChainType.EXACT, True)),
                     ("auto_align_all_bones", (unreal.RetargetSourceOrTarget.SOURCE,)),
                     ("auto_align_all_bones", (unreal.RetargetSourceOrTarget.TARGET,))):
        if hasattr(rc, fn):
            try:
                getattr(rc, fn)(*args)
            except Exception as e:
                log("%s: %s failed: %s" % (char, fn, e))
    # Same as retarget_enemies.py: the Root Motion op would copy the source root's rotation onto a root with another
    # orientation. Without it the travel stays in the pelvis; the game's animation code takes it out (clips in place).
    for i in range(rc.get_num_retarget_ops()):
        if str(rc.get_op_name(i)) == "Root Motion":
            rc.set_retarget_op_enabled(i, False)
    EAL.save_loaded_asset(rtg)
    assets = [EAL.find_asset_data(DST + "/A_KB_" + short) for _, short in clips if EAL.does_asset_exist(DST + "/A_KB_" + short)]
    dest = "/Game/Glyphbound/Characters/%s/Anims" % char
    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(assets, uefn, mesh, rtg, "", "", "RTG_%s_" % char, "", dest, False, False, True)
    for ad in made:
        p = str(ad.package_name)
        name = p.rsplit("/", 1)[1].replace("RTG_%s_A_KB_" % char, "")
        target = "%s/A_%s_KB_%s" % (dest, char, name)
        if EAL.does_asset_exist(target):
            EAL.delete_asset(target)
        EAL.rename_asset(p, target)
        anim = unreal.load_asset(target)
        length = anim.get_play_length()
        a = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "pelvis", 0.0, False)
        e = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "pelvis", length - 0.001, False)
        d = e.translation - a.translation
        log("  %s: %.2f s, pelvis travel (%.0f, %.0f, %.0f), pelvis height %.0f" % (target.rsplit("/", 1)[1], length, d.x, d.y, d.z, a.translation.z))
EAL.save_directory("/Game/Glyphbound/Characters", only_if_is_dirty=True, recursive=True)
"\n".join(LOG)
