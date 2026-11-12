# make_materials.py - Unreal Editor Python script that (re)builds the project's few materials from text.
#
# The .uasset files are not in git (Content/ is ignored except Content/Data), so this script IS the source of the
# materials. Run it through Tools/Jam/Make-Materials.ps1 (UnrealEditor-Cmd -run=pythonscript), Editor closed.
#
# M_PixelSurface (/Game/Glyphbound/Materials/M_PixelSurface): the look of every medieval / magic structure
# (GDD 3: "pixel-art environments are 3D geometry with low-resolution pixel textures"). Used from C++ by
# GBPlaceholder::SetupTexturedShape through a dynamic material instance. Parameters:
#   Texture   (texture object) the tile, a transient texture set at runtime (wrap addressing, nearest filter)
#   TileSize  (scalar, cm)     one tile covers TileSize x TileSize cm in the world, on every face
#   Tint      (vector)         multiplies the texture color
#   Glow      (scalar)         emissive = color * Glow (0 = plain lit surface)
#
# Why the projection is built by hand instead of using the engine function WorldAlignedTexture: the texture sample
# inside that function uses the shared sampler "Wrap, World group settings", which takes its filtering from the
# World texture group (smooth). Pixel art needs the texture's own nearest filter, so our sample uses
# "From texture asset" and the world-space projection is rebuilt here: absolute world position / TileSize, and the
# projection plane is picked by the dominant axis of the vertex normal (hard switch, no blending between planes).

import unreal

MATERIAL_DIR = "/Game/Glyphbound/Materials"
PIXEL_SURFACE = "M_PixelSurface"
DEFAULT_TEXTURE = "/Engine/EngineMaterials/DefaultDiffuse.DefaultDiffuse"

MEL = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("make_materials: " + message)


def fail(message):
    unreal.log_error("make_materials: ERROR " + message)
    raise RuntimeError(message)


def get_or_create_material(directory, name):
    path = directory + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(material, unreal.Material):
            fail("%s exists but is not a Material" % path)
        # Rebuild the graph from scratch so the asset always matches this script.
        MEL.delete_all_material_expressions(material)
        log("rebuilding %s" % path)
        return material
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        unreal.EditorAssetLibrary.make_directory(directory)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(name, directory, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        fail("could not create %s" % path)
    log("created %s" % path)
    return material


class Graph(object):
    def __init__(self, material):
        self.material = material
        self.x = -1600

    def node(self, expression_class, x, y, **properties):
        expression = MEL.create_material_expression(self.material, expression_class, x, y)
        if expression is None:
            fail("could not create %s" % expression_class.__name__)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def connect(self, source, source_output, target, target_inputs):
        """target_inputs: one input name or a list of accepted names (script name first, full name second)."""
        if isinstance(target_inputs, str):
            target_inputs = [target_inputs]
        for target_input in target_inputs:
            if MEL.connect_material_expressions(source, source_output, target, target_input):
                return
        fail("cannot connect %s.%r -> %s.%r (outputs %s, inputs %s)" % (
            source.get_class().get_name(), source_output, target.get_class().get_name(), target_inputs,
            list(MEL.get_material_expression_output_names(source)),
            list(MEL.get_material_expression_input_names(target))))

    def to_property(self, source, source_output, material_property):
        if not MEL.connect_material_property(source, source_output, material_property):
            fail("cannot connect %s.%r -> %s (outputs %s)" % (
                source.get_class().get_name(), source_output, material_property,
                list(MEL.get_material_expression_output_names(source))))


def mask(graph, source, x, y, r=False, g=False, b=False, a=False):
    expression = graph.node(unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=a)
    graph.connect(source, "", expression, ["", "Input"])
    return expression


def build_pixel_surface():
    material = get_or_create_material(MATERIAL_DIR, PIXEL_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", False)

    default_texture = unreal.load_asset(DEFAULT_TEXTURE)
    if default_texture is None:
        fail("cannot load the default texture %s" % DEFAULT_TEXTURE)

    g = Graph(material)

    # ---- Parameters ----
    texture = g.node(unreal.MaterialExpressionTextureObjectParameter, -1400, -500,
                     parameter_name="Texture", texture=default_texture,
                     sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tile_size = g.node(unreal.MaterialExpressionScalarParameter, -2000, 0,
                       parameter_name="TileSize", default_value=100.0)
    tint = g.node(unreal.MaterialExpressionVectorParameter, -600, -250,
                  parameter_name="Tint", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    glow = g.node(unreal.MaterialExpressionScalarParameter, -400, 150,
                  parameter_name="Glow", default_value=0.0)

    # ---- World-space coordinates in tiles ----
    world_position = g.node(unreal.MaterialExpressionWorldPosition, -2000, -150)
    in_tiles = g.node(unreal.MaterialExpressionDivide, -1800, -100)
    g.connect(world_position, "", in_tiles, "A")
    g.connect(tile_size, "", in_tiles, "B")

    wx = mask(g, in_tiles, -1650, -250, r=True)
    wy = mask(g, in_tiles, -1650, -150, g=True)
    wz = mask(g, in_tiles, -1650, -50, b=True)
    # Texture rows go top to bottom: V = -Z keeps the sprite upright on walls.
    wz_down = g.node(unreal.MaterialExpressionMultiply, -1500, -50, const_b=-1.0)
    g.connect(wz, "", wz_down, "A")

    uv_x = g.node(unreal.MaterialExpressionAppendVector, -1300, -250)  # faces looking along X
    g.connect(wy, "", uv_x, "A")
    g.connect(wz_down, "", uv_x, "B")
    uv_y = g.node(unreal.MaterialExpressionAppendVector, -1300, -150)  # faces looking along Y
    g.connect(wx, "", uv_y, "A")
    g.connect(wz_down, "", uv_y, "B")
    uv_z = g.node(unreal.MaterialExpressionAppendVector, -1300, -50)   # floors and ceilings
    g.connect(wx, "", uv_z, "A")
    g.connect(wy, "", uv_z, "B")

    # ---- Pick the plane by the dominant axis of the normal ----
    normal = g.node(unreal.MaterialExpressionVertexNormalWS, -2000, 250)
    normal_abs = g.node(unreal.MaterialExpressionAbs, -1800, 250)
    g.connect(normal, "", normal_abs, ["", "Input"])
    nx = mask(g, normal_abs, -1650, 200, r=True)
    ny = mask(g, normal_abs, -1650, 300, g=True)
    nz = mask(g, normal_abs, -1650, 400, b=True)

    uv_side = g.node(unreal.MaterialExpressionIf, -1100, -150)
    g.connect(nx, "", uv_side, "A")
    g.connect(ny, "", uv_side, "B")
    g.connect(uv_x, "", uv_side, ["A > B", "AGreaterThanB"])
    g.connect(uv_y, "", uv_side, ["A == B", "AEqualsB"])
    g.connect(uv_y, "", uv_side, ["A < B", "ALessThanB"])

    side_max = g.node(unreal.MaterialExpressionMax, -1300, 300)
    g.connect(nx, "", side_max, "A")
    g.connect(ny, "", side_max, "B")

    uv = g.node(unreal.MaterialExpressionIf, -900, -100)
    g.connect(nz, "", uv, "A")
    g.connect(side_max, "", uv, "B")
    g.connect(uv_z, "", uv, ["A > B", "AGreaterThanB"])
    g.connect(uv_side, "", uv, ["A == B", "AEqualsB"])
    g.connect(uv_side, "", uv, ["A < B", "ALessThanB"])

    # ---- Sample with the texture's own filter and addressing (nearest + wrap for tiles) ----
    sample = g.node(unreal.MaterialExpressionTextureSample, -700, -300,
                    sampler_source=unreal.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET,
                    sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    g.connect(texture, "", sample, ["Tex", "TextureObject"])
    g.connect(uv, "", sample, ["UVs", "Coordinates"])

    tint_rgb = mask(g, tint, -450, -250, r=True, g=True, b=True)
    color = g.node(unreal.MaterialExpressionMultiply, -300, -300)
    g.connect(sample, "RGB", color, "A")
    g.connect(tint_rgb, "", color, "B")

    emissive = g.node(unreal.MaterialExpressionMultiply, -150, 0)
    g.connect(color, "", emissive, "A")
    g.connect(glow, "", emissive, "B")

    roughness = g.node(unreal.MaterialExpressionConstant, -150, 150, r=0.9)

    g.to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.to_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.to_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    errors = MEL.recompile_material(material)
    if errors:
        for error in errors:
            unreal.log_error("make_materials: ERROR compile: %s" % error)
        fail("%s has compile errors" % PIXEL_SURFACE)
    bad_nodes = MEL.get_material_error_expressions(material)
    if bad_nodes:
        fail("%s: %d expressions report errors: %s" % (
            PIXEL_SURFACE, len(bad_nodes), [n.get_class().get_name() for n in bad_nodes]))

    path = MATERIAL_DIR + "/" + PIXEL_SURFACE
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        fail("could not save %s" % path)
    log("saved %s (params: %s %s %s)" % (
        path,
        list(MEL.get_texture_parameter_names(material)) if hasattr(MEL, "get_texture_parameter_names") else "",
        list(MEL.get_scalar_parameter_names(material)) if hasattr(MEL, "get_scalar_parameter_names") else "",
        list(MEL.get_vector_parameter_names(material)) if hasattr(MEL, "get_vector_parameter_names") else ""))


def main():
    build_pixel_surface()
    log("OK")


main()
