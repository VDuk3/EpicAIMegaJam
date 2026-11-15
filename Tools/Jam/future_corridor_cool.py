# future_corridor_cool.py - the warm ship corridor after the gate room, recoloured to the future base's cool palette
# (owner, 2026-11-15). Run inside the Editor with FutureBase open; everything inside one function, no lingering
# references. It does not save the map.
# Scope: the upper ShipCorridor (x about 3380, y 4190-6910). The underground wing reuses the same corridor model and
# materials and stays as it is (owner), so the upper corridor gets per-component material overrides with cool copies:
#   MI_ShipCorridor_Light     (warm white strips, 1.00 0.93 0.82) -> MI_ShipCorridor_LightCool     (0.72 0.88 1.00)
#   MI_ShipCorridor_LightWarm (amber accents,     1.00 0.75 0.45) -> MI_ShipCorridor_LightAccentCool (0.35 0.82 1.00)
# and its lights: red points (FF3C28) and warm-white rects take the map's cool colours (BAE4FF points, CDE8FF rects,
# the colours the control room, bay and lab use).
import unreal

BASE = '/Game/Glyphbound/Environment/ShipCorridor/'
SWAP = {'MI_ShipCorridor_Light': 'MI_ShipCorridor_LightCool', 'MI_ShipCorridor_LightWarm': 'MI_ShipCorridor_LightAccentCool'}
BOX = (2900.0, 3900.0, 3900.0, 7400.0, -100.0)   # x min/max, y min/max, z min
POINT_COOL = unreal.Color(r=0xBA, g=0xE4, b=0xFF, a=255)
RECT_COOL = unreal.Color(r=0xCD, g=0xE8, b=0xFF, a=255)


def corridor_cool():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert w.get_name() == 'FutureBase', 'open FutureBase first'
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cool = {k: unreal.load_asset(BASE + v) for k, v in SWAP.items()}
    swapped, lights = 0, []

    def inside(l):
        return BOX[0] < l.x < BOX[1] and BOX[2] < l.y < BOX[3] and l.z > BOX[4]

    for a in eas.get_all_level_actors():
        # Never touch the corridor smoke or the tentacle boss (other sessions' work).
        if unreal.Name('FutureSmoke') in a.tags or 'Boss_Tentacle' in a.get_actor_label() or 'Boss_Tentacle' in a.get_name():
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.get_editor_property('static_mesh')
            if not sm or 'ShipCorridor' not in sm.get_name() or c.get_world_location().z < BOX[4]:
                continue
            for i in range(c.get_num_materials()):
                m = c.get_material(i)
                if m and m.get_name() in cool:
                    c.set_material(i, cool[m.get_name()])
                    swapped += 1
        for c in a.get_components_by_class(unreal.LightComponent):
            if not inside(c.get_world_location()):
                continue
            col = c.get_editor_property('light_color')
            warm = col.r > col.b + 20
            if warm:
                c.set_editor_property('light_color', POINT_COOL if isinstance(c, unreal.PointLightComponent) else RECT_COOL)
                lights.append('%s %02X%02X%02X' % (a.get_actor_label(), col.r, col.g, col.b))
    return 'material slots swapped=%d, lights recoloured=%d: %s' % (swapped, len(lights), ', '.join(lights))


# The corridor right after the spawn room and the transfer bay (the owner's screenshot): its frames and slatted panels
# were lit by 18 orange floor lights (FF6D37) and warm-white ceiling lights (FFD7B9 and kin). Orange becomes the map's
# cool accent (65EBFF, the lab's cyan), warm whites become cool white (CDE8FF).
ENTRY_BOX = (-100.0, 3900.0, -1300.0, 3600.0, -100.0)
ACCENT_COOL = unreal.Color(r=0x65, g=0xEB, b=0xFF, a=255)


def entry_corridor_cool():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert w.get_name() == 'FutureBase', 'open FutureBase first'
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    done = []
    for a in eas.get_all_level_actors():
        if unreal.Name('FutureSmoke') in a.tags or 'Boss_Tentacle' in a.get_actor_label() or 'Boss_Tentacle' in a.get_name():
            continue
        for c in a.get_components_by_class(unreal.LightComponent):
            l = c.get_world_location()
            if not (ENTRY_BOX[0] < l.x < ENTRY_BOX[1] and ENTRY_BOX[2] < l.y < ENTRY_BOX[3] and l.z > ENTRY_BOX[4]):
                continue
            col = c.get_editor_property('light_color')
            r, g, b = col.r, col.g, col.b
            if r <= b + 20:
                continue   # already cool or neutral
            orange = g < 0.6 * r
            new = ACCENT_COOL if orange else (POINT_COOL if isinstance(c, unreal.PointLightComponent) else RECT_COOL)
            c.set_editor_property('light_color', new)
            done.append('%02X%02X%02X' % (r, g, b))
    # The frames' slatted panels glow through an orange emissive texture (MI_NonSquare_Floor_Lighting_01). The project's
    # master M_GBKit_Surface now has an optional tint (EmissiveTintAmount 0 = untouched, 1 = the texture's brightness in
    # EmissiveTint); a cool copy of the panel material is set only on this zone's components.
    src_mi = '/Game/High_Tech_Base/Materials/MI_NonSquare_Floor_Lighting_01'
    cool_mi = '/Game/Glyphbound/Materials/Kit/MI_NonSquare_Floor_Lighting_Cool'
    EAL = unreal.EditorAssetLibrary
    MEL = unreal.MaterialEditingLibrary
    if not EAL.does_asset_exist(cool_mi):
        EAL.duplicate_asset(src_mi, cool_mi)
    mi = unreal.load_asset(cool_mi)
    MEL.set_material_instance_scalar_parameter_value(mi, 'EmissiveTintAmount', 1.0)
    MEL.set_material_instance_vector_parameter_value(mi, 'EmissiveTint', unreal.LinearColor(0.40, 0.85, 1.0, 1.0))
    MEL.update_material_instance(mi)
    EAL.save_asset(cool_mi, only_if_is_dirty=False)
    slats_path = '/Game/Glyphbound/Materials/Kit/MI_Emissive_01_Cool'
    if not EAL.does_asset_exist(slats_path):
        EAL.duplicate_asset('/Game/High_Tech_Base/Materials/MI_Emissive_01_Inst', slats_path)
    slats = unreal.load_asset(slats_path)
    MEL.set_material_instance_vector_parameter_value(slats, 'Emissive_Color', unreal.LinearColor(0.40, 0.85, 1.0, 1.0))
    MEL.update_material_instance(slats)
    EAL.save_asset(slats_path, only_if_is_dirty=False)
    panels = 0
    for a in eas.get_all_level_actors():
        if unreal.Name('FutureSmoke') in a.tags or 'Boss_Tentacle' in a.get_actor_label():
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            l = c.get_world_location()
            if not (ENTRY_BOX[0] < l.x < ENTRY_BOX[1] and ENTRY_BOX[2] < l.y < ENTRY_BOX[3] and l.z > ENTRY_BOX[4]):
                continue
            for i in range(c.get_num_materials()):
                m = c.get_material(i)
                if m and m.get_name() == 'MI_NonSquare_Floor_Lighting_01':
                    c.set_material(i, mi)
                    panels += 1
                elif m and isinstance(m, unreal.MaterialInstance) and m.get_editor_property('parent') \
                        and m.get_editor_property('parent').get_name() == 'M_GBKit_Surface' and not m.get_name().endswith('_Cool'):
                    # Any other kit surface with an emissive texture (ceiling and floor strips: MI_Tile_Machine_01,
                    # MI_Sci_Fi_Electric): a tinted cool copy, set on this component only.
                    tex = MEL.get_material_instance_texture_parameter_value(m, 'Emissive')
                    if tex and tex.get_name() != 'Black':
                        copy_path = '/Game/Glyphbound/Materials/Kit/' + m.get_name() + '_Cool'
                        if not EAL.does_asset_exist(copy_path):
                            EAL.duplicate_asset(m.get_path_name().split('.')[0], copy_path)
                            cm = unreal.load_asset(copy_path)
                            MEL.set_material_instance_scalar_parameter_value(cm, 'EmissiveTintAmount', 1.0)
                            MEL.set_material_instance_vector_parameter_value(cm, 'EmissiveTint', unreal.LinearColor(0.40, 0.85, 1.0, 1.0))
                            MEL.update_material_instance(cm)
                            EAL.save_asset(copy_path, only_if_is_dirty=False)
                        c.set_material(i, unreal.load_asset(copy_path))
                        panels += 1
                elif m and m.get_name() == 'MI_Emissive_01_Inst':
                    # The frames' orange slats (Emissive_Color 1.00 0.15 0.03): a cool copy, never the kit's own asset.
                    c.set_material(i, slats)
                    panels += 1
    done.append('panels:%d' % panels)
    summary = {}
    for k in done:
        summary[k] = summary.get(k, 0) + 1
    return 'entry lights recoloured=%d %s' % (len(done), summary)
