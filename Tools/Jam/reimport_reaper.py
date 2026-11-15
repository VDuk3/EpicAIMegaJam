# reimport_reaper.py - reimports SK_Reaper from Saved/reaper_stage/Reaper/SK_Reaper.fbx (enemies_export_fbx.py reaper: the
# crescent blade now skinned to hand_r with the staff) onto its EXISTING skeleton, so the retargeted halberd clips, IK rig and
# retargeter stay valid; keeps MI_Reaper; then adds the sockets on hand_r from report.json "sockets_m" (Unreal cm, mesh space,
# reference pose): BladeBase, BladeMid, BladeTip (the empowered scythe's trail runs along them) and Grip (the green thorns).
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/reimport_reaper.py
import unreal, os, json

EAL = unreal.EditorAssetLibrary
DEST = "/Game/Glyphbound/Characters/Reaper"
STAGE = os.path.join(unreal.Paths.project_saved_dir(), "reaper_stage", "Reaper")
FBX = os.path.join(STAGE, "SK_Reaper.fbx")
REP = json.load(open(os.path.join(STAGE, "report.json")))
out = []
skel = unreal.load_asset(DEST + "/SK_Reaper_Skeleton")
ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", True)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("create_physics_asset", False)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property("skeleton", skel)
data = ui.skeletal_mesh_import_data
data.set_editor_property("convert_scene", True)
data.set_editor_property("force_front_x_axis", False)
data.set_editor_property("import_uniform_scale", 1.0)
data.set_editor_property("import_morph_targets", False)
data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
task = unreal.AssetImportTask()
task.set_editor_property("filename", FBX)
task.set_editor_property("destination_path", DEST)
task.set_editor_property("destination_name", "SK_Reaper")
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
task.set_editor_property("options", ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
out.append("imported: %s" % list(task.get_editor_property("imported_object_paths")))
mesh = unreal.load_asset(DEST + "/SK_Reaper")
mi = unreal.load_asset(DEST + "/MI_Reaper")
if mesh and mi:
    mats = mesh.get_editor_property("materials")
    for m in mats:
        m.set_editor_property("material_interface", mi)
    mesh.set_editor_property("materials", mats)
out.append("skeleton same=%s" % (mesh.get_editor_property("skeleton") == skel))

# hand_r's reference-pose transform in mesh space, read from a temporary actor far below the level (destroyed at once). The
# sockets themselves are added with the SkeletalMeshToolset (AddSocket, SetSocketTransform): Python cannot write them.
EAL.save_loaded_asset(mesh)
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -500000))
actor.set_actor_label('REAPERFX_socketprobe')
sk = actor.skeletal_mesh_component
sk.set_skinned_asset_and_update(mesh) if hasattr(sk, 'set_skinned_asset_and_update') else sk.set_skeletal_mesh(mesh)
hand = sk.get_socket_transform('hand_r', unreal.RelativeTransformSpace.RTS_COMPONENT)
actor.destroy_actor()
out.append('hand_r mesh-space %s vs blender %s' % (hand.translation, REP['bones'].get('hand_r')))
inv = hand.inverse()
rel = {}
for name, p in REP['sockets_m'].items():
    v = inv.transform_location(unreal.Vector(*p))
    rel[name] = [round(v.x, 2), round(v.y, 2), round(v.z, 2)]
json.dump(rel, open(os.path.join(STAGE, 'sockets_hand_r.json'), 'w'))

# The weapon (owner: body and weapon are separate meshes): SM_ReaperScythe, exported in the model's frame (reference pose).
# It rides hand_r with the inverse of hand_r's reference transform, written as Enemies.json WeaponOffset
# ("x y z pitch yaw roll scale"); its sockets (the empowered blade's trails) are in the model frame, so report.json's
# sockets_m go on it as they are.
ui = unreal.FbxImportUI()
ui.set_editor_property('import_mesh', True)
ui.set_editor_property('import_as_skeletal', False)
ui.set_editor_property('import_materials', False)
ui.set_editor_property('import_textures', False)
ui.set_editor_property('import_animations', False)
ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
sd = ui.static_mesh_import_data
sd.set_editor_property('combine_meshes', True)
sd.set_editor_property('auto_generate_collision', False)
sd.set_editor_property('generate_lightmap_u_vs', False)
sd.set_editor_property('convert_scene', True)
sd.set_editor_property('force_front_x_axis', False)
sd.set_editor_property('import_uniform_scale', 1.0)
sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
t2 = unreal.AssetImportTask()
t2.set_editor_property('filename', os.path.join(STAGE, 'SM_ReaperScythe.fbx'))
t2.set_editor_property('destination_path', DEST)
t2.set_editor_property('destination_name', 'SM_ReaperScythe')
t2.set_editor_property('replace_existing', True)
t2.set_editor_property('automated', True)
t2.set_editor_property('save', True)
t2.set_editor_property('options', ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t2])
sm = unreal.load_asset(DEST + '/SM_ReaperScythe')
if sm and mi:
    sm.set_material(0, mi)
for s0 in list(sm.get_editor_property('sockets') or []) if False else []:
    pass
existing = [str(x.get_editor_property('socket_name')) for x in sm.get_sockets_by_tag('')] if hasattr(sm, 'get_sockets_by_tag') else []
for name, p in REP['sockets_m'].items():
    old = sm.find_socket(name)
    if old:
        sm.remove_socket(old)
    sock = unreal.StaticMeshSocket(sm)
    sock.set_editor_property('socket_name', name)
    sock.set_editor_property('relative_location', unreal.Vector(*p))
    sm.add_socket(sock)
EAL.save_loaded_asset(sm)
off = inv
r = off.rotation.rotator()
offset = '%.3f %.3f %.3f %.3f %.3f %.3f 1' % (off.translation.x, off.translation.y, off.translation.z, r.pitch, r.yaw, r.roll)
json.dump({'WeaponOffset': offset}, open(os.path.join(STAGE, 'weapon_offset.json'), 'w'))
out.append('weapon imported, sockets %s, WeaponOffset %s' % ([n for n in REP['sockets_m'] if sm.find_socket(n)], offset))

out.append('sockets %s' % rel)
"; ".join(out)
