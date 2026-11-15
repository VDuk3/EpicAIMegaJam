# future_smoke.py - smoke along the way to the future Valkyrie's room (owner, 2026-11-15, with three screenshots):
# the first corridor a little smoke and its lamps failing (flicker, now and then out); the room after it more smoke;
# the third corridor more still; the boss room full of smoke, so seeing is hard. "Not the shape, that it is smoke:
# between smoke, vapour and mist."
# Two layers per area:
#   - an AGBFluidFog box (Niagara Fluids gas that parts around the player and shots) for the motion; its fill is
#     scaled per box with the tag FogDensity=<x> and its grid with FogResolution=<n> (a big room gets a coarser grid);
#   - "mist curtains": vertical planes across the way with our own instances of the castle's fog material
#     (M_NightFog: unlit, translucent, world-space drifting noise, soft where it meets walls), stacked every couple of
#     metres so the loss of sight grows with distance like a real volume. (Local Fog Volumes drew nothing in this
#     project, see make_night_fog.py.)
# The lamps of the first corridor get the failing-lamp light function (M_GB_LightFlicker) the deep wing already uses.
# Run inside the Editor with FutureBase open: node Tools/Jam/mcp_call.js py Tools/Jam/future_smoke.py
# Idempotent: everything it spawns has the tag FutureSmoke and is removed first. It does not save the map.
import unreal

TAG = 'FutureSmoke'
SMOKE_DIR = '/Game/Glyphbound/FX/Smoke'
FOG_MAT = '/Game/Glyphbound/Castle/M_NightFog'
FLICKER = '/Game/Glyphbound/Materials/M_GB_LightFlicker'
PLANE = '/Engine/BasicShapes/Plane'

# The deep wing (DEEP_Z = -600; boxes from Build-FutureBase.py WORN_X, NEST, DARK, PIT), in the owner's order.
# box (x0, x1, y0, y1, z0, z1): interior faces. fill: multiplier of the AGBFluidFog fill (0 = no new fluid box; the
# worn corridor's last 15 m and the dark corridor already have one each). cells: its grid. along / across: spacing of
# the mist planes standing across X / across Y (0 = none). opacity: of each plane. fade: how high (0..1 of the box) the
# mist keeps its full opacity before it thins out towards the ceiling. color: (r, g, b, brightness).
Z = -600.0
AREAS = {
    # 1. the worn corridor: a little smoke, low.
    'Worn': dict(box=(6954, 11000, -265, 265, Z, Z + 310), fill=0, cells=0, along=500, across=0, opacity=0.15, fade=0.35,
                 color=(0.45, 0.62, 0.60, 0.13)),
    # 2. the small room: more.
    'Nest': dict(box=(11054, 12254, -600, 600, Z, Z + 600), fill=0, cells=0, along=300, across=400, opacity=0.18, fade=0.4,
                 color=(0.62, 0.48, 0.45, 0.10)),
    # 3. the dark corridor: more still.
    'Dark': dict(box=(12254, 14762, -200, 200, Z, Z + 350), fill=0, cells=0, along=230, across=0, opacity=0.26, fade=0.6,
                 color=(0.42, 0.52, 0.66, 0.10)),
    # 4. the tentacle boss's hall: full of smoke, seeing is hard.
    'Pit': dict(box=(14762, 18362, -1400, 1400, Z, Z + 600), fill=0, cells=0, along=200, across=250, opacity=0.30, fade=0.8,
                color=(0.50, 0.55, 0.55, 0.11)),
}

# The floor fog (owner, later the same day: "dense fog on the floor, strong, with Niagara Fluids"): along the whole
# path a low, heavy layer from the floor to knee-waist height. Per area: a flat AGBFluidFog box (wide, shallow: a few
# cells high, so it stays cheap) with a strong fill, and close-set low mist planes under it so the floor is hidden even
# where the gas thins. x: the fluid box's X range (None = the area's; the worn corridor's last 15 m and the dark
# corridor keep the boxes Build-FutureBase.py placed, made denser with a FogDensity tag below).
# h: height of the layer, cm. The ramp still holds: the pit's is the densest and highest.
FLOOR = {
    'Worn': dict(h=110, x=(6954, 9500), fill=3.0, cells=96, along=110, across=0, opacity=0.40, fade=0.4, color=(0.45, 0.62, 0.60, 0.26)),
    'Nest': dict(h=130, x=None, fill=4.0, cells=96, along=110, across=150, opacity=0.34, fade=0.4, color=(0.62, 0.50, 0.46, 0.16)),
    'Dark': dict(h=140, x=None, fill=0, cells=0, along=110, across=0, opacity=0.38, fade=0.45, color=(0.42, 0.52, 0.66, 0.16)),
    'Pit': dict(h=170, x=None, fill=5.0, cells=128, along=110, across=140, opacity=0.42, fade=0.5, color=(0.52, 0.57, 0.57, 0.17)),
}
# Build-FutureBase.py's own fog boxes (not removed by this script): their fill, multiplied, by the box's centre X range.
BASE_FOG = [((9500, 11000), 3.0), ((12254, 14762), 4.0)]

# Lamps inside this box (x0, x1, y0, y1, z0, z1) get the failing-lamp light function: the worn corridor's.
FLICKER_BOX = (6954, 11000, -300, 300, Z - 50, Z + 400)


def mist_instance(name, z0, z1, opacity, fade, color, top_pad=80.0):
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    path = SMOKE_DIR + '/MI_GB_Mist_' + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_GB_Mist_' + name, SMOKE_DIR, unreal.MaterialInstanceConstant,
                                                                    unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', unreal.load_asset(FOG_MAT))
    for k, v in (('Opacity', opacity), ('NoiseOpacity', 1.0), ('FogFadeStart', z0 + (z1 - z0) * fade), ('FogTop', z1 + top_pad),
                 ('Brightness', color[3])):
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.set_material_instance_vector_parameter_value(mi, 'FogColor', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


def planes(eas, plane, mi, name, box, along_x, spacing, first):
    """Mist planes standing across X (along_x) or across Y, every spacing cm through the box."""
    x0, x1, y0, y1, z0, z1 = box
    cy, cx, cz = (y0 + y1) / 2.0, (x0 + x1) / 2.0, (z0 + z1) / 2.0
    a0, a1 = (x0, x1) if along_x else (y0, y1)
    w = (y1 - y0) if along_x else (x1 - x0)
    n = 0
    p = a0 + spacing * 0.5
    while p < a1:
        # The plane is 100 x 100 cm in XY; pitch 90 stands it up facing along X, yaw 90 turns it to face along Y.
        loc = unreal.Vector(p, cy, cz) if along_x else unreal.Vector(cx, p, cz)
        m = eas.spawn_actor_from_object(plane, loc, unreal.Rotator(roll=0, pitch=90, yaw=0 if along_x else 90))
        m.set_actor_scale3d(unreal.Vector((z1 - z0) / 100.0, w / 100.0, 1.0))
        sm = m.static_mesh_component
        sm.set_material(0, mi)
        sm.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        sm.set_editor_property('cast_shadow', False)
        tagged(m, name)
        m.set_actor_label('Smoke_%s_Mist%02d' % (name, first + n))
        n += 1
        p += spacing
    return n


def tagged(a, folder):
    a.tags = list(a.tags) + [unreal.Name(TAG)]
    a.set_folder_path(unreal.Name('Smoke/' + folder))


def build():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_name() == 'FutureBase', 'open FutureBase first, not ' + world.get_name()
    gone = 0
    for a in eas.get_all_level_actors():
        if unreal.Name(TAG) in a.tags:
            eas.destroy_actor(a)
            gone += 1
    log = ['removed %d' % gone]
    plane = unreal.load_asset(PLANE)
    for name, d in AREAS.items():
        x0, x1, y0, y1, z0, z1 = d['box']
        c = unreal.Vector((x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0)
        if d['fill'] > 0:
            f = eas.spawn_actor_from_class(unreal.GBFluidFog, c, unreal.Rotator(0, 0, 0))
            f.set_editor_property('box_extent', unreal.Vector((x1 - x0) / 2.0, (y1 - y0) / 2.0, (z1 - z0) / 2.0))
            f.tags = [unreal.Name('FogDensity=%g' % d['fill']), unreal.Name('FogResolution=%d' % d['cells'])]
            tagged(f, name)
            f.set_actor_label('Smoke_%s_Fluid' % name)
        mi = mist_instance(name, z0, z1, d['opacity'], d['fade'], d['color'])
        n = 0
        if d['along']:
            n += planes(eas, plane, mi, name, d['box'], True, d['along'], n)
        if d['across']:
            n += planes(eas, plane, mi, name, d['box'], False, d['across'], n)
        log.append('%s: fluid x%g, %d mist planes' % (name, d['fill'], n))
        fl = FLOOR.get(name)
        if fl:
            fx0, fx1 = fl['x'] or (x0, x1)
            fbox = (fx0, fx1, y0, y1, z0, z0 + fl['h'])
            if fl['fill'] > 0:
                # The fluid box is a bit higher than the layer, so the gas has room to roll over itself.
                fz1 = z0 + fl['h'] * 1.6
                f = eas.spawn_actor_from_class(unreal.GBFluidFog, unreal.Vector((fx0 + fx1) / 2.0, c.y, (z0 + fz1) / 2.0), unreal.Rotator(0, 0, 0))
                f.set_editor_property('box_extent', unreal.Vector((fx1 - fx0) / 2.0, (y1 - y0) / 2.0, (fz1 - z0) / 2.0))
                f.tags = [unreal.Name('FogDensity=%g' % fl['fill']), unreal.Name('FogResolution=%d' % fl['cells'])]
                tagged(f, name)
                f.set_actor_label('Smoke_%s_FloorFluid' % name)
            fmi = mist_instance(name + '_Floor', fbox[4], fbox[5], fl['opacity'], fl['fade'], fl['color'], top_pad=-5.0)   # gone at the planes' top: no hard edge
            k = 0
            if fl['along']:
                k += planes(eas, plane, fmi, name + '_Floor', fbox, True, fl['along'], k)
            if fl['across']:
                k += planes(eas, plane, fmi, name + '_Floor', fbox, False, fl['across'], k)
            log.append('%s floor: fluid x%g, %d low planes' % (name, fl['fill'], k))
    for a in eas.get_all_level_actors():
        if isinstance(a, unreal.GBFluidFog) and unreal.Name(TAG) not in a.tags:
            x = a.get_actor_location().x
            for (bx0, bx1), mult in BASE_FOG:
                if bx0 <= x <= bx1:
                    a.tags = [t for t in a.tags if not str(t).startswith('FogDensity=')] + [unreal.Name('FogDensity=%g' % mult)]
                    log.append('%s: FogDensity=%g' % (a.get_actor_label(), mult))
    if FLICKER_BOX:
        fx0, fx1, fy0, fy1, fz0, fz1 = FLICKER_BOX
        fn = unreal.load_asset(FLICKER)
        k = 0
        for a in eas.get_all_level_actors():
            if not isinstance(a, unreal.Light):
                continue
            l = a.get_actor_location()
            if fx0 <= l.x <= fx1 and fy0 <= l.y <= fy1 and fz0 <= l.z <= fz1:
                lc = a.light_component
                lc.set_mobility(unreal.ComponentMobility.MOVABLE)
                lc.set_editor_property('light_function_material', fn)
                k += 1
        log.append('flicker on %d lamps' % k)
    return '\n'.join(log)


build()
