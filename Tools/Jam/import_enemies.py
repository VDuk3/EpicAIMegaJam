# import_enemies.py - imports the staged FBX/PNG of the future enemies (Tools/Blender/enemies_export_fbx.py) into
# /Game/Glyphbound/Characters/<Asset> (weapons: /Game/Glyphbound/Weapons/EnemyRifle), with one shared master material.
# Run inside the Editor (node Tools/Jam/mcp_call.js py <this>) or the Python commandlet. Env:
#   GB_ENEMIES_STAGE  the stage folder (one sub-folder per asset, each with report.json)
#   GB_ENEMIES_ONLY   optional, comma list of asset folder names to import (default: every folder in the stage)
# Writes <stage>/import_result.json with what was made. Content is not in git; this script is the record.
import unreal, os, json

# Inside a running Editor the environment is the Editor's: the same two values can come from Saved/import_enemies_args.json.
_ARGS = os.path.join(unreal.Paths.project_saved_dir(), "import_enemies_args.json")
_CFG = json.load(open(_ARGS)) if os.path.isfile(_ARGS) else {}
STAGE = os.environ.get("GB_ENEMIES_STAGE", _CFG.get("stage", ""))
ONLY = [a for a in os.environ.get("GB_ENEMIES_ONLY", _CFG.get("only", "")).split(",") if a]
MASTER = "/Game/Glyphbound/Characters/Shared/M_GBEnemy"
MEL = unreal.MaterialEditingLibrary
ATOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
RESULT = {"assets": {}, "errors": []}

DEST = {"EnemyRifle": "/Game/Glyphbound/Weapons/EnemyRifle", "ValkyrieSword": "/Game/Glyphbound/Weapons/ValkyrieSword", "OrcArmor": "/Game/Glyphbound/Characters/Orc/Armor"}
# Armor pieces share the orc's skeleton (import Orc first); Armor 1 slots use the first texture set, the rest the second.
ARMOR_SKELETON = "/Game/Glyphbound/Characters/Orc/SK_Orc_Skeleton"
ARMOR1_SLOTS = ("Helmet", "Chest", "Hand", "Foot")
# Glow: emissive = BaseColor * mask * EmissiveBoost, mask = saturate((dot(BaseColor, GlowAxis) - GlowThreshold) * 8).
# Blue lights (sentinel core, spider eyes, rifle strips): axis (-0.6, -0.4, 1). Red lens (drone eye): (1, -0.6, -0.6).
BLUE = (-0.6, -0.4, 1.0)
RED = (1.0, -0.6, -0.6)
# per asset: material instances {MI name: (glow axis, threshold, boost)}, which mesh gets which MI
MATS = {
    "EnemyRifle": {"MI_EnemyRifle": (BLUE, 0.12, 6.0)},
    "Robot": {"MI_Robot": (BLUE, 0.15, 4.0)},
    "Orc": {"MI_Orc": (BLUE, 0.5, 0.0)},
    "Valkyrie": {"MI_Valkyrie": (BLUE, 0.2, 3.0)},
    "ValkyrieSword": {"MI_ValkyrieSword": (BLUE, 0.15, 2.0)},
    "Drone": {"MI_Drone": (RED, 0.25, 2.0), "MI_Drone_Eye": (RED, 0.08, 12.0)},
    "Sentinel": {"MI_Sentinel": (BLUE, 0.12, 3.0), "MI_Sentinel_Core": (BLUE, 0.05, 10.0)},
    "Spider": {"MI_Spider": (BLUE, 0.1, 10.0)},
    "OrcArmor": {"MI_OrcArmor1": (BLUE, 0.2, 3.0), "MI_OrcArmor2": (BLUE, 0.2, 3.0)},
}
MESH_MI = {"SM_Drone_Eye": "MI_Drone_Eye", "SM_Sentinel_Core": "MI_Sentinel_Core"}


def log(m):
    unreal.log("import_enemies: " + m)


def err(m):
    unreal.log_error("import_enemies: ERROR " + m)
    RESULT["errors"].append(m)


def task_import(filename, dest, options=None):
    t = unreal.AssetImportTask()
    t.filename = filename
    t.destination_path = dest
    t.automated = True
    t.replace_existing = True
    t.save = False
    if options is not None:
        t.options = options
    ATOOLS.import_asset_tasks([t])
    return list(t.imported_object_paths)


# ------------------------------------------------------------------ master material
class G:
    def __init__(self, m):
        self.m = m

    def node(self, cls, x, y, **props):
        n = MEL.create_material_expression(self.m, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, a, ao, b, bi):
        MEL.connect_material_expressions(a, ao, b, bi)


def ensure_master():
    if EAL.does_asset_exist(MASTER):
        return unreal.load_asset(MASTER)
    pkg, name = MASTER.rsplit("/", 1)
    m = ATOOLS.create_asset(name, pkg, unreal.Material, unreal.MaterialFactoryNew())
    g = G(m)
    S = unreal.MaterialSamplerType
    # texture parameters need a default texture, or the material does not compile and renders as the default grey
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    flat = unreal.load_asset("/Engine/EngineMaterials/DefaultNormal")
    bc = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -1200, -400, parameter_name="BaseColorTex",
                sampler_type=S.SAMPLERTYPE_COLOR, texture=white)
    nm = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -1200, 200, parameter_name="NormalTex",
                sampler_type=S.SAMPLERTYPE_NORMAL, texture=flat)
    mr = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -1200, -100, parameter_name="MRTex",
                sampler_type=S.SAMPLERTYPE_MASKS, texture=white)
    tint = g.node(unreal.MaterialExpressionVectorParameter, -900, -550, parameter_name="Tint",
                  default_value=unreal.LinearColor(1, 1, 1, 1))
    base = g.node(unreal.MaterialExpressionMultiply, -700, -450)
    g.link(bc, "RGB", base, "A")
    g.link(tint, "", base, "B")
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough_mul = g.node(unreal.MaterialExpressionScalarParameter, -900, -150, parameter_name="RoughnessScale", default_value=1.0)
    rough = g.node(unreal.MaterialExpressionMultiply, -700, -150)
    g.link(mr, "G", rough, "A")
    g.link(rough_mul, "", rough, "B")
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mr, "B", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    # glow mask
    axis = g.node(unreal.MaterialExpressionVectorParameter, -1200, 600, parameter_name="GlowAxis",
                  default_value=unreal.LinearColor(-0.6, -0.4, 1.0, 0))
    dot = g.node(unreal.MaterialExpressionDotProduct, -950, 550)
    g.link(bc, "RGB", dot, "A")
    g.link(axis, "", dot, "B")
    thr = g.node(unreal.MaterialExpressionScalarParameter, -950, 700, parameter_name="GlowThreshold", default_value=0.15)
    sub = g.node(unreal.MaterialExpressionSubtract, -800, 600)
    g.link(dot, "", sub, "A")
    g.link(thr, "", sub, "B")
    k = g.node(unreal.MaterialExpressionConstant, -800, 720, r=8.0)
    mul = g.node(unreal.MaterialExpressionMultiply, -650, 600)
    g.link(sub, "", mul, "A")
    g.link(k, "", mul, "B")
    sat = g.node(unreal.MaterialExpressionSaturate, -500, 600)
    g.link(mul, "", sat, "")
    boost = g.node(unreal.MaterialExpressionScalarParameter, -500, 720, parameter_name="EmissiveBoost", default_value=0.0)
    e1 = g.node(unreal.MaterialExpressionMultiply, -350, 550)
    g.link(bc, "RGB", e1, "A")
    g.link(sat, "", e1, "B")
    e2 = g.node(unreal.MaterialExpressionMultiply, -200, 550)
    g.link(e1, "", e2, "A")
    g.link(boost, "", e2, "B")
    # hit flash: added flat colour the code drives (HitFlash 0..1)
    flash = g.node(unreal.MaterialExpressionScalarParameter, -350, 800, parameter_name="HitFlash", default_value=0.0)
    flash_col = g.node(unreal.MaterialExpressionVectorParameter, -350, 900, parameter_name="HitFlashColor",
                       default_value=unreal.LinearColor(3.0, 0.6, 0.3, 0))
    f = g.node(unreal.MaterialExpressionMultiply, -200, 820)
    g.link(flash, "", f, "A")
    g.link(flash_col, "", f, "B")
    em = g.node(unreal.MaterialExpressionAdd, -50, 650)
    g.link(e2, "", em, "A")
    g.link(f, "", em, "B")
    MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("master material created")
    return m


# ------------------------------------------------------------------ textures and instances
def import_textures(asset, d, dest, tex):
    out = {}
    for kind, fname in tex.items():
        path = os.path.join(d, fname)
        task_import(path, dest)
        name = os.path.splitext(fname)[0]
        t = unreal.load_asset(dest + "/" + name)
        if not isinstance(t, unreal.Texture2D):
            err("%s: texture %s did not import" % (asset, name))
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
        out[kind] = t
    return out


def make_mi(name, dest, master, textures, glow):
    path = dest + "/" + name
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = ATOOLS.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    for p, kind in (("BaseColorTex", "BaseColor"), ("NormalTex", "Normal"), ("MRTex", "MR")):
        if kind in textures:
            MEL.set_material_instance_texture_parameter_value(mi, p, textures[kind])
    axis, thr, boost = glow
    MEL.set_material_instance_vector_parameter_value(mi, "GlowAxis", unreal.LinearColor(axis[0], axis[1], axis[2], 0))
    MEL.set_material_instance_scalar_parameter_value(mi, "GlowThreshold", thr)
    MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveBoost", boost)
    EAL.save_loaded_asset(mi)
    return mi


# ------------------------------------------------------------------ meshes
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


def skeletal_options(anims, skeleton=None):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", anims)
    ui.set_editor_property("create_physics_asset", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    if skeleton is not None:
        ui.set_editor_property("skeleton", skeleton)
    data = ui.skeletal_mesh_import_data
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    # The skeletal FBX files are exported in centimetres (enemies_export_fbx.py to_cm): no scale here.
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("import_morph_targets", False)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    if anims:
        a = ui.anim_sequence_import_data
        a.set_editor_property("convert_scene", True)
        a.set_editor_property("import_uniform_scale", 1.0)
        a.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    return ui


def add_sockets(sm, sockets):
    if not sockets:
        return
    for name, loc in sockets.items():
        s = sm.find_socket(name)
        if s is None:
            # the socket must be created with the mesh as its outer, or it is not saved with the asset
            s = unreal.new_object(unreal.StaticMeshSocket, outer=sm)
            s.set_editor_property("socket_name", name)
            sm.add_socket(s)
        s.set_editor_property("relative_location", unreal.Vector(loc[0], loc[1], loc[2]))
    sm.modify()
    EAL.save_loaded_asset(sm, only_if_is_dirty=False)


def set_slot(mesh, mi):
    if isinstance(mesh, unreal.StaticMesh):
        n = len(mesh.get_editor_property("static_materials"))
        for i in range(n):
            mesh.set_material(i, mi)
    else:
        mats = list(mesh.get_editor_property("materials"))   # struct copies: write them back
        for i, m in enumerate(mats):
            m.set_editor_property("material_interface", mi)
            mats[i] = m
        mesh.set_editor_property("materials", mats)
    EAL.save_loaded_asset(mesh)


def do_asset(asset, master):
    d = os.path.join(STAGE, asset)
    rep = json.load(open(os.path.join(d, "report.json")))
    dest = DEST.get(asset, "/Game/Glyphbound/Characters/" + asset)
    if asset == "OrcArmor":
        mis = {}
        for k in ("1", "2"):
            t = import_textures(asset, d, dest, rep["textures"]["Armor" + k])
            mis["MI_OrcArmor" + k] = make_mi("MI_OrcArmor" + k, dest, master, t, MATS[asset]["MI_OrcArmor" + k])
    else:
        textures = import_textures(asset, d, dest, rep.get("textures", {}))
        mis = {n: make_mi(n, dest, master, textures, glow) for n, glow in MATS.get(asset, {}).items()}
    default_mi = list(mis.values())[0] if mis else None
    made = []
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    for fname in sorted(os.listdir(d)):
        if not fname.endswith(".fbx"):
            continue
        name = os.path.splitext(fname)[0]
        path = os.path.join(d, fname)
        if name.startswith("SK_"):
            anims = asset == "Spider"
            skel = unreal.load_asset(ARMOR_SKELETON) if asset == "OrcArmor" else None
            if asset == "OrcArmor" and skel is None:
                err("OrcArmor: no orc skeleton at %s" % ARMOR_SKELETON)
                continue
            paths = task_import(path, dest, skeletal_options(anims, skel))
            mesh = unreal.load_asset(dest + "/" + name)
            if not isinstance(mesh, unreal.SkeletalMesh):
                err("%s: %s is not a skeletal mesh (%s)" % (asset, name, paths))
                continue
            if asset == "OrcArmor":
                set_slot(mesh, mis["MI_OrcArmor1" if name.split("_")[-1] in ARMOR1_SLOTS else "MI_OrcArmor2"])
            elif default_mi:
                set_slot(mesh, default_mi)
            made.append(dest + "/" + name)
            # the FBX importer names clips <mesh>_Anim_<armature>_<action> and does not always list them as imported
            found = [a for a in EAL.list_assets(dest, recursive=False) if (name + "_Anim_") in a]
            for p in set(paths) | set(found):
                o = unreal.load_asset(p)
                if isinstance(o, unreal.AnimSequence):
                    clip = o.get_name().split("_")[-1]
                    target = dest + "/Anims/A_%s_%s" % (asset, clip)
                    if EAL.does_asset_exist(target):
                        EAL.delete_asset(target)
                    EAL.rename_asset(p.split(".")[0], target)
                    made.append(target)
        else:
            task_import(path, dest, static_options())
            mesh = unreal.load_asset(dest + "/" + name)
            if not isinstance(mesh, unreal.StaticMesh):
                err("%s: %s is not a static mesh" % (asset, name))
                continue
            mi = mis.get(MESH_MI.get(name, ""), default_mi)
            if mi:
                set_slot(mesh, mi)
            if name in ("SM_Drone_Body", "SM_Sentinel_Shell", "SM_EnemyRifle", "SM_ValkyrieSword"):
                add_sockets(mesh, rep.get("sockets", {}))
            made.append(dest + "/" + name)
    RESULT["assets"][asset] = made
    log("%s: %s" % (asset, made))


def main():
    if not STAGE or not os.path.isdir(STAGE):
        err("no stage folder %r" % STAGE)
        return
    master = ensure_master()
    for asset in sorted(os.listdir(STAGE)):
        if not os.path.isdir(os.path.join(STAGE, asset)) or (ONLY and asset not in ONLY):
            continue
        try:
            do_asset(asset, master)
        except Exception as e:
            err("%s: %s" % (asset, e))
    EAL.save_directory("/Game/Glyphbound/Characters", only_if_is_dirty=True, recursive=True)
    with open(os.path.join(STAGE, "import_result.json"), "w") as f:
        json.dump(RESULT, f, indent=1)


main()
RESULT
