# import_pistol.py - Unreal Editor Python script that imports the cyberpunk pistol from SourceArt and builds its
# materials. Content/ is not in git, so this script plus the source files on CENTRAL are the source of the assets.
# Run it through Tools/Jam/Import-Pistol.ps1 (UnrealEditor-Cmd -run=pythonscript), Editor closed.
#
# Inputs, in the temporary folder GB_PISTOL_STAGE, made from Glyphbound/SourceArt/Weapons/Pistol/Pistol.blend:
#   Export/SM_Pistol_{Body,Trigger,Magazine,EnergyBar}.fbx  (Tools/Blender/pistol_export_fbx.py)
#   Textures/T_Pistol_{BaseColor,ORM,Normal,EmissiveMask}.png (Tools/Blender/pistol_prepare_ue.py)
# Output: /Game/Glyphbound/Weapons/Pistol/ (meshes, textures, materials, instances). Parameters the game sets
# (keep in sync with AGBGun, Source/Glyphbound/Weapons/GBGun.cpp):
#   ElementColor   (vector) the glow colour: the enchant element, or the gun's own colour when not enchanted
#   ImbueRemaining (scalar) M_GunEnergy only: 0 = bar dark, 1 = full. Each of the 13 cells has one ImbueU value
#                  (0 at the muzzle end); a cell is lit while 1 - ImbueU < ImbueRemaining, so as the enchant runs out
#                  the cells go dark from the muzzle end towards the grip (left to right on screen, owner).
#   Flash          (scalar) M_GunIndicator only: the weak-point light, 0 = off, 1 = full glow
#   DisplayTexture (texture) M_GunHolo only: what the ammo display shows (white on black, the R channel glows)
# Every material slot of every mesh must get a material from SLOT_MATERIALS, or the script fails.

import os
import re
import unreal

MESH_DIR = "/Game/Glyphbound/Weapons/Pistol"
# Import-Pistol.ps1 bakes the textures and exports the FBX files from Pistol.blend into a temporary folder and passes
# it here; the project keeps only Pistol.blend as the source and the Unreal assets as the result (owner's rule).
SOURCE_DIR = os.environ.get("GB_PISTOL_STAGE", "")
MESHES = ["SM_Pistol_Body", "SM_Pistol_Trigger", "SM_Pistol_Magazine", "SM_Pistol_EnergyBar",
          "SM_Pistol_WeakPointLight", "SM_Pistol_AmmoDisplay"]
ENERGY_CELLS = 13
TEXTURES = {
    # name: (compression, sRGB)
    "T_Pistol_BaseColor": (unreal.TextureCompressionSettings.TC_DEFAULT, True),
    "T_Pistol_ORM": (unreal.TextureCompressionSettings.TC_MASKS, False),
    "T_Pistol_Normal": (unreal.TextureCompressionSettings.TC_NORMALMAP, False),
    "T_Pistol_EmissiveMask": (unreal.TextureCompressionSettings.TC_MASKS, False),
}
GUN_GREEN = unreal.LinearColor(0.02, 1.0, 0.07, 1.0)

# Blender material -> (parent material, {param: value}). Values copied from the Principled BSDF of each material
# (Tools/Blender/pistol_list_materials.py). Glow = Blender emission strength (emission colour = element colour).
SOLID = "M_GunSolid"
SLOT_MATERIALS = {
    "tripo_material": ("M_PistolAtlas", {}),
    "Refinished_gunmetal_panels": (SOLID, {"BaseColor": (0.83, 0.85, 0.88), "Metallic": 0.55, "Roughness": 0.28}),
    "Refined_dark_receiver": (SOLID, {"BaseColor": (0.055, 0.061, 0.07), "Metallic": 0.78, "Roughness": 0.32}),
    "FPS_SightHousing_Black": (SOLID, {"BaseColor": (0.027, 0.031, 0.039), "Metallic": 0.7, "Roughness": 0.36}),
    "Optic_Frame_Machined_Black": (SOLID, {"BaseColor": (0.025, 0.03, 0.038), "Metallic": 0.7, "Roughness": 0.3}),
    "Optic_Tinted_Transparent": ("M_GunGlass", {}),
    "Optic_Reticle_Element": (SOLID, {"BaseColor": (0.01, 0.01, 0.01), "Metallic": 0.0, "Roughness": 0.5, "Glow": 2.0}),
    "Panel_Finish_Bezel": (SOLID, {"BaseColor": (0.008, 0.012, 0.016), "Metallic": 0.75, "Roughness": 0.28}),
    "Panel_Finish_Element": (SOLID, {"BaseColor": (0.014, 0.02, 0.02), "Metallic": 0.1, "Roughness": 0.23, "Glow": 0.04}),
    "Panel_Finish_Graphics": (SOLID, {"BaseColor": (0.008, 0.012, 0.012), "Metallic": 0.1, "Roughness": 0.3, "Glow": 1.0}),
    "Trial_Magazine_DarkMetal": (SOLID, {"BaseColor": (0.04, 0.046, 0.055), "Metallic": 0.75, "Roughness": 0.3}),
    "Trial_Magazine_Silver": (SOLID, {"BaseColor": (0.43, 0.46, 0.49), "Metallic": 0.8, "Roughness": 0.28}),
    "V4_dark_metal": (SOLID, {"BaseColor": (0.027, 0.03, 0.038), "Metallic": 0.8, "Roughness": 0.35}),
    "V4_satin_edge": (SOLID, {"BaseColor": (0.1, 0.105, 0.12), "Metallic": 0.8, "Roughness": 0.3}),
    "Refined_vent_recess": (SOLID, {"BaseColor": (0.008, 0.01, 0.014), "Metallic": 0.45, "Roughness": 0.42}),
    "Refined_satin_silver": (SOLID, {"BaseColor": (0.63, 0.66, 0.69), "Metallic": 0.8, "Roughness": 0.28}),
    "V4_element_emission": ("M_GunEnergy", {}),
    "WeakPoint_Indicator": ("M_GunIndicator", {}),
    "Holo_AmmoDisplay": ("M_GunHolo", {}),
}

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(message):
    unreal.log("import_pistol: " + message)


def fail(message):
    unreal.log_error("import_pistol: ERROR " + message)
    raise RuntimeError(message)


def key(name):
    return re.sub(r"[^a-z0-9]", "", name.lower())


# ---------------------------------------------------------------- import

def run_import(filename, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = MESH_DIR
    task.automated = True
    task.replace_existing = True
    task.save = False
    if options is not None:
        task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        fail("nothing imported from %s" % filename)
    return paths


def import_textures():
    textures = {}
    for name, (compression, srgb) in TEXTURES.items():
        source = os.path.join(SOURCE_DIR, "Textures", name + ".png")
        if not os.path.isfile(source):
            fail("missing %s" % source)
        run_import(source)
        texture = unreal.load_asset(MESH_DIR + "/" + name)
        if not isinstance(texture, unreal.Texture2D):
            fail("%s did not import as a Texture2D" % name)
        texture.set_editor_property("compression_settings", compression)
        texture.set_editor_property("srgb", srgb)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WEAPON)
        textures[name] = texture
        log("texture %s %s srgb=%s" % (name, compression, srgb))
    return textures


def fbx_options():
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
    data.set_editor_property("remove_degenerates", False)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def import_meshes():
    # The legacy FBX importer honours FbxImportUI; with Interchange on, these options would be translated.
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    meshes = {}
    for name in MESHES:
        source = os.path.join(SOURCE_DIR, "Export", name + ".fbx")
        if not os.path.isfile(source):
            fail("missing %s (run Tools/Blender/pistol_export_fbx.py)" % source)
        run_import(source, fbx_options())
        mesh = unreal.load_asset(MESH_DIR + "/" + name)
        if not isinstance(mesh, unreal.StaticMesh):
            fail("%s did not import as a StaticMesh" % name)
        meshes[name] = mesh
    return meshes


# ---------------------------------------------------------------- materials

def get_or_create_material(name):
    path = MESH_DIR + "/" + name
    if EAL.does_asset_exist(path):
        material = EAL.load_asset(path)
        if not isinstance(material, unreal.Material):
            fail("%s exists but is not a Material" % path)
        MEL.delete_all_material_expressions(material)
        return material
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MESH_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        fail("could not create %s" % path)
    return material


class Graph(object):
    def __init__(self, material):
        self.material = material

    def node(self, cls, x, y, **props):
        e = MEL.create_material_expression(self.material, cls, x, y)
        if e is None:
            fail("could not create %s" % cls.__name__)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def connect(self, src, out, dst, inputs):
        if isinstance(inputs, str):
            inputs = [inputs]
        for i in inputs:
            if MEL.connect_material_expressions(src, out, dst, i):
                return
        fail("cannot connect %s.%r -> %s.%r (inputs %s)" % (
            src.get_class().get_name(), out, dst.get_class().get_name(), inputs,
            list(MEL.get_material_expression_input_names(dst))))

    def out(self, src, out, prop):
        if not MEL.connect_material_property(src, out, prop):
            fail("cannot connect %s.%r -> %s" % (src.get_class().get_name(), out, prop))

    def vector(self, name, x, y, value):
        return self.node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                         default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))

    def scalar(self, name, x, y, value):
        return self.node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def mul(self, a, b, x, y):
        m = self.node(unreal.MaterialExpressionMultiply, x, y)
        self.connect(a, "", m, "A")
        self.connect(b, "", m, "B")
        return m

    def rgb(self, src, x, y):
        m = self.node(unreal.MaterialExpressionComponentMask, x, y, r=True, g=True, b=True, a=False)
        self.connect(src, "", m, ["", "Input"])
        return m


def finish(material, name):
    errors = MEL.recompile_material(material)
    if errors:
        for e in errors:
            unreal.log_error("import_pistol: ERROR compile: %s" % e)
        fail("%s has compile errors" % name)
    bad = MEL.get_material_error_expressions(material)
    if bad:
        fail("%s: %d expressions report errors" % (name, len(bad)))
    log("material %s" % name)


def opaque(material):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", False)


def element_glow(g, x, y, glow_default):
    """ElementColor * Glow -> the emissive of a part that glows in the element colour."""
    color = g.rgb(g.vector("ElementColor", x, y, (GUN_GREEN.r, GUN_GREEN.g, GUN_GREEN.b)), x + 200, y)
    return g.mul(color, g.scalar("Glow", x, y + 150, glow_default), x + 400, y + 50)


def build_solid():
    # Plain PBR values per slot (instances set them) + optional glow in the element colour.
    m = get_or_create_material(SOLID)
    opaque(m)
    g = Graph(m)
    g.out(g.rgb(g.vector("BaseColor", -800, -300, (0.05, 0.05, 0.06)), -600, -300), "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar("Metallic", -800, -150, 0.75), "", unreal.MaterialProperty.MP_METALLIC)
    g.out(g.scalar("Roughness", -800, -50, 0.3), "", unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(element_glow(g, -1000, 100, 0.0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, SOLID)
    return m


def texture_sample(g, texture, sampler, x, y):
    return g.node(unreal.MaterialExpressionTextureSample, x, y, texture=texture, sampler_type=sampler)


def build_atlas(textures):
    # The cleaned Tripo atlas: albedo, ORM (R AO, G roughness, B metallic), DirectX normal, and the glow mask
    # (Blender: emission strength 0.65 where the mask is set, in the element colour).
    m = get_or_create_material("M_PistolAtlas")
    opaque(m)
    g = Graph(m)
    base = texture_sample(g, textures["T_Pistol_BaseColor"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -900, -500)
    orm = texture_sample(g, textures["T_Pistol_ORM"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -900, -200)
    nrm = texture_sample(g, textures["T_Pistol_Normal"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -900, 100)
    mask = texture_sample(g, textures["T_Pistol_EmissiveMask"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -900, 400)
    g.out(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    g.out(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    g.out(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    glow = element_glow(g, -900, 650, 0.65)
    emissive = g.node(unreal.MaterialExpressionMultiply, -300, 500)
    g.connect(mask, "R", emissive, "A")
    g.connect(glow, "", emissive, "B")
    g.out(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_PistolAtlas")
    return m


def build_energy():
    # The energy bar: dark metal; cells glow (strength 7 in Blender) where ImbueU (UV channel 1) < ImbueRemaining.
    m = get_or_create_material("M_GunEnergy")
    opaque(m)
    g = Graph(m)
    g.out(g.node(unreal.MaterialExpressionConstant3Vector, -600, -400,
                 constant=unreal.LinearColor(0.027, 0.03, 0.038, 1.0)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.node(unreal.MaterialExpressionConstant, -600, -300, r=0.75), "", unreal.MaterialProperty.MP_METALLIC)
    g.out(g.node(unreal.MaterialExpressionConstant, -600, -220, r=0.34), "", unreal.MaterialProperty.MP_ROUGHNESS)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, -1400, 0, coordinate_index=1)
    u_raw = g.node(unreal.MaterialExpressionComponentMask, -1200, 0, r=True, g=False, b=False, a=False)
    g.connect(uv, "", u_raw, ["", "Input"])
    u = g.node(unreal.MaterialExpressionOneMinus, -1000, 0)  # the grip-end cell stays lit longest
    g.connect(u_raw, "", u, ["", "Input"])
    remaining = g.scalar("ImbueRemaining", -1000, 120, 1.0)
    one = g.node(unreal.MaterialExpressionConstant, -1000, 240, r=1.0)
    zero = g.node(unreal.MaterialExpressionConstant, -1000, 320, r=0.0)
    lit = g.node(unreal.MaterialExpressionIf, -800, 100)
    g.connect(u, "", lit, "A")
    g.connect(remaining, "", lit, "B")
    g.connect(zero, "", lit, ["A > B", "AGreaterThanB"])
    g.connect(zero, "", lit, ["A == B", "AEqualsB"])
    g.connect(one, "", lit, ["A < B", "ALessThanB"])
    glow = element_glow(g, -1000, 450, 7.0)
    g.out(g.mul(lit, glow, -400, 200), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_GunEnergy")
    return m


def build_indicator():
    # The weak-point light next to the energy bar: dark metal like the bar, glows in the element colour with Flash.
    m = get_or_create_material("M_GunIndicator")
    opaque(m)
    g = Graph(m)
    g.out(g.node(unreal.MaterialExpressionConstant3Vector, -600, -400,
                 constant=unreal.LinearColor(0.027, 0.03, 0.038, 1.0)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.node(unreal.MaterialExpressionConstant, -600, -300, r=0.75), "", unreal.MaterialProperty.MP_METALLIC)
    g.out(g.node(unreal.MaterialExpressionConstant, -600, -220, r=0.34), "", unreal.MaterialProperty.MP_ROUGHNESS)
    glow = element_glow(g, -1000, 100, 10.0)
    g.out(g.mul(glow, g.scalar("Flash", -600, 300, 0.0), -300, 200), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_GunIndicator")
    return m


def black_texture():
    for path in ["/Engine/EngineResources/Black.Black", "/Engine/EngineMaterials/BlackSquareTexture.BlackSquareTexture",
                 "/Engine/EngineMaterials/DefaultBlackTexture.DefaultBlackTexture"]:
        texture = unreal.load_asset(path)
        if texture is not None:
            return texture
    fail("no black engine texture found for the M_GunHolo default")


def build_holo():
    # Holographic ammo display: additive, unlit. The game draws the number white on black into a render target and
    # passes it as DisplayTexture; it glows in the element colour, with scanlines, a slow flicker and a faint tint
    # over the whole pane so it reads as a projected screen.
    m = get_or_create_material("M_GunHolo")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", False)
    g = Graph(m)
    texture = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -1400, -300, parameter_name="DisplayTexture",
                     texture=black_texture(), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tint = g.scalar("PaneTint", -1400, -50, 0.04)
    ink = g.node(unreal.MaterialExpressionAdd, -1150, -200)
    g.connect(texture, "R", ink, "A")
    g.connect(tint, "", ink, "B")
    # scanlines: 0.7 + 0.3 * sin(2 pi * 48 * V)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, -1600, 200, coordinate_index=0)
    v = g.node(unreal.MaterialExpressionComponentMask, -1400, 200, r=False, g=True, b=False, a=False)
    g.connect(uv, "", v, ["", "Input"])
    lines = g.node(unreal.MaterialExpressionMultiply, -1250, 200, const_b=48.0)
    g.connect(v, "", lines, "A")
    wave = g.node(unreal.MaterialExpressionSine, -1100, 200, period=1.0)
    g.connect(lines, "", wave, ["", "Input"])
    scan = g.node(unreal.MaterialExpressionMultiply, -950, 200, const_b=0.3)
    g.connect(wave, "", scan, "A")
    scan_lit = g.node(unreal.MaterialExpressionAdd, -800, 200, const_b=0.7)
    g.connect(scan, "", scan_lit, "A")
    # flicker: 0.93 + 0.07 * sin(2 pi * 7 * time)
    time = g.node(unreal.MaterialExpressionTime, -1400, 400)
    fast = g.node(unreal.MaterialExpressionMultiply, -1250, 400, const_b=7.0)
    g.connect(time, "", fast, "A")
    flick = g.node(unreal.MaterialExpressionSine, -1100, 400, period=1.0)
    g.connect(fast, "", flick, ["", "Input"])
    flick_amp = g.node(unreal.MaterialExpressionMultiply, -950, 400, const_b=0.07)
    g.connect(flick, "", flick_amp, "A")
    flick_lit = g.node(unreal.MaterialExpressionAdd, -800, 400, const_b=0.93)
    g.connect(flick_amp, "", flick_lit, "A")
    shade = g.mul(ink, g.mul(scan_lit, flick_lit, -650, 300), -500, 0)
    g.out(g.mul(shade, element_glow(g, -900, 600, 4.0), -300, 100), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_GunHolo")
    return m


def build_glass():
    # Sight pane: Blender mixes 13% of a dark glossy surface over full transparency.
    m = get_or_create_material("M_GunGlass")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    g.out(g.node(unreal.MaterialExpressionConstant3Vector, -600, -300,
                 constant=unreal.LinearColor(0.1, 0.13, 0.13, 1.0)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.node(unreal.MaterialExpressionConstant, -600, -200, r=0.1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.scalar("Opacity", -600, -100, 0.15), "", unreal.MaterialProperty.MP_OPACITY)
    finish(m, "M_GunGlass")
    return m


def make_instance(name, parent, params):
    path = MESH_DIR + "/" + name
    if EAL.does_asset_exist(path):
        mi = EAL.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MESH_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if mi is None:
        fail("could not create %s" % path)
    MEL.set_material_instance_parent(mi, parent)
    for p, v in params.items():
        if isinstance(v, tuple):
            MEL.set_material_instance_vector_parameter_value(mi, p, unreal.LinearColor(v[0], v[1], v[2], 1.0))
        else:
            MEL.set_material_instance_scalar_parameter_value(mi, p, v)
    MEL.update_material_instance(mi)
    return mi


# ---------------------------------------------------------------- assign and check

def imbue_u_range(mesh):
    """Min and max U of UV channel 1 over the mesh source data (the editor UV queries are not there in a
    commandlet). ImbueU must span about 0..1 along the bar."""
    description = mesh.get_static_mesh_description(0)
    count = description.get_vertex_instance_count()
    us = []
    for i in range(count):
        vid = unreal.VertexInstanceID()
        vid.set_editor_property("id_value", i)
        us.append(round(description.get_vertex_instance_uv(vid, 1).x, 3))
    return min(us), max(us), count, len(set(us))


def assign(meshes, masters):
    instances = {}
    for blender_name, (parent_name, params) in SLOT_MATERIALS.items():
        if parent_name == SOLID:
            instances[key(blender_name)] = make_instance("MI_Pistol_" + blender_name, masters[SOLID], params)
        else:
            instances[key(blender_name)] = masters[parent_name]
    for name, mesh in meshes.items():
        slots = mesh.get_editor_property("static_materials")
        for index, slot in enumerate(slots):
            slot_key = key(str(slot.material_slot_name))
            match = next((m for k, m in instances.items() if slot_key.startswith(k)), None)
            if match is None:
                fail("%s slot %d '%s' has no material in SLOT_MATERIALS" % (name, index, slot.material_slot_name))
            mesh.set_material(index, match)
        box = mesh.get_bounding_box()
        log("mesh %s slots=%d tris=%d bounds min=(%.1f, %.1f, %.1f) max=(%.1f, %.1f, %.1f)" % (
            name, len(slots), mesh.get_num_triangles(0),
            box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z))
    u_min, u_max, count, distinct = imbue_u_range(meshes["SM_Pistol_EnergyBar"])
    log("energy bar ImbueU (UV 1) over %d vertex instances: %.3f .. %.3f, %d distinct values" % (
        count, u_min, u_max, distinct))
    if distinct != ENERGY_CELLS or u_min > 0.5 / ENERGY_CELLS + 0.01 or u_max < 1 - 0.5 / ENERGY_CELLS - 0.01:
        fail("SM_Pistol_EnergyBar UV channel 1 is not one ImbueU value per cell (expected %d)" % ENERGY_CELLS)


def main():
    if not SOURCE_DIR or not os.path.isdir(SOURCE_DIR):
        fail("GB_PISTOL_STAGE is not set or missing; run Tools/Jam/Import-Pistol.ps1")
    if not EAL.does_directory_exist(MESH_DIR):
        EAL.make_directory(MESH_DIR)
    textures = import_textures()
    meshes = import_meshes()
    masters = {SOLID: build_solid(), "M_PistolAtlas": build_atlas(textures), "M_GunEnergy": build_energy(),
               "M_GunGlass": build_glass(), "M_GunIndicator": build_indicator(), "M_GunHolo": build_holo()}
    assign(meshes, masters)
    if not EAL.save_directory(MESH_DIR, only_if_is_dirty=False, recursive=True):
        fail("could not save %s" % MESH_DIR)
    log("OK")


main()
