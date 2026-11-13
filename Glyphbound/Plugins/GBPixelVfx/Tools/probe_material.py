# Probe: which material node pattern fails to compile in this engine. Builds PROBE in
# /Game/Glyphbound/Materials step by step and logs the compile errors of each step.
import unreal

MEL = unreal.MaterialEditingLibrary
MATERIAL_DIR = "/Game/Glyphbound/Materials"
PROBE = "PROBE_DeleteMe"
DEFAULT_TEXTURE = "/Engine/EngineMaterials/DefaultDiffuse.DefaultDiffuse"


def log(message):
    unreal.log("probe: " + message)


def fresh():
    path = MATERIAL_DIR + "/" + PROBE
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(PROBE, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.5)
    return material


def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(a, a_out, b, b_in):
    names = b_in if isinstance(b_in, list) else [b_in]
    for name in names:
        if MEL.connect_material_expressions(a, a_out, b, name):
            return True
    return False


def prop(source, out, target):
    return MEL.connect_material_property(source, out, target)


def check(material, label):
    errors = MEL.recompile_material(material)
    log(label + " errors=" + str(errors))


def main():
    default_texture = unreal.load_asset(DEFAULT_TEXTURE)
    # Step 1: vector param + single R mask driving opacity mask.
    material = fresh()
    vec = node(material, unreal.MaterialExpressionVectorParameter, -400, 0,
               parameter_name="FrameRect", default_value=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    comp = node(material, unreal.MaterialExpressionComponentMask, -200, 0, r=True)
    log("mask link input: " + str(link(vec, "", comp, ["", "Input"])))
    tex = node(material, unreal.MaterialExpressionTextureObjectParameter, -400, -300,
               parameter_name="Atlas", texture=default_texture,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    uv = node(material, unreal.MaterialExpressionTextureCoordinate, -200, -300, coordinate_index=0)
    sample = node(material, unreal.MaterialExpressionTextureSample, 0, -300,
                  sampler_source=unreal.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    log("sample tex link: " + str(link(tex, "", sample, ["Tex", "TextureObject"])))
    log("sample uv link: " + str(link(uv, "", sample, ["UVs", "Coordinates"])))
    log("emissive link: " + str(prop(sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)))
    log("mask->opacity link: " + str(prop(comp, "", unreal.MaterialProperty.MP_OPACITY_MASK)))
    check(material, "step1 vec+Rmask")
    # Step 1b: the other three channel masks off the same vector param.
    comp_g = node(material, unreal.MaterialExpressionComponentMask, -200, 80, g=True)
    comp_b = node(material, unreal.MaterialExpressionComponentMask, -200, 160, b=True)
    comp_a = node(material, unreal.MaterialExpressionComponentMask, -200, 240, a=True)
    log("g link: " + str(link(vec, "", comp_g, ["", "Input"])))
    log("b link: " + str(link(vec, "", comp_b, ["", "Input"])))
    log("a link: " + str(link(vec, "", comp_a, ["", "Input"])))
    append_xy = node(material, unreal.MaterialExpressionAppendVector, 0, 40)
    log("append A: " + str(link(comp, "", append_xy, "A")))
    log("append B: " + str(link(comp_g, "", append_xy, "B")))
    append_zw = node(material, unreal.MaterialExpressionAppendVector, 0, 200)
    log("append2 A: " + str(link(comp_b, "", append_zw, "A")))
    log("append2 B: " + str(link(comp_a, "", append_zw, "B")))
    scaled = node(material, unreal.MaterialExpressionMultiply, 200, -200)
    log("scaled A: " + str(link(uv, "", scaled, "A")))
    log("scaled B: " + str(link(append_zw, "", scaled, "B")))
    check(material, "step1b appends+multiply")
    # Step 1c: drive the sample UVs from the computed value.
    if MEL.connect_material_expressions(scaled, "", sample, "UVs"):
        log("uv override: True")
    else:
        # Fall back: rebuild is overkill for a probe; report and stop.
        log("uv override: False")
    check(material, "step1c computed uvs")
    # Step 2: dump the mask node properties and input names.
    log("mask inputs=" + str(list(MEL.get_material_expression_input_names(comp))))
    log("mask outputs=" + str(list(MEL.get_material_expression_output_names(comp))))
    log("vec outputs=" + str(list(MEL.get_material_expression_output_names(vec))))
    try:
        log("mask r=" + str(comp.get_editor_property("r")) + " g=" + str(comp.get_editor_property("g")))
    except Exception as exc:
        log("mask prop read failed: " + str(exc))
    log("probe: done")


main()
