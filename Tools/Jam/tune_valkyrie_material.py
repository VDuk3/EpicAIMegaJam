# tune_valkyrie_material.py - the Valkyrie's material instance, after the owner's screenshot of 2026-11-14: her white
# wings burned out to flat white in the game (the map runs at an exposure bias of 4 and her room has a cold lamp on
# the tube). The instance's Tint darkens the base colour and the glow is lowered; the lamps were cut to a third in
# Build-FutureBase.py. Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/tune_valkyrie_material.py
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
out = []
for path, tint, boost in (('/Game/Glyphbound/Characters/Valkyrie/MI_Valkyrie', (0.5, 0.52, 0.56), 1.0),
                          ('/Game/Glyphbound/Weapons/ValkyrieSword/MI_ValkyrieSword', (0.6, 0.62, 0.66), 1.2)):
    mi = unreal.load_asset(path)
    if not mi:
        out.append(path + ': missing')
        continue
    mel.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, 'EmissiveBoost', boost)
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    out.append('%s: tint %s, glow %.1f' % (path.rsplit('/', 1)[1], tint, boost))
' | '.join(out)
