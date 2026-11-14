# import_orc_gear.py - the horde chief's shield and heavy pistol (ASSETS delivery of 2026-11-14) into the project.
# Run in the Editor after Tools/Blender/orc_gear_export.py ... export:  node Tools/Jam/mcp_call.js py Tools/Jam/import_orc_gear.py
#   /Game/Glyphbound/Characters/Orc/Gear/SM_OrcShield   face to +X, origin at the handle on its back
#   /Game/Glyphbound/Weapons/OrcPistol/SM_OrcPistol     barrel to +X, origin on the grip, socket Muzzle
# Both use an instance of the enemies' shared master material (M_GBEnemy): colour and normal straight from the delivered
# JPG maps, roughness and metal from the packed T_<name>_MR.png, and the cyan lights of the shield glowing.
import unreal, os, json

STAGE = "C:/GameJam/Glyphbound/Saved/orc_gear_stage/"
SRC = {"OrcShield": "C:/GameJam/Glyphbound/SourceArt/Enemies/OrcShield/", "OrcPistol": "C:/GameJam/Glyphbound/SourceArt/Weapons/OrcPistol/"}
DEST = {"OrcShield": "/Game/Glyphbound/Characters/Orc/Gear", "OrcPistol": "/Game/Glyphbound/Weapons/OrcPistol"}
# glow axis, threshold, boost (see import_enemies.py): the shield's cyan lights glow; the pistol has none
GLOW = {"OrcShield": ((-0.6, -0.4, 1.0), 0.22, 5.0), "OrcPistol": ((-0.6, -0.4, 1.0), 0.5, 0.0)}
MASTER = "/Game/Glyphbound/Characters/Shared/M_GBEnemy"
MEL = unreal.MaterialEditingLibrary
ATOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
out = []


def task(filename, dest, name, options=None):
    t = unreal.AssetImportTask()
    t.filename = filename
    t.destination_path = dest
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    if options is not None:
        t.options = options
    ATOOLS.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name)


def static_options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
report = json.load(open(STAGE + "report.json"))
master = unreal.load_asset(MASTER)
for name in ("OrcShield", "OrcPistol"):
    dest = DEST[name]
    tex = {}
    for kind, path in (("BaseColor", SRC[name] + name + "_BaseColor.jpg"), ("Normal", SRC[name] + name + "_Normal.jpg"), ("MR", STAGE + "T_%s_MR.png" % name)):
        t = task(path, dest, "T_%s_%s" % (name, kind))
        if not isinstance(t, unreal.Texture2D):
            out.append("%s: texture %s did not import" % (name, kind))
            continue
        if kind == "Normal":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False)
            t.set_editor_property("flip_green_channel", True)     # Tripo normal maps are OpenGL
        elif kind == "MR":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            t.set_editor_property("srgb", False)
        t.set_editor_property("max_texture_size", 2048)
        EAL.save_loaded_asset(t)
        tex[kind] = t
    mi_path = dest + "/MI_" + name
    mi = unreal.load_asset(mi_path) if EAL.does_asset_exist(mi_path) else ATOOLS.create_asset("MI_" + name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    for p, kind in (("BaseColorTex", "BaseColor"), ("NormalTex", "Normal"), ("MRTex", "MR")):
        if kind in tex:
            MEL.set_material_instance_texture_parameter_value(mi, p, tex[kind])
    axis, thr, boost = GLOW[name]
    MEL.set_material_instance_vector_parameter_value(mi, "GlowAxis", unreal.LinearColor(axis[0], axis[1], axis[2], 0))
    MEL.set_material_instance_scalar_parameter_value(mi, "GlowThreshold", thr)
    MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveBoost", boost)
    EAL.save_loaded_asset(mi)
    sm = task(STAGE + "SM_%s.fbx" % name, dest, "SM_" + name, static_options())
    if not isinstance(sm, unreal.StaticMesh):
        out.append("%s: mesh did not import" % name)
        continue
    for i in range(len(sm.get_editor_property("static_materials"))):
        sm.set_material(i, mi)
    for sock, loc in report[name].get("sockets", {}).items():
        s = sm.find_socket(sock)
        if s is None:
            s = unreal.new_object(unreal.StaticMeshSocket, outer=sm)      # the mesh as its outer, or it is not saved
            s.set_editor_property("socket_name", sock)
            sm.add_socket(s)
        s.set_editor_property("relative_location", unreal.Vector(loc[0], loc[1], loc[2]))
    sm.modify()
    EAL.save_loaded_asset(sm, only_if_is_dirty=False)
    b = sm.get_bounding_box()
    out.append("%s: box (%.0f, %.0f, %.0f)..(%.0f, %.0f, %.0f), sockets %s, textures %s" % (
        name, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, list(report[name].get("sockets", {}).keys()), sorted(tex.keys())))
" | ".join(out)
