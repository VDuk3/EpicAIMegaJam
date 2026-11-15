# reimport_valkyrie.py - reimports SK_Valkyrie from Glyphbound/Saved/valk_stage/Valkyrie/SK_Valkyrie.fbx (made by
# Tools/Blender/enemies_export_fbx.py ... valkyrie, with the wings now rigid on the chest bone: overnight point 1)
# onto her EXISTING skeleton, so her retargeted clips, the retargeter and the IK rig stay valid. The material slot keeps
# MI_Valkyrie. Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/reimport_valkyrie.py
import unreal, os

EAL = unreal.EditorAssetLibrary
DEST = "/Game/Glyphbound/Characters/Valkyrie"
FBX = os.path.join(unreal.Paths.project_saved_dir(), "valk_stage", "Valkyrie", "SK_Valkyrie.fbx")
out = []
skel = unreal.load_asset(DEST + "/SK_Valkyrie_Skeleton")
old = unreal.load_asset(DEST + "/SK_Valkyrie")
out.append("before: skeleton=%s mesh=%s" % (skel is not None, old is not None))
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
task.set_editor_property("destination_name", "SK_Valkyrie")
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
task.set_editor_property("options", ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
out.append("imported: %s" % list(task.get_editor_property("imported_object_paths")))
mesh = unreal.load_asset(DEST + "/SK_Valkyrie")
mi = unreal.load_asset(DEST + "/MI_Valkyrie")
if mesh and mi:
    mats = mesh.get_editor_property("materials")
    for m in mats:
        m.set_editor_property("material_interface", mi)
    mesh.set_editor_property("materials", mats)
    EAL.save_loaded_asset(mesh)
out.append("after: skeleton same=%s, slots=%d" % (mesh.get_editor_property("skeleton") == skel if mesh else None,
                                                  len(mesh.get_editor_property("materials")) if mesh else -1))
"\n".join(out)
