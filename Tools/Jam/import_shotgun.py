# import_shotgun.py - Unreal Editor Python script that imports the shotgun from SourceArt and builds its materials.
# Content/ is not in git, so this script plus Shotgun.blend on CENTRAL are the source of the assets.
# Run it through Tools/Jam/Import-Shotgun.ps1 (UnrealEditor-Cmd -run=pythonscript), Editor closed, with the pistol
# and the rifle imported first (it reuses M_GunSolid and M_RifleHolo).
#
# Inputs, in the temporary folder GB_SHOTGUN_STAGE (Tools/Blender/shotgun_export_fbx.py):
#   Export/SM_Shotgun_{Body,Trigger,Pump,Magazine,WeakPointLight,AmmoDisplay}.fbx
#   Textures/T_Shotgun_{BaseColor,ORM,Normal,EmissiveMask}.png   (rebuilt by Tools/Blender/shotgun_build_blend.py)
# Output: /Game/Glyphbound/Weapons/Shotgun/. Parameters the game sets (same names as the pistol and the rifle, AGBGun):
#   ElementColor   (vector) the glow colour (enchant element, or the shotgun's own cyan)
#   ImbueRemaining (scalar) M_ShotgunAtlas: the energy bar's strip is split into 13 cells along the gun (UV 1 U = 0 at
#                  the strip's muzzle end); a cell is lit while 1 - its centre U < ImbueRemaining, so the strip drains
#                  from the muzzle end towards the stock. Glow off the strip (UV 1 V = 1) stays lit.
#   Flash          (scalar) M_ShotgunIndicator: the lens under the sight's window, 0 = off, 1 = full glow (weak-point hit)
#   DisplayTexture (texture) M_RifleHolo: the ammo display the game draws (frame, count, one pip per shell)
import os
import re
import unreal

MESH_DIR = "/Game/Glyphbound/Weapons/Shotgun"
SHARED_DIR = "/Game/Glyphbound/Weapons/Pistol"   # M_GunSolid, built by import_pistol.py
HOLO_DIR = "/Game/Glyphbound/Weapons/Rifle"      # M_RifleHolo, built by the rifle's import
SOURCE_DIR = os.environ.get("GB_SHOTGUN_STAGE", "")
MESHES = ["SM_Shotgun_Body", "SM_Shotgun_Trigger", "SM_Shotgun_Pump", "SM_Shotgun_Magazine", "SM_Shotgun_WeakPointLight",
          "SM_Shotgun_AmmoDisplay"]
STRIP_CELLS = 13
CELL_GAP = 0.12          # dark share at the start of each cell, so the strip reads as segments
STRIP_GLOW = 4.0         # emission strength of the neon in the element colour
INDICATOR_GLOW = 10.0
TEXTURES = {
    # name: (compression, sRGB, flip green)
    "T_Shotgun_BaseColor": (unreal.TextureCompressionSettings.TC_DEFAULT, True, False),
    # R = ambient occlusion, G = roughness, B = metallic
    "T_Shotgun_ORM": (unreal.TextureCompressionSettings.TC_MASKS, False, False),
    # baked in Blender (OpenGL, +Y): Unreal wants -Y, so the green channel is flipped on import
    "T_Shotgun_Normal": (unreal.TextureCompressionSettings.TC_NORMALMAP, False, True),
    "T_Shotgun_EmissiveMask": (unreal.TextureCompressionSettings.TC_MASKS, False, False),
}
MAX_TEXTURE_SIZE = 4096   # full size: at 2048 the parts nearest the camera look blurry and soft
SHOTGUN_CYAN = (0.10, 0.85, 1.00)
AO_IN_ALBEDO = 0.6        # share of the baked AO multiplied into the albedo (the AO pin alone only dims sky light)

SOLID = "M_GunSolid"
SLOT_MATERIALS = {
    "Shotgun_Atlas": ("M_ShotgunAtlas", {}),
    "Shotgun_Light": ("M_ShotgunIndicator", {}),
    "Holo_AmmoDisplay": ("M_RifleHolo", {}),
    # the shell: dark hull, brass base, a band that glows in the element colour
    "Shotgun_Shell_Hull": (SOLID, {"BaseColor": (0.010, 0.016, 0.024), "Metallic": 0.1, "Roughness": 0.45}),
    "Shotgun_Shell_Base": (SOLID, {"BaseColor": (0.570, 0.320, 0.055), "Metallic": 0.9, "Roughness": 0.30}),
    "Shotgun_Shell_Glow": (SOLID, {"BaseColor": (0.01, 0.01, 0.01), "Metallic": 0.0, "Roughness": 0.5, "Glow": 4.0}),
}

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(message):
    unreal.log("import_shotgun: " + message)


def fail(message):
    unreal.log_error("import_shotgun: ERROR " + message)
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
    for name, (compression, srgb, flip_green) in TEXTURES.items():
        source = os.path.join(SOURCE_DIR, "Textures", name + ".png")
        if not os.path.isfile(source):
            fail("missing %s" % source)
        run_import(source)
        texture = unreal.load_asset(MESH_DIR + "/" + name)
        if not isinstance(texture, unreal.Texture2D):
            fail("%s did not import as a Texture2D" % name)
        texture.set_editor_property("compression_settings", compression)
        texture.set_editor_property("srgb", srgb)
        texture.set_editor_property("flip_green_channel", flip_green)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WEAPON_NORMAL_MAP if flip_green
                                    else unreal.TextureGroup.TEXTUREGROUP_WEAPON)
        texture.set_editor_property("max_texture_size", MAX_TEXTURE_SIZE)
        # Always resident: streamed, a gun shows its lowest mip (flat, blurry) for the first second after equip.
        texture.set_editor_property("never_stream", True)
        textures[name] = texture
        log("texture %s %s srgb=%s max=%d" % (name, compression, srgb, MAX_TEXTURE_SIZE))
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
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    meshes = {}
    for name in MESHES:
        source = os.path.join(SOURCE_DIR, "Export", name + ".fbx")
        if not os.path.isfile(source):
            fail("missing %s (run Tools/Blender/shotgun_export_fbx.py)" % source)
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

    def const(self, value, x, y):
        return self.node(unreal.MaterialExpressionConstant, x, y, r=value)

    def binary(self, cls, a, b, x, y, a_out="", b_out=""):
        m = self.node(cls, x, y)
        self.connect(a, a_out, m, "A")
        self.connect(b, b_out, m, "B")
        return m

    def mul(self, a, b, x, y, a_out="", b_out=""):
        return self.binary(unreal.MaterialExpressionMultiply, a, b, x, y, a_out, b_out)

    def unary(self, cls, src, x, y, src_out=""):
        m = self.node(cls, x, y)
        self.connect(src, src_out, m, ["", "Input"])
        return m

    def less(self, a, b, x, y):
        """1 where a < b, else 0."""
        e = self.node(unreal.MaterialExpressionIf, x, y)
        self.connect(a, "", e, "A")
        self.connect(b, "", e, "B")
        zero, one = self.const(0.0, x - 150, y + 120), self.const(1.0, x - 150, y + 200)
        self.connect(zero, "", e, ["A > B", "AGreaterThanB"])
        self.connect(zero, "", e, ["A == B", "AEqualsB"])
        self.connect(one, "", e, ["A < B", "ALessThanB"])
        return e


def finish(material, name):
    errors = MEL.recompile_material(material)
    if errors:
        for e in errors:
            unreal.log_error("import_shotgun: ERROR compile: %s" % e)
        fail("%s has compile errors" % name)
    bad = MEL.get_material_error_expressions(material)
    if bad:
        fail("%s: %d expressions report errors" % (name, len(bad)))
    log("material %s" % name)


def opaque(material):
    # One-sided on purpose, as on the other guns (two-sided, doubled panels z-fought and looked grainy).
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", False)


def surface(g, textures):
    """Albedo, ORM and normal map as built by shotgun_build_blend.py. Returns the glow mask sample."""
    base = g.node(unreal.MaterialExpressionTextureSample, -1400, -700, texture=textures["T_Shotgun_BaseColor"],
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    orm = g.node(unreal.MaterialExpressionTextureSample, -1400, -450, texture=textures["T_Shotgun_ORM"],
                 sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    normal = g.node(unreal.MaterialExpressionTextureSample, -1400, -200, texture=textures["T_Shotgun_Normal"],
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    # Part of the baked AO goes into the albedo, so crevices stay dark under direct light too.
    shade = g.node(unreal.MaterialExpressionLinearInterpolate, -1000, -560)
    g.connect(g.const(1.0, -1150, -600), "", shade, "A")
    g.connect(orm, "R", shade, "B")
    g.connect(g.const(AO_IN_ALBEDO, -1150, -520), "", shade, "Alpha")
    g.out(g.mul(base, shade, -800, -700, a_out="RGB"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    g.out(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    g.out(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    return g.node(unreal.MaterialExpressionTextureSample, -1400, 50, texture=textures["T_Shotgun_EmissiveMask"],
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)


def element_color(g, x, y):
    v = g.node(unreal.MaterialExpressionVectorParameter, x, y, parameter_name="ElementColor",
               default_value=unreal.LinearColor(SHOTGUN_CYAN[0], SHOTGUN_CYAN[1], SHOTGUN_CYAN[2], 1.0))
    m = g.node(unreal.MaterialExpressionComponentMask, x + 200, y, r=True, g=True, b=True, a=False)
    g.connect(v, "", m, ["", "Input"])
    return m


def build_atlas(textures):
    # Body, trigger, pump: albedo and the glow mask in the element colour; on the energy bar's strip (UV 1 V = 0)
    # only lit cells glow, with thin gaps.
    m = get_or_create_material("M_ShotgunAtlas")
    opaque(m)
    g = Graph(m)
    mask = surface(g, textures)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, -2400, 200, coordinate_index=1)
    u_raw = g.node(unreal.MaterialExpressionComponentMask, -2200, 200, r=True, g=False, b=False, a=False)
    g.connect(uv, "", u_raw, ["", "Input"])
    off_strip = g.node(unreal.MaterialExpressionComponentMask, -2200, 500, r=False, g=True, b=False, a=False)
    g.connect(uv, "", off_strip, ["", "Input"])
    u = g.unary(unreal.MaterialExpressionSaturate, u_raw, -2000, 200)
    scaled = g.mul(u, g.const(float(STRIP_CELLS) * 0.99999, -2000, 320), -1850, 200)
    cell = g.unary(unreal.MaterialExpressionFloor, scaled, -1700, 150)
    centre = g.mul(g.binary(unreal.MaterialExpressionAdd, cell, g.const(0.5, -1700, 260), -1550, 150),
                   g.const(1.0 / STRIP_CELLS, -1550, 260), -1400, 150)
    from_muzzle = g.unary(unreal.MaterialExpressionOneMinus, centre, -1250, 150)
    lit = g.less(from_muzzle, g.scalar("ImbueRemaining", -1250, 260, 1.0), -1050, 150)
    within = g.unary(unreal.MaterialExpressionFrac, scaled, -1700, 380)
    not_gap = g.less(g.const(CELL_GAP, -1550, 480), within, -1400, 380)
    strip_on = g.mul(lit, not_gap, -850, 250)
    on = g.binary(unreal.MaterialExpressionMax, strip_on, off_strip, -650, 300)
    glow = g.mul(element_color(g, -1200, 700), g.scalar("Glow", -1000, 820, STRIP_GLOW), -800, 700)
    emissive = g.mul(g.mul(mask, on, -500, 400, a_out="R"), glow, -300, 500)
    g.out(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_ShotgunAtlas")
    return m


def build_indicator():
    # The lens under the sight's window: dark glass that glows only with Flash (weak-point hit).
    m = get_or_create_material("M_ShotgunIndicator")
    opaque(m)
    g = Graph(m)
    g.out(g.node(unreal.MaterialExpressionConstant3Vector, -600, -300, constant=unreal.LinearColor(0.004, 0.006, 0.008, 1.0)),
          "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.const(0.15, -600, -100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.0, -600, 0), "", unreal.MaterialProperty.MP_METALLIC)
    glow = g.mul(element_color(g, -1200, 400), g.scalar("Glow", -1000, 520, INDICATOR_GLOW), -800, 400)
    g.out(g.mul(glow, g.scalar("Flash", -800, 560, 0.0), -600, 450), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_ShotgunIndicator")
    return m


def shared(folder, name, importer):
    path = folder + "/" + name
    asset = EAL.load_asset(path) if EAL.does_asset_exist(path) else None
    if not isinstance(asset, unreal.Material):
        fail("%s is missing: run Tools/Jam/%s first" % (path, importer))
    return asset


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

def imbue_uv_check(mesh):
    """UV channel 1 of the body must carry the strip coordinate: U across 0..1 on the strip, V 0 (strip) or 1."""
    description = mesh.get_static_mesh_description(0)
    count = description.get_vertex_instance_count()
    on_strip, off_strip, us = 0, 0, []
    for i in range(count):
        vid = unreal.VertexInstanceID()
        vid.set_editor_property("id_value", i)
        uv = description.get_vertex_instance_uv(vid, 1)
        if uv.y < 0.5:
            on_strip += 1
            us.append(uv.x)
        else:
            off_strip += 1
    log("body UV 1 over %d vertex instances: %d on the strip (U %.3f .. %.3f), %d off" % (
        count, on_strip, min(us) if us else -1, max(us) if us else -1, off_strip))
    if on_strip == 0 or off_strip == 0 or min(us) > 0.05 or max(us) < 0.95:
        fail("SM_Shotgun_Body UV channel 1 does not carry the strip coordinate")


def assign(meshes, masters):
    instances = {}
    for blender_name, (parent_name, params) in SLOT_MATERIALS.items():
        if parent_name == SOLID:
            instances[key(blender_name)] = make_instance("MI_" + blender_name, masters[SOLID], params)
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
    body_slots = len(meshes["SM_Shotgun_Body"].get_editor_property("static_materials"))
    if body_slots != 1:
        fail("SM_Shotgun_Body has %d material slots, expected 1 (atlas)" % body_slots)
    shell_slots = len(meshes["SM_Shotgun_Magazine"].get_editor_property("static_materials"))
    if shell_slots != 3:
        fail("SM_Shotgun_Magazine has %d material slots, expected 3 (hull, base, glow)" % shell_slots)
    imbue_uv_check(meshes["SM_Shotgun_Body"])


def main():
    if not SOURCE_DIR or not os.path.isdir(SOURCE_DIR):
        fail("GB_SHOTGUN_STAGE is not set or missing; run Tools/Jam/Import-Shotgun.ps1")
    if not EAL.does_directory_exist(MESH_DIR):
        EAL.make_directory(MESH_DIR)
    masters = {SOLID: shared(SHARED_DIR, SOLID, "Import-Pistol.ps1"),
               "M_RifleHolo": shared(HOLO_DIR, "M_RifleHolo", "Import-Rifle.ps1")}
    textures = import_textures()
    meshes = import_meshes()
    masters["M_ShotgunAtlas"] = build_atlas(textures)
    masters["M_ShotgunIndicator"] = build_indicator()
    assign(meshes, masters)
    if not EAL.save_directory(MESH_DIR, only_if_is_dirty=False, recursive=True):
        fail("could not save %s" % MESH_DIR)
    log("OK")


main()
