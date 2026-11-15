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

# The smoke carpet (owner, later the same day: "dense fog on the floor, strong, with Niagara Fluids", then "strong on
# the floor, NOT up high like mist: corridor floor smoke"): along the whole path a thick, opaque layer 20-60 cm deep
# with a defined top, like stage smoke rolling on the floor, kept out of eye level. Per area: a flat AGBFluidFog box
# (about 90 cm high, sitting on the floor: a few cells high, so it stays cheap) with a very strong fill, and close-set
# low planes, nearly opaque up to most of their height and gone at their top (the carpet's surface).
# x: the fluid box's X range (None = the area's; the worn corridor's last 15 m and the dark corridor keep the boxes
# Build-FutureBase.py placed, made denser with a FogDensity tag below). h: carpet depth, cm. Densest in the pit.
FLOOR = {
    'Worn': dict(h=45, x=(6954, 9500), fill=8.0, cells=96, opacity=0.55, cut=0.8, color=(0.50, 0.55, 0.56, 0.14), inset=60),
    'Nest': dict(h=50, x=None, fill=10.0, cells=96, opacity=0.6, cut=0.75, color=(0.62, 0.57, 0.54, 0.32)),
    'Dark': dict(h=55, x=None, fill=0, cells=0, opacity=0.62, cut=0.7, color=(0.45, 0.52, 0.62, 0.14), inset=8),
    'Pit': dict(h=65, x=None, fill=12.0, cells=128, opacity=0.7, cut=0.65, color=(0.60, 0.62, 0.62, 0.36)),
}
FLUID_H = 90.0   # the carpet's fluid boxes: floor to 90 cm
# Build-FutureBase.py's own fog boxes (not removed by this script): their fill, multiplied, by the box's centre X range.
BASE_FOG = [((9500, 11000), 6.0), ((12254, 14762), 8.0)]

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


CARPET_MAT = SMOKE_DIR + '/M_GB_SmokeCarpet'
NOISE = '/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq'
LAYERS = 12   # horizontal slices of the carpet, floor to top


def carpet_material():
    """M_GB_SmokeCarpet: unlit, translucent, for horizontal slices stacked over the floor. One drifting world-space
    noise N (big billows mixed with finer detail) is read as a height map: a slice at relative height t (0 at Base, 1
    at Base + Height) shows only where N > t * CutMax, so the stacked slices draw the same rolling humps in 3D, solid
    below and torn at the top, like stage smoke. Soft where a slice meets a wall or a prop (depth fade)."""
    eal = unreal.EditorAssetLibrary
    MEL = unreal.MaterialEditingLibrary
    if eal.does_asset_exist(CARPET_MAT):
        mat = unreal.load_asset(CARPET_MAT)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GB_SmokeCarpet', SMOKE_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)

    def node(cls, x, y, **props):
        e = MEL.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, inp, out=''):
        MEL.connect_material_expressions(a, out, b, inp)

    def op(cls, a, b, x, y):
        e = node(cls, x, y)
        link(a, e, 'A')
        link(b, e, 'B')
        return e

    def const(v, x, y):
        return node(unreal.MaterialExpressionConstant, x, y, r=v)

    def scalar(name, v, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)

    tex = unreal.load_asset(NOISE)
    wp = node(unreal.MaterialExpressionWorldPosition, -1800, 0)
    xy = node(unreal.MaterialExpressionComponentMask, -1600, 0, r=True, g=True, b=False, a=False)
    link(wp, xy, '')
    wz = node(unreal.MaterialExpressionComponentMask, -1600, 300, r=False, g=False, b=True, a=False)
    link(wp, wz, '')

    def layer(scale_cm, sx, sy, y):
        d = op(unreal.MaterialExpressionDivide, xy, const(scale_cm, -1400, y + 60), -1300, y)
        p = node(unreal.MaterialExpressionPanner, -1150, y, speed_x=sx, speed_y=sy)
        link(d, p, 'Coordinate')
        t = node(unreal.MaterialExpressionTextureSample, -950, y, texture=tex)
        link(p, t, 'UVs')
        m = node(unreal.MaterialExpressionComponentMask, -750, y, r=True, g=False, b=False, a=False)
        link(t, m, '')
        return m

    big = layer(9000.0, 0.010, 0.004, -300)       # billows of about 1.5 m
    fine = layer(2600.0, -0.022, 0.012, -100)     # wisps of about 40 cm drifting the other way
    n = op(unreal.MaterialExpressionLinearInterpolate, big, fine, -550, -200)
    link(const(0.3, -700, -150), n, 'Alpha')
    base = scalar('Base', -600.0, -1400, 300)
    height = scalar('Height', 60.0, -1400, 380)
    t = op(unreal.MaterialExpressionDivide, op(unreal.MaterialExpressionSubtract, wz, base, -1200, 300), height, -1050, 320)
    ts = node(unreal.MaterialExpressionSaturate, -900, 320)
    link(t, ts, '')
    cut = op(unreal.MaterialExpressionMultiply, ts, scalar('CutMax', 0.85, -900, 420), -750, 360)
    soft = scalar('Soft', 0.12, -750, 460)
    a = op(unreal.MaterialExpressionDivide, op(unreal.MaterialExpressionSubtract, n, cut, -550, 300), soft, -400, 340)
    asat = node(unreal.MaterialExpressionSaturate, -250, 340)
    link(a, asat, '')
    o = op(unreal.MaterialExpressionMultiply, asat, scalar('Opacity', 0.5, -250, 440), -100, 380)
    df = node(unreal.MaterialExpressionDepthFade, 50, 380, fade_distance_default=30.0)
    link(o, df, 'Opacity')
    MEL.connect_material_property(df, '', unreal.MaterialProperty.MP_OPACITY)
    col = node(unreal.MaterialExpressionVectorParameter, -550, -500, parameter_name='SmokeColor', default_value=unreal.LinearColor(0.55, 0.57, 0.58, 1.0))
    shade = op(unreal.MaterialExpressionAdd, const(0.45, -550, -380), op(unreal.MaterialExpressionMultiply, n, const(0.55, -700, -330), -550, -330), -400, -360)
    lit = op(unreal.MaterialExpressionMultiply, shade, op(unreal.MaterialExpressionAdd, const(0.6, -400, -280), ts, -300, -280), -250, -330)
    e = op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, col, scalar('Brightness', 0.3, -400, -460), -250, -480), lit, -100, -400)
    MEL.connect_material_property(e, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    eal.save_asset(CARPET_MAT, only_if_is_dirty=False)
    return mat


def carpet_instance(name, z0, h, opacity, color, cut):
    eal = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    path = SMOKE_DIR + '/MI_GB_Carpet_' + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_GB_Carpet_' + name, SMOKE_DIR, unreal.MaterialInstanceConstant,
                                                                    unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', unreal.load_asset(CARPET_MAT))
    for k, v in (('Base', z0), ('Height', h), ('Opacity', opacity), ('CutMax', cut), ('Brightness', color[3])):
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.set_material_instance_vector_parameter_value(mi, 'SmokeColor', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


def carpet(eas, plane, mi, name, box):
    """LAYERS horizontal slices over the floor of the box (x0, x1, y0, y1, z0, z1), the lowest 3 cm up."""
    x0, x1, y0, y1, z0, z1 = box
    for i in range(LAYERS):
        z = z0 + 3.0 + (z1 - z0 - 3.0) * i / float(LAYERS - 1)
        m = eas.spawn_actor_from_object(plane, unreal.Vector((x0 + x1) / 2.0, (y0 + y1) / 2.0, z), unreal.Rotator(0, 0, 0))
        m.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, 1.0))
        sm = m.static_mesh_component
        sm.set_material(0, mi)
        sm.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        sm.set_editor_property('cast_shadow', False)
        tagged(m, name)
        m.set_actor_label('Smoke_%s_Carpet%02d' % (name, i))
    return LAYERS


def tagged(a, folder):
    a.tags = list(a.tags) + [unreal.Name(TAG)]
    a.set_folder_path(unreal.Name('Smoke/' + folder))


def build():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_name() == 'FutureBase', 'open FutureBase first, not ' + world.get_name()
    gone = 0
    for a in eas.get_all_level_actors():
        if unreal.Name(TAG) in a.tags or unreal.Name('SmokeTest') in a.tags:
            eas.destroy_actor(a)
            gone += 1
    log = ['removed %d' % gone]
    plane = unreal.load_asset(PLANE)
    carpet_material()
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
                fz1 = z0 + FLUID_H
                f = eas.spawn_actor_from_class(unreal.GBFluidFog, unreal.Vector((fx0 + fx1) / 2.0, c.y, (z0 + fz1) / 2.0), unreal.Rotator(0, 0, 0))
                f.set_editor_property('box_extent', unreal.Vector((fx1 - fx0) / 2.0, (y1 - y0) / 2.0, (fz1 - z0) / 2.0))
                f.tags = [unreal.Name('FogDensity=%g' % fl['fill']), unreal.Name('FogResolution=%d' % fl['cells'])]
                tagged(f, name)
                f.set_actor_label('Smoke_%s_FloorFluid' % name)
            k = carpet(eas, plane, carpet_instance(name, z0, fl['h'], fl['opacity'], fl['color'], fl['cut']), name + '_Floor', (x0, x1, y0 + fl.get('inset', 0), y1 - fl.get('inset', 0), z0, z0 + fl['h']))
            log.append('%s floor: fluid x%g, %d carpet slices' % (name, fl['fill'], k))
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
