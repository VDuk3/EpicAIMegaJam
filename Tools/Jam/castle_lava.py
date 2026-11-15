# castle_lava.py - owner, 2026-11-15: the dungeon corridor's floor becomes lava, with heat haze in the air over it.
#  - M_PxLava: procedural pixel lava (world-space noise snapped to a pixel grid, three colour steps, slowly churning), unlit.
#  - M_PxHeatHaze: an invisible refracting volume (a two-sided box over the lava) whose normal scrolls upward.
#  - An orange point light over every lava tile (same units as the map's own point lights).
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_lava.py
import unreal

DIR = '/Game/Glyphbound/Castle/Pixel'
MAP = '/Game/Glyphbound/Maps/PastCastle'
GROUPS = (('StaticMeshActor_21287', 'StaticMeshActor_21283', 'StaticMeshActor_21290'),
          ('StaticMeshActor_21269', 'StaticMeshActor_21265', 'StaticMeshActor_21272'))
HAZE_HEIGHT = 70.0
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
tools = unreal.AssetToolsHelpers.get_asset_tools()
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
log = []

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world or not world.get_path_name().startswith(MAP):
    les.load_level(MAP)


def new_material(name):
    path = DIR + '/' + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        MEL.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        e = MEL.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e
    return m, path, node


def link(a, out, b, inp):
    if not MEL.connect_material_expressions(a, out, b, inp):
        log.append('connection failed: %s.%s -> %s.%s' % (a.get_name(), out, b.get_name(), inp))


def lava_master():
    m, path, node = new_material('M_PxLava')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    wp = node(unreal.MaterialExpressionWorldPosition, -1800, 0)
    xy = node(unreal.MaterialExpressionComponentMask, -1600, 0, r=True, g=True, b=False, a=False)
    link(wp, '', xy, '')
    px = node(unreal.MaterialExpressionScalarParameter, -1800, 200, parameter_name='PixelSize', default_value=12.5)
    div = node(unreal.MaterialExpressionDivide, -1400, 0)
    link(xy, '', div, 'A')
    link(px, '', div, 'B')
    fl = node(unreal.MaterialExpressionFloor, -1250, 0)
    link(div, '', fl, '')
    snap = node(unreal.MaterialExpressionMultiply, -1100, 0)
    link(fl, '', snap, 'A')
    link(px, '', snap, 'B')
    time = node(unreal.MaterialExpressionTime, -1800, 350)
    speed = node(unreal.MaterialExpressionScalarParameter, -1800, 450, parameter_name='Churn', default_value=18.0)
    tz = node(unreal.MaterialExpressionMultiply, -1500, 400)
    link(time, '', tz, 'A')
    link(speed, '', tz, 'B')
    pos = node(unreal.MaterialExpressionAppendVector, -900, 100)
    link(snap, '', pos, 'A')
    link(tz, '', pos, 'B')
    noise = node(unreal.MaterialExpressionNoise, -700, 100, scale=0.006, levels=3, output_min=0.0, output_max=1.0,
                 turbulence=False, quality=1)
    link(pos, '', noise, 'World Position')
    # five flat steps of heat
    steps = node(unreal.MaterialExpressionMultiply, -500, 100, const_b=5.0)
    link(noise, '', steps, 'A')
    stf = node(unreal.MaterialExpressionFloor, -380, 100)
    link(steps, '', stf, '')
    q = node(unreal.MaterialExpressionMultiply, -260, 100, const_b=0.25)
    link(stf, '', q, 'A')
    qs = node(unreal.MaterialExpressionSaturate, -140, 100)
    link(q, '', qs, '')
    crust = node(unreal.MaterialExpressionVectorParameter, -500, 300, parameter_name='Crust',
                 default_value=unreal.LinearColor(0.10, 0.012, 0.004, 1))
    mid = node(unreal.MaterialExpressionVectorParameter, -500, 500, parameter_name='Molten',
               default_value=unreal.LinearColor(1.0, 0.16, 0.01, 1))
    hot = node(unreal.MaterialExpressionVectorParameter, -500, 700, parameter_name='Hot',
               default_value=unreal.LinearColor(1.0, 0.75, 0.12, 1))
    a1 = node(unreal.MaterialExpressionMultiply, 0, 200, const_b=2.0)
    link(qs, '', a1, 'A')
    a1s = node(unreal.MaterialExpressionSaturate, 120, 200)
    link(a1, '', a1s, '')
    a2 = node(unreal.MaterialExpressionSubtract, 0, 400, const_b=1.0)
    link(a1, '', a2, 'A')
    a2s = node(unreal.MaterialExpressionSaturate, 120, 400)
    link(a2, '', a2s, '')
    l1 = node(unreal.MaterialExpressionLinearInterpolate, 300, 250)
    link(crust, '', l1, 'A')
    link(mid, '', l1, 'B')
    link(a1s, '', l1, 'Alpha')
    l2 = node(unreal.MaterialExpressionLinearInterpolate, 480, 350)
    link(l1, '', l2, 'A')
    link(hot, '', l2, 'B')
    link(a2s, '', l2, 'Alpha')
    glow = node(unreal.MaterialExpressionScalarParameter, 300, 600, parameter_name='Glow', default_value=2.5)
    em = node(unreal.MaterialExpressionMultiply, 650, 400)
    link(l2, '', em, 'A')
    link(glow, '', em, 'B')
    MEL.connect_material_property(em, '', MP.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


def haze_master():
    m, path, node = new_material('M_PxHeatHaze')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    try:
        m.set_editor_property('refraction_method', unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception as e:
        log.append('haze: refraction_method not set (%s)' % e)
    wp = node(unreal.MaterialExpressionWorldPosition, -1400, 0)
    mx = node(unreal.MaterialExpressionComponentMask, -1200, -100, r=True, g=True, b=False, a=False)
    link(wp, '', mx, '')
    mz = node(unreal.MaterialExpressionComponentMask, -1200, 100, r=False, g=False, b=True, a=False)
    link(wp, '', mz, '')
    # u = x + y (so every wall of the box gets a moving picture), v = z - rising
    dot = node(unreal.MaterialExpressionDotProduct, -1000, -100)
    one = node(unreal.MaterialExpressionConstant2Vector, -1200, -250, r=1.0, g=1.0)
    link(mx, '', dot, 'A')
    link(one, '', dot, 'B')
    time = node(unreal.MaterialExpressionTime, -1400, 300)
    rise = node(unreal.MaterialExpressionScalarParameter, -1400, 400, parameter_name='Rise', default_value=35.0)
    tr = node(unreal.MaterialExpressionMultiply, -1200, 350)
    link(time, '', tr, 'A')
    link(rise, '', tr, 'B')
    v = node(unreal.MaterialExpressionSubtract, -1000, 150)
    link(mz, '', v, 'A')
    link(tr, '', v, 'B')
    uv = node(unreal.MaterialExpressionAppendVector, -800, 0)
    link(dot, '', uv, 'A')
    link(v, '', uv, 'B')
    size = node(unreal.MaterialExpressionScalarParameter, -1000, 400, parameter_name='WaveSize', default_value=22.0)
    uvs = node(unreal.MaterialExpressionDivide, -650, 0)
    link(uv, '', uvs, 'A')
    link(size, '', uvs, 'B')
    # A shimmer, not rising blobs (owner): two fast sine ripples bend the normal sideways.
    uu = node(unreal.MaterialExpressionComponentMask, -500, -150, r=True, g=False, b=False, a=False)
    vv = node(unreal.MaterialExpressionComponentMask, -500, 50, r=False, g=True, b=False, a=False)
    link(uvs, '', uu, '')
    link(uvs, '', vv, '')
    amp = node(unreal.MaterialExpressionScalarParameter, -350, 500, parameter_name='Amount', default_value=0.2)

    def ripple(y, vmul, umul):
        v1 = node(unreal.MaterialExpressionMultiply, -350, y, const_b=vmul)
        link(vv, '', v1, 'A')
        u1 = node(unreal.MaterialExpressionMultiply, -350, y + 80, const_b=umul)
        link(uu, '', u1, 'A')
        ph = node(unreal.MaterialExpressionAdd, -220, y)
        link(v1, '', ph, 'A')
        link(u1, '', ph, 'B')
        sn = node(unreal.MaterialExpressionSine, -100, y, period=1.0)
        link(ph, '', sn, '')
        am = node(unreal.MaterialExpressionMultiply, 20, y)
        link(sn, '', am, 'A')
        link(amp, '', am, 'B')
        return am
    nx = ripple(-200, 1.0, 0.35)
    ny = ripple(100, 1.7, -0.6)
    nxy = node(unreal.MaterialExpressionAppendVector, 160, -50)
    link(nx, '', nxy, 'A')
    link(ny, '', nxy, 'B')
    up = node(unreal.MaterialExpressionConstant, 160, 100, r=1.0)
    ts = node(unreal.MaterialExpressionAppendVector, 300, 0)
    link(nxy, '', ts, 'A')
    link(up, '', ts, 'B')
    ior = node(unreal.MaterialExpressionScalarParameter, -450, 300, parameter_name='IOR', default_value=1.05)
    op = node(unreal.MaterialExpressionConstant, -450, 400, r=0.0)
    MEL.connect_material_property(ts, '', MP.MP_NORMAL)
    MEL.connect_material_property(ior, '', MP.MP_REFRACTION)
    MEL.connect_material_property(op, '', MP.MP_OPACITY)
    MEL.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


lava = lava_master()
haze = haze_master()

actors = eas.get_all_level_actors()
by_name = {a.get_name(): a for a in actors}
ref_light = next((a for a in actors if isinstance(a, unreal.PointLight)), None)
for a in actors:                                   # a second run replaces its own actors
    if a.get_actor_label().startswith(('LavaHaze_', 'LavaGlow_')):
        eas.destroy_actor(a)

cube = unreal.load_asset('/Engine/BasicShapes/Cube')
n_tiles = 0
n_lights = 0
for gi, names in enumerate(GROUPS):
    lo = hi = None
    for nm in names:
        a = by_name.get(nm)
        if not a:
            log.append('missing actor %s' % nm)
            continue
        c = a.static_mesh_component
        a.modify()
        for i in range(max(1, c.get_num_materials())):
            c.set_material(i, lava)
        n_tiles += 1
        o, e = a.get_actor_bounds(False)
        mn = unreal.Vector(o.x - e.x, o.y - e.y, o.z - e.z)
        mxv = unreal.Vector(o.x + e.x, o.y + e.y, o.z + e.z)
        lo = mn if lo is None else unreal.Vector(min(lo.x, mn.x), min(lo.y, mn.y), min(lo.z, mn.z))
        hi = mxv if hi is None else unreal.Vector(max(hi.x, mxv.x), max(hi.y, mxv.y), max(hi.z, mxv.z))
        lt = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(o.x, o.y, o.z + e.z + 110.0))
        lt.set_actor_label('LavaGlow_%d_%s' % (gi, nm[-5:]))
        lt.set_folder_path('Lava')
        lc = lt.point_light_component
        if ref_light:
            lc.set_editor_property('intensity_units', ref_light.point_light_component.get_editor_property('intensity_units'))
            lc.set_editor_property('intensity', ref_light.point_light_component.get_editor_property('intensity') * 1.5)
        lc.set_editor_property('light_color', unreal.Color(r=255, g=96, b=20, a=255))
        lc.set_editor_property('attenuation_radius', 900.0)
        lc.set_editor_property('cast_shadows', False)
        n_lights += 1
    if lo is None:
        continue
    top = hi.z
    sx, sy = hi.x - lo.x, hi.y - lo.y
    box = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, top + HAZE_HEIGHT / 2 + 2.0))
    box.set_actor_label('LavaHaze_%d' % gi)
    box.set_folder_path('Lava')
    bc = box.static_mesh_component
    bc.set_static_mesh(cube)
    bc.set_material(0, haze)
    bc.set_collision_profile_name('NoCollision')
    bc.set_editor_property('cast_shadow', False)
    box.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, HAZE_HEIGHT / 100.0))
    log.append('group %d: floor top z=%.0f, %.0f x %.0f cm' % (gi, top, sx, sy))

saved = les.save_current_level()
log.append('lava on %d tiles, %d lights (ref %s), saved=%s' % (n_tiles, n_lights, ref_light.get_actor_label() if ref_light else None, saved))
unreal.log('\n'.join(log))
'\n'.join(log)
