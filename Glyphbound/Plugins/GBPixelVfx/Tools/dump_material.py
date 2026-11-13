# Dump the parameter layout of the built M_PixelVfxSprite material.
import unreal


def log(message):
    unreal.log("dumpmat: " + message)


def main():
    path = "/Game/Glyphbound/Materials/M_PixelVfxSprite"
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material is None:
        log("ERROR cannot load " + path)
        return
    log("class=" + material.get_class().get_name())
    log("blend=" + str(material.get_editor_property("blend_mode")))
    log("shading=" + str(material.get_editor_property("shading_model")))
    log("twosided=" + str(material.get_editor_property("two_sided")))
    log("clip=" + str(material.get_editor_property("opacity_mask_clip_value")))
    try:
        names = unreal.MaterialEditingLibrary.get_texture_parameter_names(material)
        log("texparams=" + str(list(names)))
    except Exception as exc:
        log("texparams failed: " + str(exc))
    try:
        names = unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)
        log("scalarparams=" + str(list(names)))
    except Exception as exc:
        log("scalarparams failed: " + str(exc))
    try:
        names = unreal.MaterialEditingLibrary.get_vector_parameter_names(material)
        log("vectorparams=" + str(list(names)))
    except Exception as exc:
        log("vectorparams failed: " + str(exc))
    try:
        values = unreal.MaterialEditingLibrary.get_material_texture_parameter_values(material)
        log("texvalues=" + str(values))
    except Exception as exc:
        log("texvalues failed: " + str(exc))
    log("dumpmat: done")


main()
