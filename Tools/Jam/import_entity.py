# import_entity.py - Unreal Editor Python script that imports the Entity (final boss, phase 1 "the machine") from
# SourceArt and builds its materials. Content/ is not in git, so this script plus Entity.blend on CENTRAL are the source.
# Run it through Tools/Jam/Import-Entity.ps1 (UnrealEditor-Cmd -run=pythonscript), Editor closed.
#
# Inputs, in the temporary folder GB_ENTITY_STAGE (Tools/Blender/entity_export_fbx.py):
#   Export/SM_Entity_{Body,Eye,Tentacles}.fbx
#   Textures/T_Entity_{BaseColor,Normal,GlowMask}.png
# Output: /Game/Glyphbound/Enemies/Entity/
#   M_EntityBody   opaque metal: the Tripo albedo desaturated into gunmetal, high metallic, normal map; the blue lights
#                  of the albedo (GlowMask) are electronic lights with a slow pulse and a fast flicker.
#                  Parameters: GlowColor (vector), Glow (scalar), Flash (scalar, 0..1, weak-point hit), FlashGlow.
#   MI_EntityEye   instance of M_EntityBody for the eye, brighter (Glow EYE_GLOW).
#   M_EntityHolo   the tentacles: additive, unlit, two-sided hologram. Rim light, a faint fill, the albedo's lights,
#                  scanlines running up in world Z, flicker, fading towards the tips. World position offset sways each
#                  tentacle (UV 1: U = 0 root .. 1 tip, V = phase per tentacle). Parameters: HoloColor, HoloGlow,
#                  SwayAmount (cm), SwaySpeed (cycles per second), Flash (shared with the body: the hit flash).
import os
import re
import unreal

MESH_DIR = "/Game/Glyphbound/Enemies/Entity"
SOURCE_DIR = os.environ.get("GB_ENTITY_STAGE", "")
MESHES = ["SM_Entity_Body", "SM_Entity_Eye", "SM_Entity_Tentacles"]
TEXTURES = {
    # name: (compression, sRGB, flip green)
    "T_Entity_BaseColor": (unreal.TextureCompressionSettings.TC_DEFAULT, True, False),
    "T_Entity_Normal": (unreal.TextureCompressionSettings.TC_NORMALMAP, False, True),   # Tripo maps are OpenGL
    "T_Entity_GlowMask": (unreal.TextureCompressionSettings.TC_MASKS, False, False),
}
MAX_TEXTURE_SIZE = 2048
GLOW_COLOR = (0.12, 0.3, 1.0)       # the model's own blue
BODY_GLOW = 6.0
EYE_GLOW = 18.0
FLASH_GLOW = 30.0
HOLO_COLOR = (0.1, 1.0, 0.3)        # hologram green (owner, 2026-11-12: green, as holograms usually are, not blue)
HOLO_GLOW = 2.5
SWAY_CM = 14.0
SWAY_SPEED = 0.3
SCAN_SPACING_CM = 7.0
SCAN_SPEED = 0.5                    # lines per second, upwards

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(message):
    unreal.log("import_entity: " + message)


def fail(message):
    unreal.log_error("import_entity: ERROR " + message)
    raise RuntimeError(message)


def key(name):
    return re.sub(r"[^a-z0-9]", "", name.lower())


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
    for name, (compression, srgb, flip) in TEXTURES.items():
        source = os.path.join(SOURCE_DIR, "Textures", name + ".png")
        if not os.path.isfile(source):
            fail("missing %s" % source)
        run_import(source)
        texture = unreal.load_asset(MESH_DIR + "/" + name)
        if not isinstance(texture, unreal.Texture2D):
            fail("%s did not import as a Texture2D" % name)
        texture.set_editor_property("compression_settings", compression)
        texture.set_editor_property("srgb", srgb)
        texture.set_editor_property("flip_green_channel", flip)
        texture.set_editor_property("max_texture_size", MAX_TEXTURE_SIZE)
        textures[name] = texture
        log("texture %s %s srgb=%s flip_green=%s max=%d" % (name, compression, srgb, flip, MAX_TEXTURE_SIZE))
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
    data.set_editor_property("auto_generate_collision", False)   # the enemy's collision is the actor's, in code
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("remove_degenerates", False)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def import_meshes():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    meshes = {}
    for name in MESHES:
        source = os.path.join(SOURCE_DIR, "Export", name + ".fbx")
        if not os.path.isfile(source):
            fail("missing %s (run Tools/Blender/entity_export_fbx.py)" % source)
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

    def scalar(self, name, x, y, value):
        return self.node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def vector(self, name, x, y, rgb):
        v = self.node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name,
                      default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(v, x + 200, y, "rgb")

    def const(self, value, x, y):
        return self.node(unreal.MaterialExpressionConstant, x, y, r=value)

    def mask(self, src, x, y, channels, src_out=""):
        m = self.node(unreal.MaterialExpressionComponentMask, x, y, r="r" in channels, g="g" in channels,
                      b="b" in channels, a="a" in channels)
        self.connect(src, src_out, m, ["", "Input"])
        return m

    def binary(self, cls, a, b, x, y, a_out="", b_out=""):
        m = self.node(cls, x, y)
        self.connect(a, a_out, m, "A")
        self.connect(b, b_out, m, "B")
        return m

    def mul(self, a, b, x, y, a_out="", b_out=""):
        return self.binary(unreal.MaterialExpressionMultiply, a, b, x, y, a_out, b_out)

    def add(self, a, b, x, y, a_out="", b_out=""):
        return self.binary(unreal.MaterialExpressionAdd, a, b, x, y, a_out, b_out)

    def unary(self, cls, src, x, y, src_out="", **props):
        m = self.node(cls, x, y, **props)
        self.connect(src, src_out, m, ["", "Input"])
        return m

    def wave(self, phase, x, y, amp, base, cos=False):
        """base + amp * sin(2 pi * phase) (or cos)."""
        s = self.unary(unreal.MaterialExpressionCosine if cos else unreal.MaterialExpressionSine, phase, x, y, period=1.0)
        return self.add(self.mul(s, self.const(amp, x, y + 90), x + 150, y), self.const(base, x + 150, y + 90), x + 300, y)

    def time_times(self, rate, x, y):
        return self.mul(self.node(unreal.MaterialExpressionTime, x, y), self.const(rate, x, y + 90), x + 150, y)


def finish(material, name):
    errors = MEL.recompile_material(material)
    if errors:
        for e in errors:
            unreal.log_error("import_entity: ERROR compile: %s" % e)
        fail("%s has compile errors" % name)
    bad = MEL.get_material_error_expressions(material)
    if bad:
        fail("%s: %d expressions report errors" % (name, len(bad)))
    log("material %s" % name)


def tex(g, textures, name, x, y, sampler):
    return g.node(unreal.MaterialExpressionTextureSample, x, y, texture=textures[name], sampler_type=sampler)


def build_body(textures):
    m = get_or_create_material("M_EntityBody")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", False)
    g = Graph(m)
    S = unreal.MaterialSamplerType
    albedo = tex(g, textures, "T_Entity_BaseColor", -2000, -700, S.SAMPLERTYPE_COLOR)
    normal = tex(g, textures, "T_Entity_Normal", -2000, -300, S.SAMPLERTYPE_NORMAL)
    glow = g.mask(tex(g, textures, "T_Entity_GlowMask", -2000, 100, S.SAMPLERTYPE_MASKS), -1750, 100, "r")
    # Metal: the albedo's dark blue paint becomes gunmetal. Desaturate most of it, brighten (a metal's base colour is
    # its reflectance, 0.5-0.9), keep a hint of blue in the tint.
    grey = g.node(unreal.MaterialExpressionDesaturation, -1700, -700)
    g.connect(albedo, "RGB", grey, ["", "Input"])
    g.connect(g.const(0.75, -1850, -600), "", grey, "Fraction")
    metal = g.mul(g.mul(grey, g.vector("MetalTint", -1700, -560, (0.72, 0.78, 0.9)), -1450, -700),
                  g.scalar("MetalBright", -1450, -560, 2.2), -1250, -700)
    g.out(g.unary(unreal.MaterialExpressionSaturate, metal, -1050, -700), "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Lights are not metal: metallic drops on the glow mask.
    not_glow = g.unary(unreal.MaterialExpressionOneMinus, glow, -1500, 100)
    g.out(g.mul(g.scalar("Metallic", -1300, -200, 0.9), not_glow, -1050, -200), "", unreal.MaterialProperty.MP_METALLIC)
    # Roughness: darker panel lines rougher, flat plates polished.
    lum = g.mask(albedo, -1700, -420, "g", "RGB")
    rough = g.add(g.const(0.22, -1450, -330), g.mul(g.unary(unreal.MaterialExpressionOneMinus,
                  g.unary(unreal.MaterialExpressionSaturate, g.mul(lum, g.const(3.0, -1550, -330), -1450, -420),
                          -1300, -420), -1150, -420), g.const(0.3, -1150, -330), -1000, -420), -850, -380)
    g.out(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    # Electronic lights: slow pulse (0.4 Hz) x fast flicker (11 Hz, slight), plus the hit flash.
    pulse = g.mul(g.wave(g.time_times(0.4, -1900, 400), -1700, 400, 0.2, 0.8),
                  g.wave(g.time_times(11.0, -1900, 600), -1700, 600, 0.04, 0.96), -1200, 500)
    level = g.add(g.mul(g.scalar("Glow", -1200, 650, BODY_GLOW), pulse, -1000, 550),
                  g.mul(g.scalar("Flash", -1200, 800, 0.0), g.scalar("FlashGlow", -1200, 880, FLASH_GLOW), -1000, 800),
                  -800, 650)
    light = g.mul(g.mul(g.vector("GlowColor", -1000, 300, GLOW_COLOR), level, -700, 400), glow, -500, 300)
    g.out(light, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_EntityBody")
    return m


def build_holo(textures):
    m = get_or_create_material("M_EntityHolo")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    S = unreal.MaterialSamplerType
    sway_uv = g.node(unreal.MaterialExpressionTextureCoordinate, -2400, 900, coordinate_index=1)
    tip = g.mask(sway_uv, -2200, 900, "r")                   # 0 root .. 1 tip
    phase = g.mask(sway_uv, -2200, 1000, "g")                # per tentacle
    # Shape of the light: rim + faint fill + the albedo's lights (dots along each tentacle).
    fres = g.node(unreal.MaterialExpressionFresnel, -1900, -500, exponent=2.5, base_reflect_fraction=0.0)
    rim = g.mul(fres, g.scalar("Rim", -1900, -380, 1.0), -1700, -500)
    dots = g.mul(g.mask(tex(g, textures, "T_Entity_GlowMask", -2200, -200, S.SAMPLERTYPE_MASKS), -1950, -200, "r"),
                 g.scalar("Dots", -1900, -80, 1.6), -1700, -200)
    shape = g.add(g.add(rim, g.scalar("Fill", -1700, -350, 0.1), -1500, -450), dots, -1300, -350)
    # Scanlines in world Z, moving up: 0.55 + 0.45 * sin(2 pi * (z / spacing - t * speed)).
    world = g.node(unreal.MaterialExpressionWorldPosition, -2400, 200)
    z = g.mask(world, -2200, 200, "b")
    lines = g.binary(unreal.MaterialExpressionSubtract,
                     g.mul(z, g.scalar("ScanDensity", -2200, 300, 1.0 / SCAN_SPACING_CM), -2000, 200),
                     g.time_times(SCAN_SPEED, -2200, 400), -1800, 250)
    scan = g.wave(lines, -1650, 250, 0.45, 0.55)
    # Flicker: 9 Hz shimmer and a slow 1.3 Hz breath.
    flicker = g.mul(g.wave(g.time_times(9.0, -2200, 550), -1950, 550, 0.08, 0.92),
                    g.wave(g.time_times(1.3, -2200, 700), -1950, 700, 0.1, 0.9), -1600, 600)
    fade = g.unary(unreal.MaterialExpressionOneMinus, g.mul(tip, g.const(0.45, -2000, 820), -1800, 800), -1600, 800)
    level = g.mul(g.mul(scan, flicker, -1350, 400), fade, -1150, 500)
    bright = g.add(g.scalar("HoloGlow", -1150, 650, HOLO_GLOW),
                   g.mul(g.scalar("Flash", -1150, 750, 0.0), g.const(6.0, -1150, 830), -950, 750), -800, 650)
    color = g.mul(g.vector("HoloColor", -1000, 0, HOLO_COLOR), bright, -700, 100)
    g.out(g.mul(g.mul(shape, level, -1000, -300), color, -450, -100), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Sway (world position offset): each tentacle circles slowly, the tip most (tip^2), out of step by its phase.
    t = g.add(g.add(g.time_times(SWAY_SPEED, -2200, 1200), phase, -1900, 1150),
              g.mul(tip, g.const(0.35, -2000, 1300), -1900, 1300), -1700, 1200)
    s = g.unary(unreal.MaterialExpressionSine, t, -1500, 1150, period=1.0)
    c = g.unary(unreal.MaterialExpressionCosine, t, -1500, 1300, period=1.0)
    weight = g.mul(g.mul(tip, tip, -1500, 1450), g.scalar("SwayAmount", -1500, 1550, SWAY_CM), -1300, 1450)
    xy = g.binary(unreal.MaterialExpressionAppendVector, s, c, -1300, 1200)
    xyz = g.binary(unreal.MaterialExpressionAppendVector, xy, g.mul(s, g.const(0.4, -1300, 1350), -1150, 1350),
                   -1000, 1250)
    g.out(g.mul(xyz, weight, -800, 1300), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    finish(m, "M_EntityHolo")
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
        MEL.set_material_instance_scalar_parameter_value(mi, p, v)
    MEL.update_material_instance(mi)
    return mi


def assign(meshes, materials):
    for name, mesh in meshes.items():
        slots = mesh.get_editor_property("static_materials")
        for index, slot in enumerate(slots):
            k = key(str(slot.material_slot_name))
            match = next((m for sk, m in materials.items() if k.startswith(sk)), None)
            if match is None:
                fail("%s slot %d '%s' has no material" % (name, index, slot.material_slot_name))
            mesh.set_material(index, match)
        box = mesh.get_bounding_box()
        log("mesh %s slots=%d tris=%d verts=%d bounds min=(%.1f, %.1f, %.1f) max=(%.1f, %.1f, %.1f)" % (
            name, len(slots), mesh.get_num_triangles(0), mesh.get_num_vertices(0),
            box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z))
    # The hologram moves its vertices (sway): grow its bounds so it is not culled at the screen edge.
    tentacles = meshes["SM_Entity_Tentacles"]
    tentacles.set_editor_property("positive_bounds_extension", unreal.Vector(SWAY_CM, SWAY_CM, SWAY_CM))
    tentacles.set_editor_property("negative_bounds_extension", unreal.Vector(SWAY_CM, SWAY_CM, SWAY_CM))
    sway_uv_check(tentacles)


def sway_uv_check(mesh):
    """UV channel 1 of the tentacles must carry the sway weight: U from 0 (roots) to 1 (tips)."""
    description = mesh.get_static_mesh_description(0)
    us = []
    for i in range(description.get_vertex_instance_count()):
        vid = unreal.VertexInstanceID()
        vid.set_editor_property("id_value", i)
        us.append(description.get_vertex_instance_uv(vid, 1).x)
    log("tentacles UV 1 (Sway) over %d vertex instances: U %.3f .. %.3f" % (len(us), min(us), max(us)))
    if min(us) > 0.05 or max(us) < 0.95:
        fail("SM_Entity_Tentacles UV channel 1 does not carry the sway weight")


def main():
    if not SOURCE_DIR or not os.path.isdir(SOURCE_DIR):
        fail("GB_ENTITY_STAGE is not set or missing; run Tools/Jam/Import-Entity.ps1")
    if not EAL.does_directory_exist(MESH_DIR):
        EAL.make_directory(MESH_DIR)
    textures = import_textures()
    meshes = import_meshes()
    body = build_body(textures)
    holo = build_holo(textures)
    eye = make_instance("MI_EntityEye", body, {"Glow": EYE_GLOW})
    assign(meshes, {key("Entity_Body"): body, key("Entity_Eye"): eye, key("Entity_Holo"): holo})
    if not EAL.save_directory(MESH_DIR, only_if_is_dirty=False, recursive=True):
        fail("could not save %s" % MESH_DIR)
    log("OK")


main()
