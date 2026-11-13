# make_pixelvfx_material.py - Unreal Editor Python script that (re)builds the GBPixelVfx materials from text.
#
# The .uasset files are not in git (Content/ is ignored except Content/Data), so this script IS the source of the
# materials. Run it through Tools/Jam/PixelVfx-Jam.ps1 -Action Materials (UnrealEditor-Cmd -run=pythonscript).
#
# M_PixelVfxSprite (/Game/Glyphbound/Materials/M_PixelVfxSprite): the baked pixel-art flipbook quad for effects
# that emit light (fire, explosions, arcane fire). Masked, Unlit, two-sided. Parameters:
#   Atlas     (texture object) the baked atlas, sampled with the texture's own sampler so nearest filtering survives
#   FrameU/V/W/H (scalars)   UV = TexCoord0 * (FrameW, FrameH) + (FrameU, FrameV) (set per frame from C++)
#   Tint      (vector)         multiplies the texture color
#   Glow      (scalar)         emissive = sample.rgb * Tint.rgb * (1 + Glow * hot); hot = 1 on fire-like pixels, 0 on grey smoke
#
# M_PixelVfxSpriteLit: same layout, Masked, Default Lit, for effects that do not emit (smoke). It receives the
# scene's light and shadows, including the light of a pixel fire next to it. Base color = sample * Tint,
# roughness 1, specular 0, emissive = sample * Tint * Glow.
#
# M_PixelVfxStage (/Game/Glyphbound/Materials/M_PixelVfxStage): Opaque, Default Lit, used only by the bake
# showcase stage (a glossy floor to see reflections). Parameters:
#   Color    (vector) base color
#   Roughness (scalar, default 0.6)
#   Metallic  (scalar, default 0)

import unreal

MATERIAL_DIR = "/Game/Glyphbound/Materials"
SPRITE = "M_PixelVfxSprite"
SPRITE_LIT = "M_PixelVfxSpriteLit"
STAGE = "M_PixelVfxStage"
DEFAULT_TEXTURE = "/Engine/EngineMaterials/DefaultDiffuse.DefaultDiffuse"

MEL = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("make_pixelvfx_material: " + message)


def fail(message):
    unreal.log_error("make_pixelvfx_material: ERROR " + message)
    raise RuntimeError(message)


def get_or_create_material(directory, name):
    path = directory + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(material, unreal.Material):
            fail("%s exists but is not a Material" % path)
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
            list(MEL.get_material_expression_input_names(source))))

    def to_property(self, source, source_output, material_property):
        if not MEL.connect_material_property(source, source_output, material_property):
            fail("cannot connect %s.%r -> %s (outputs %s)" % (
                source.get_class().get_name(), source_output, material_property,
                list(MEL.get_material_expression_output_names(source))))


def mask(graph, source, x, y, r=False, g=False, b=False, a=False):
    expression = graph.node(unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=b, a=a)
    graph.connect(source, "", expression, ["", "Input"])
    return expression


def add_sprite_params(graph):
    default_texture = unreal.load_asset(DEFAULT_TEXTURE)
    if default_texture is None:
        fail("cannot load the default texture %s" % DEFAULT_TEXTURE)
    atlas = graph.node(unreal.MaterialExpressionTextureObjectParameter, -1400, -500,
                       parameter_name="Atlas", texture=default_texture,
                       sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    # The frame rect as four scalars (NOT one vector): masks on a uniform that feeds the
    # texture UVs are folded into the preshader as float3 and fail to compile, so the rect
    # stays scalar and the only masks sit on the vertex-stage texture coordinate.
    frame_u = graph.node(unreal.MaterialExpressionScalarParameter, -1400, -260,
                         parameter_name="FrameU", default_value=0.0)
    frame_v = graph.node(unreal.MaterialExpressionScalarParameter, -1400, -180,
                         parameter_name="FrameV", default_value=0.0)
    frame_w = graph.node(unreal.MaterialExpressionScalarParameter, -1400, -100,
                         parameter_name="FrameW", default_value=1.0)
    frame_h = graph.node(unreal.MaterialExpressionScalarParameter, -1400, -20,
                         parameter_name="FrameH", default_value=1.0)
    tint = graph.node(unreal.MaterialExpressionVectorParameter, -600, -250,
                      parameter_name="Tint", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    glow = graph.node(unreal.MaterialExpressionScalarParameter, -400, 150,
                      parameter_name="Glow", default_value=0.0)
    return atlas, frame_u, frame_v, frame_w, frame_h, tint, glow


def single_mask(graph, source, x, y, channel):
    kwargs = {channel: True}
    return mask(graph, source, x, y, **kwargs)


def add_sprite_sampling(graph, atlas, frame_u, frame_v, frame_w, frame_h):
    # UV = TexCoord0 * (FrameW, FrameH) + (FrameU, FrameV), sampled with the texture's own filter.
    texcoord = graph.node(unreal.MaterialExpressionTextureCoordinate, -1100, -100, coordinate_index=0)
    tex_u = single_mask(graph, texcoord, -900, -200, "r")
    tex_v = single_mask(graph, texcoord, -900, -100, "g")
    scaled_u = graph.node(unreal.MaterialExpressionMultiply, -700, -200)
    graph.connect(tex_u, "", scaled_u, "A")
    graph.connect(frame_w, "", scaled_u, "B")
    uv_u = graph.node(unreal.MaterialExpressionAdd, -500, -200)
    graph.connect(scaled_u, "", uv_u, "A")
    graph.connect(frame_u, "", uv_u, "B")
    scaled_v = graph.node(unreal.MaterialExpressionMultiply, -700, 0)
    graph.connect(tex_v, "", scaled_v, "A")
    graph.connect(frame_h, "", scaled_v, "B")
    uv_v = graph.node(unreal.MaterialExpressionAdd, -500, 0)
    graph.connect(scaled_v, "", uv_v, "A")
    graph.connect(frame_v, "", uv_v, "B")
    uv = graph.node(unreal.MaterialExpressionAppendVector, -300, -100)
    graph.connect(uv_u, "", uv, "A")
    graph.connect(uv_v, "", uv, "B")
    sample = graph.node(unreal.MaterialExpressionTextureSample, -150, -300,
                        sampler_source=unreal.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET,
                        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    graph.connect(atlas, "", sample, ["Tex", "TextureObject"])
    graph.connect(uv, "", sample, ["UVs", "Coordinates"])
    return sample


def constant(graph, value, x, y):
    return graph.node(unreal.MaterialExpressionConstant, x, y, r=value)


def binary(graph, expression_class, a, a_output, b, b_output, x, y):
    expression = graph.node(expression_class, x, y)
    graph.connect(a, a_output, expression, "A")
    graph.connect(b, b_output, expression, "B")
    return expression


def add_hot_mask(graph, sample):
    """1 on fire-like pixels (saturated, or very bright like the bone-white core), 0 on greys (smoke).
    Only those pixels take the Glow, so the smoke of a clip keeps its palette colour.
    hot = saturate(max(saturation * 2.5 - 0.25, (max_channel - 0.7) * 4))"""
    max_rg = binary(graph, unreal.MaterialExpressionMax, sample, "R", sample, "G", 150, 420)
    max_c = binary(graph, unreal.MaterialExpressionMax, max_rg, "", sample, "B", 300, 420)
    min_rg = binary(graph, unreal.MaterialExpressionMin, sample, "R", sample, "G", 150, 520)
    min_c = binary(graph, unreal.MaterialExpressionMin, min_rg, "", sample, "B", 300, 520)
    spread = binary(graph, unreal.MaterialExpressionSubtract, max_c, "", min_c, "", 450, 470)
    safe_max = binary(graph, unreal.MaterialExpressionMax, max_c, "", constant(graph, 0.001, 300, 620), "", 450, 570)
    saturation = binary(graph, unreal.MaterialExpressionDivide, spread, "", safe_max, "", 600, 470)
    sat_scaled = binary(graph, unreal.MaterialExpressionMultiply, saturation, "", constant(graph, 2.5, 600, 570), "", 750, 470)
    sat_term = binary(graph, unreal.MaterialExpressionSubtract, sat_scaled, "", constant(graph, 0.25, 750, 570), "", 900, 470)
    bright = binary(graph, unreal.MaterialExpressionSubtract, max_c, "", constant(graph, 0.7, 450, 720), "", 600, 670)
    bright_term = binary(graph, unreal.MaterialExpressionMultiply, bright, "", constant(graph, 4.0, 600, 770), "", 750, 670)
    both = binary(graph, unreal.MaterialExpressionMax, sat_term, "", bright_term, "", 1050, 570)
    hot = graph.node(unreal.MaterialExpressionSaturate, 1200, 570)
    graph.connect(both, "", hot, ["", "Input"])
    return hot


def add_sprite_color(graph, sample, tint, glow, lit):
    hot = add_hot_mask(graph, sample)
    glow = binary(graph, unreal.MaterialExpressionMultiply, glow, "", hot, "", 300, 150)
    tint_r = single_mask(graph, tint, -150, -300, "r")
    tint_g = single_mask(graph, tint, -150, -220, "g")
    tint_b = single_mask(graph, tint, -150, -140, "b")
    tint_rgb = graph.node(unreal.MaterialExpressionAppendVector, 0, -260)
    graph.connect(tint_r, "", tint_rgb, "A")
    graph.connect(tint_g, "", tint_rgb, "B")
    tint_rgb2 = graph.node(unreal.MaterialExpressionAppendVector, 150, -260)
    graph.connect(tint_rgb, "", tint_rgb2, "A")
    graph.connect(tint_b, "", tint_rgb2, "B")
    color = graph.node(unreal.MaterialExpressionMultiply, 300, -300)
    graph.connect(sample, "RGB", color, "A")
    graph.connect(tint_rgb2, "", color, "B")
    if lit:
        # Lit sprites receive scene light: emissive is only the glow contribution.
        emissive = graph.node(unreal.MaterialExpressionMultiply, 600, -150)
        graph.connect(color, "", emissive, "A")
        graph.connect(glow, "", emissive, "B")
    else:
        one = graph.node(unreal.MaterialExpressionConstant, 300, 0, r=1.0)
        one_plus_glow = graph.node(unreal.MaterialExpressionAdd, 450, 0)
        graph.connect(one, "", one_plus_glow, "A")
        graph.connect(glow, "", one_plus_glow, "B")
        emissive = graph.node(unreal.MaterialExpressionMultiply, 600, -150)
        graph.connect(color, "", emissive, "A")
        graph.connect(one_plus_glow, "", emissive, "B")
    return color, emissive


def build_sprite(name, lit):
    material = get_or_create_material(MATERIAL_DIR, name)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    if lit:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    else:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.5)

    g = Graph(material)
    atlas, frame_u, frame_v, frame_w, frame_h, tint, glow = add_sprite_params(g)
    sample = add_sprite_sampling(g, atlas, frame_u, frame_v, frame_w, frame_h)
    color, emissive = add_sprite_color(g, sample, tint, glow, lit)

    if lit:
        g.to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        roughness = g.node(unreal.MaterialExpressionConstant, 0, 250, r=1.0)
        specular = g.node(unreal.MaterialExpressionConstant, 150, 250, r=0.0)
        g.to_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        g.to_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    g.to_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.to_property(sample, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

    check_and_save(material, name)


def build_stage():
    material = get_or_create_material(MATERIAL_DIR, STAGE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", False)

    g = Graph(material)
    color_param = g.node(unreal.MaterialExpressionVectorParameter, -600, -250,
                         parameter_name="Color", default_value=unreal.LinearColor(0.02, 0.02, 0.025, 1.0))
    roughness = g.node(unreal.MaterialExpressionScalarParameter, -400, 0,
                       parameter_name="Roughness", default_value=0.6)
    metallic = g.node(unreal.MaterialExpressionScalarParameter, -400, 150,
                      parameter_name="Metallic", default_value=0.0)

    color_rgb = mask(g, color_param, -150, -250, r=True, g=True, b=True)

    g.to_property(color_rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    g.to_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    g.to_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    check_and_save(material, STAGE)


def check_and_save(material, name):
    errors = MEL.recompile_material(material)
    if errors:
        for error in errors:
            unreal.log_error("make_pixelvfx_material: ERROR compile: %s" % error)
        fail("%s has compile errors" % name)
    bad_nodes = MEL.get_material_error_expressions(material)
    if bad_nodes:
        fail("%s: %d expressions report errors: %s" % (
            name, len(bad_nodes), [n.get_class().get_name() for n in bad_nodes]))

    path = MATERIAL_DIR + "/" + name
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        fail("could not save %s" % path)
    log("saved %s" % path)


def main():
    build_sprite(SPRITE, False)
    build_sprite(SPRITE_LIT, True)
    build_stage()
    log("OK")


main()
