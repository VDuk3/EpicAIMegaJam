# castle_lava.py - owner, 2026-11-15: the dungeon corridor's floor becomes lava, glowing orange.
#  - M_PxLava: procedural pixel lava (world-space noise snapped to a pixel grid, three colour steps, slowly churning), unlit.
#  - M_PxHeatHaze: an invisible refracting volume (a two-sided box over the lava) whose normal scrolls upward.
#  - An orange point light over every lava tile (same units as the map's own point lights).
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_lava.py
import unreal

DIR = '/Game/Glyphbound/Castle/Pixel'
MAP = '/Game/Glyphbound/Maps/PastCastle'
GROUPS = (('StaticMeshActor_21287', 'StaticMeshActor_21283', 'StaticMeshActor_21290'),
          ('StaticMeshActor_21269', 'StaticMeshActor_21265', 'StaticMeshActor_21272'))
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


lava = lava_master()
# No heat haze: the owner rejected every try (refracting box, Niagara haze sprites). Their assets are removed.
for old in ('M_PxHeatHaze', 'M_PxHeatSprite', 'NS_PxLavaHeat'):
    if eal.does_asset_exist(DIR + '/' + old):
        eal.delete_asset(DIR + '/' + old)

actors = eas.get_all_level_actors()
by_name = {a.get_name(): a for a in actors}
ref_light = next((a for a in actors if isinstance(a, unreal.PointLight)), None)
for a in actors:                                   # a second run replaces its own actors
    if a.get_actor_label().startswith(('LavaHaze_', 'LavaGlow_')):
        eas.destroy_actor(a)

n_tiles = 0
boxes = []
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
        lc.set_editor_property('attenuation_radius', 520.0)   # the corridor is 8 m wide: the glow stops at its walls
        lc.set_lighting_channels(False, True, False)   # only the corridor's own pieces take this light (below)
        lc.set_editor_property('cast_shadows', True)   # without shadows the glow went through the walls (owner)
        n_lights += 1
    if lo is None:
        continue
    boxes.append((lo, hi))
    log.append('group %d: floor top z=%.0f, %.0f x %.0f cm' % (gi, hi.z, hi.x - lo.x, hi.y - lo.y))

# The glow lit the cell behind the barred wall (owner: "the lava's light goes through the jail wall"). The lava lights
# are on lighting channel 1, and only the pieces standing over the lava (its walls, vault and pillars) are on it.
n_ch = 0
for a in eas.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    o, e = a.get_actor_bounds(False)
    c = a.static_mesh_component
    inside = any(lo.x - 80 <= o.x <= hi.x + 80 and lo.y - 80 <= o.y <= hi.y + 80 and lo.z - 100 <= o.z <= hi.z + 900 for lo, hi in boxes)
    if inside:
        c.set_lighting_channels(True, True, False)
        n_ch += 1
log.append('%d pieces of the lava corridor take the lava light' % n_ch)
saved = les.save_current_level()
log.append('lava on %d tiles, %d lights (ref %s), saved=%s' % (n_tiles, n_lights, ref_light.get_actor_label() if ref_light else None, saved))
unreal.log('\n'.join(log))
'\n'.join(log)
