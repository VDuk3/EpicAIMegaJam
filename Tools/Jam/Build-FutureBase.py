# Build-FutureBase.py - builds the future base, slice 1 (Design/Levels/FUTURE_BASE_SLICE1.md) in the Unreal Editor.
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/Build-FutureBase.py
#
# How it builds:
#  - "Sections" are groups of pieces taken from a kit's own demo map (Design/Levels/kits/*.json, written by
#    extract_level_pieces.py): a corridor bay, a hall shell. They are placed again with the same relative positions,
#    materials and lights, so every piece sits where its author meant it to sit.
#  - Our own level design (landings, pits, stairs, catwalks, cover, doors) is placed piece by piece below, each with
#    a reason in the plan.
#  - Kit meshes never collide. What blocks the player is a set of plain boxes (floors, walls, steps) placed just
#    behind the visible surfaces, so collision is simple and matches what is seen.
# Idempotent: every actor carries the tag GBBase and a stage tag; a run deletes and rebuilds the stages named in
# STAGES (all of them when the list is empty).
import json
import math

MAP = '/Game/Glyphbound/Maps/FutureBase'
TAG = 'GBBase'
STAGES = []          # e.g. ['hall'] to rebuild one stage only
KIT_DIR = 'C:/GameJam/Design/Levels/kits/'
FLOOR_Z_HTB = 650.0  # the floor height of the High Tech Base demo map
# Our frame: +X is the way the player advances, the corridor's axis is Y = 0, every main floor is Z = 0.
HTB_ORIGIN = (900.0, 150.0, FLOOR_Z_HTB)   # demo point that becomes our (0, 0, 0): the corridor's first frame

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eal = unreal.EditorAssetLibrary
log = []
_assets = {}


def asset(path):
    if path not in _assets:
        _assets[path] = unreal.load_asset(path.split('.')[0]) if path else None
    return _assets[path]


def tag(actor, stage, folder):
    actor.tags = [unreal.Name(TAG), unreal.Name('stage_' + stage)]
    actor.set_folder_path(unreal.Name('Base/%s/%s' % (stage, folder)))


def clear(stage):
    n = 0
    for a in eas.get_all_level_actors():
        if a.actor_has_tag(TAG) and a.actor_has_tag('stage_' + stage):
            eas.destroy_actor(a)
            n += 1
    return n


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def mesh(stage, folder, path, loc, r=None, scale=(1, 1, 1), mats=None, shadow=True, collide=False):
    a = eas.spawn_actor_from_object(asset(path), unreal.Vector(*loc), r or rot())
    a.set_actor_scale3d(unreal.Vector(*scale))
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.STATIC)
    for i, m in enumerate(mats or []):
        if m:
            c.set_material(i, asset(m))
    if path.startswith('/Game/ModularSciFiStation/'):
        # The pack's steel is painted white (it is a Mars station). In our base it is dark service steel.
        for i, slot in enumerate(asset(path).static_materials):
            name = slot.material_interface.get_name() if slot.material_interface else ''
            if name in STEEL:
                c.set_material(i, asset(STEEL[name]))
    c.set_collision_profile_name('BlockAll' if collide else 'NoCollision')
    c.set_editor_property('cast_shadow', shadow)
    tag(a, stage, folder)
    return a


def light(stage, folder, cls, loc, r, d):
    a = eas.spawn_actor_from_class(getattr(unreal, cls), unreal.Vector(*loc), r)
    c = a.light_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
    c.set_editor_property('intensity', d['intensity'] * d.get('boost', 1.0))
    col = d['color']
    c.set_editor_property('light_color', unreal.Color(r=int(col[0]), g=int(col[1]), b=int(col[2]), a=255))
    c.set_editor_property('cast_shadows', bool(d.get('shadow', False)))
    for p in ('attenuation_radius', 'source_width', 'source_height', 'barn_door_angle', 'barn_door_length', 'source_radius'):
        if p in d:
            try:
                c.set_editor_property(p, d[p])
            except Exception:
                pass
    tag(a, stage, folder)
    return a


KIT_MATS = '/Game/Glyphbound/Materials/Kit'
# pack material -> (our darker instance of it, tint). The tint multiplies the pack's own colour texture.
STEEL_DEF = {'MI_interior_trim_01': ('MI_GB_SteelTrim', '/Game/ModularSciFiStation/Materials/Trims/MI_interior_trim_01', (0.17, 0.18, 0.20)),
             'MI_grid_floor': ('MI_GB_SteelGrid', '/Game/ModularSciFiStation/Materials/Metal/MI_grid_floor', (0.24, 0.24, 0.25)),
             'MI_metal_01': ('MI_GB_SteelPlate', '/Game/ModularSciFiStation/Materials/Metal/MI_metal_01', (0.30, 0.30, 0.31)),
             'MI_exterior_trim_01': ('MI_GB_SteelTruss', '/Game/ModularSciFiStation/Materials/Trims/MI_exterior_trim_01', (0.34, 0.33, 0.32))}
STEEL = {}


def ensure_steel():
    """Creates (once) our dark instances of the pack's four steel materials and fills STEEL."""
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for src, (name, parent, tint) in STEEL_DEF.items():
        path = KIT_MATS + '/' + name
        if not eal.does_asset_exist(path):
            mi = tools.create_asset(name, KIT_MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(mi, unreal.load_asset(parent))
        mi = unreal.load_asset(path)
        mel.set_material_instance_vector_parameter_value(mi, 'BaseColor01', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        mel.update_material_instance(mi)
        eal.save_asset(path, only_if_is_dirty=False)
        STEEL[src] = path


# ---- sections taken from a kit's demo map ----
_kits = {}


def kit(name):
    if name not in _kits:
        _kits[name] = json.load(open(KIT_DIR + name + '.json'))['items']
    return _kits[name]


def select(items, x=None, y=None, z=None, keep=None, drop=None):
    """Items whose position is inside the given ranges (demo coordinates). keep/drop filter by mesh name part."""
    out = []
    for it in items:
        lx, ly, lz = it['loc']
        if x and not (x[0] <= lx < x[1]):
            continue
        if y and not (y[0] <= ly < y[1]):
            continue
        if z and not (z[0] <= lz < z[1]):
            continue
        name = it.get('mesh', it.get('class', ''))
        if keep and not any(k in name for k in keep):
            continue
        if drop and any(k in name for k in drop):
            continue
        out.append(it)
    return out


def place_section(stage, folder, items, origin, to=(0.0, 0.0, 0.0), yaw=0.0, light_boost=1.0):
    """Places the items again: the demo point `origin` lands on our point `to`, turned `yaw` degrees around it."""
    rad = math.radians(yaw)
    cs, sn = math.cos(rad), math.sin(rad)
    turn = rot(yaw=yaw)
    n = 0
    for it in items:
        dx, dy, dz = it['loc'][0] - origin[0], it['loc'][1] - origin[1], it['loc'][2] - origin[2]
        loc = (to[0] + dx * cs - dy * sn, to[1] + dx * sn + dy * cs, to[2] + dz)
        r = unreal.MathLibrary.compose_rotators(rot(*it['rot']), turn)
        if it['kind'] == 'mesh':
            mesh(stage, folder, it['mesh'], loc, r, it['scale'], it['mats'], it.get('shadow', True))
        else:
            d = dict(it)
            d['boost'] = light_boost
            light(stage, folder, it['class'], loc, r, d)
        n += 1
    return n


def box(stage, folder, x, y, z, mat=None, visible=True):
    """A plain blocking box from (x0, x1), (y0, y1), (z0, z1): the collision behind what is seen."""
    a = eas.spawn_actor_from_object(asset('/Engine/BasicShapes/Cube'), unreal.Vector((x[0] + x[1]) / 2, (y[0] + y[1]) / 2, (z[0] + z[1]) / 2), rot())
    a.set_actor_scale3d(unreal.Vector(abs(x[1] - x[0]) / 100.0, abs(y[1] - y[0]) / 100.0, abs(z[1] - z[0]) / 100.0))
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.STATIC)
    c.set_material(0, asset(mat or BACKING))
    c.set_collision_profile_name('BlockAll')
    if not visible:
        a.set_actor_hidden_in_game(True)
    tag(a, stage, folder)
    return a


BACKING = '/Game/High_Tech_Base/Materials/MI_Rubber_02'   # near-black rubber: a gap between kit pieces reads as shadow


# ---- stages ----
async def stage_world():
    s = 'world'
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), rot())
    ppv.set_editor_property('unbound', True)
    st = ppv.get_editor_property('settings')
    # The exposure and tone the kit's author lit his demo with (read from its post process volume).
    for name, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM), ('auto_exposure_bias', 4.0),
                        ('auto_exposure_min_brightness', 0.6), ('auto_exposure_max_brightness', 8.0), ('bloom_intensity', 0.6),
                        ('white_temp', 7500.0), ('film_slope', 0.68), ('film_toe', 0.626),
                        ('lumen_scene_lighting_quality', 2.0), ('lumen_final_gather_quality', 2.0)):
        st.set_editor_property('override_' + name, True)
        st.set_editor_property(name, value)
    ppv.set_editor_property('settings', st)
    tag(ppv, s, 'Light')
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(150, 0, 110), rot(yaw=0))
    tag(ps, s, 'Gameplay')
    ues.get_editor_world().get_world_settings().set_editor_property('kill_z', -2500.0)
    log.append('world: post process, player start')


async def stage_corridor():
    s = 'corridor'
    htb = kit('htb_demo')
    n = place_section(s, 'Kit', select(htb, x=(880, 2905)), HTB_ORIGIN)
    box(s, 'Collision', (-20, 2020), (-520, 520), (-60, -2))
    log.append('corridor: %d pieces from the kit demo (two bays)' % n)


async def stage_hall():
    s = 'hall'
    htb = kit('htb_demo')
    n = place_section(s, 'Kit', select(htb, x=(2905, 99999)), HTB_ORIGIN)
    log.append('hall: %d pieces from the kit demo (hangar shell)' % n)


async def stage_control():
    s = 'control'
    htb = kit('htb_demo')
    n = place_section(s, 'Kit', select(htb, x=(-99999, 880)), HTB_ORIGIN)
    log.append('control: %d pieces from the kit demo (control room)' % n)


# ---- our own level design, piece by piece ----
MSS = '/Game/ModularSciFiStation/Environment/'
HTB = '/Game/High_Tech_Base/Models/'
DECK = 20.0   # a Modular SciFi Station floor tile is 20 cm thick above its origin: origin Z = walking surface - 20


def deck(s, x0, x1, y0, y1, z, grate=True):
    """A steel deck of 2 x 2 m tiles from (x0, y0) to (x1, y1), walking surface at z. Grating where services or air
    must pass (catwalks, bridges); solid plate elsewhere."""
    m = MSS + 'Floor/SM_floor_200x200_0%d' % (2 if grate else 1)
    x = x0
    while x < x1 - 1:
        y = y0
        while y < y1 - 1:
            mesh(s, 'Deck', m, (x, y + 200, z - DECK), collide=True)   # the tile spans x..x+200, y-200..y from its origin
            y += 200
        x += 200


def post(s, x, y, z_top):
    """A truss post from the floor up to the underside of a deck at z_top (2 m and 1 m pieces stacked)."""
    z = 0.0
    while z_top - DECK - z >= 199:
        mesh(s, 'Posts', MSS + 'Scaffold/SM_scaffold_200_01', (x, y, z), collide=True)
        z += 200
    while z_top - DECK - z >= 79:
        mesh(s, 'Posts', MSS + 'Scaffold/SM_scaffold_100_01', (x, y, z), collide=True)
        z += 100


def rail(s, x0, y0, x1, y1, z, mesh_grate=True):
    """Railing along a straight edge from (x0, y0) to (x1, y1), in 2 m pieces. The open hex mesh lets the player see
    and be seen through it; it is never used as cover."""
    m = MSS + 'Railing/SM_railing_200_0%d' % (2 if mesh_grate else 1)
    length = math.hypot(x1 - x0, y1 - y0)
    n = int(round(length / 200.0))
    yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
    for i in range(n):
        t = (i + 0.5) / n
        mesh(s, 'Rails', m, (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z), rot(yaw=yaw), collide=True)


def stairs(s, x, y, z, yaw, steps, rails=(True, True)):
    """A flight of `steps` stair modules (each 1.5 m long, 1 m up) starting at the bottom point (x, y, z) and
    climbing along `yaw` (0 = +X). The module itself climbs towards its own -X, so it is turned by yaw + 180."""
    rad = math.radians(yaw)
    for k in range(steps):
        p = (x + math.cos(rad) * 150 * k, y + math.sin(rad) * 150 * k, z + 100 * k)
        mesh(s, 'Stairs', MSS + 'Stairs/SM_stairs_01', p, rot(yaw=yaw + 180), collide=True)
        if rails[0]:
            mesh(s, 'Stairs', MSS + 'Railing/SM_railing_stairs_left_02', p, rot(yaw=yaw + 180), collide=True)
        if rails[1]:
            mesh(s, 'Stairs', MSS + 'Railing/SM_railing_stairs_right_02', p, rot(yaw=yaw + 180), collide=True)


def work_light(s, x, y, z, yaw, color=(255, 236, 214), intensity=3.0, radius=900.0):
    """A wall lamp and the light it gives: every light in the level has a fixture the player can see."""
    mesh(s, 'Lamps', MSS + 'Lamps/Meshes/SM_lamp_200_01', (x, y, z), rot(yaw=yaw))
    light(s, 'Lamps', 'RectLight', (x, y, z - 25), rot(pitch=-90, yaw=yaw),
          {'intensity': intensity, 'color': color, 'attenuation_radius': radius, 'source_width': 30.0, 'source_height': 190.0, 'shadow': False})


CAT_Z = 300.0   # the catwalk: three stair modules above the bay floor; 2.5 m of head room under the ceiling


async def stage_hall_design():
    """The transfer bay's level design (plan 3.6), built inside the kit's hangar shell.
    Bay floor: X 2100..5300, Y -450..300 (lane), branch X 3004..3754 up to the freight door at Y 2401.
    The service trench with the machines (Y < -500, 1 m down) is behind a railing: seen, not played."""
    s = 'hall_design'
    # North catwalk: the supervisors' walkway along the wall, from above the bay entry to the far end. It crosses
    # the mouth of the branch as a bridge, so whoever comes in through the freight door passes under it.
    deck(s, 2550, 4750, 100, 300, CAT_Z)
    for x in (2550, 2950, 3800, 4200, 4750):
        post(s, x, 110, CAT_Z)
    rail(s, 2550, 100, 4150, 100, CAT_Z)                 # lane side, west part
    rail(s, 4350, 100, 4750, 100, CAT_Z)                 # lane side, east of the cross bridge
    rail(s, 3004, 300, 3804, 300, CAT_Z)                 # over the branch mouth, north side
    # West stair: straight, against the wall, the first thing seen on entering the bay.
    stairs(s, 2100, 200, 0, 0, 3, rails=(False, True))
    # East stair: straight down towards the freight end, so the catwalk is a flanking route, not a dead end.
    stairs(s, 5200, 200, 0, 180, 3, rails=(True, False))
    # Cross bridge: from the catwalk over the lane to a lookout above the trench railing. Its lane side is open:
    # the drop back to the bay floor closes the loop.
    deck(s, 4150, 4350, -500, 100, CAT_Z)
    rail(s, 4150, -500, 4150, 100, CAT_Z)
    rail(s, 4350, 100, 4350, -500, CAT_Z)
    deck(s, 3950, 4550, -700, -500, CAT_Z)
    for x, y in ((3960, -510), (4540, -510), (3960, -690), (4540, -690), (4160, -250), (4340, -250)):
        post(s, x, y, CAT_Z)
    rail(s, 3950, -700, 4550, -700, CAT_Z)
    rail(s, 3950, -700, 3950, -500, CAT_Z)
    rail(s, 4550, -500, 4550, -700, CAT_Z)
    # Cover. Crates are what a transfer bay moves, and they stand where a loader would leave them: by the trench
    # railing, at the foot of the lookout, by the freight doors. Low (1.05 m) so the bay still reads at a glance.
    crate = HTB + 'SM_Prop_02'
    for x, y, yaw, z in ((2900, -380, 8, 0), (3010, -390, -4, 0), (2955, -385, 20, 105),      # by the railing, one stacked
                         (3550, -120, 0, 0), (3550, -230, 90, 0),                                # lane centre, before the bridge
                         (4700, -330, 12, 0), (4810, -320, 0, 0),                               # past the lookout
                         (3200, 1500, 0, 0), (3310, 1510, 15, 0), (3600, 1900, 0, 0)):          # in the branch, towards the freight door
        mesh(s, 'Cover', crate, (x, y, z + 2), rot(yaw=yaw), collide=True)
    # Two tall machine blocks break the 32 m sight line of the lane (the same unit that stands in the trench).
    for x, y in ((3350, 180), (4980, -60)):
        mesh(s, 'Cover', HTB + 'SM_Prop_HS', (x, y, 124), collide=True)
    # Work lights on the wall above the catwalk and under the lookout.
    for x in (2750, 3404, 4050, 4650):
        work_light(s, x, 290, 520, -90)
    log.append('hall design: catwalk, two stairs, cross bridge and lookout, 10 crates, 2 machine blocks')


async def stage_arrival():
    """Where the player arrives (plan 3.1), inside the kit's control room: the seal on a low round plinth in the
    middle of the watch consoles, with four emitters around it and their cable trays running to it."""
    s = 'arrival'
    cx, cy = -760.0, 60.0
    mesh(s, 'Seal', MSS + 'Scaffold/SM_hydraulic_pillar_400_01', (cx, cy, 0), scale=(1, 1, 0.15), collide=True)       # plinth, 30 cm
    for yaw in (0, 90, 180, 270):
        mesh(s, 'Seal', MSS + 'Floor/SM_floor_round_02', (cx, cy, 30 - DECK + 1), rot(yaw=yaw))                     # grating disc
    mesh(s, 'Seal', '/Game/SciFiWorld/Meshes/SM_LabHoloProjector01_Base', (cx, cy, 31))
    mesh(s, 'Seal', '/Game/SciFiWorld/Meshes/SM_LabHoloProjector01_Volume', (cx, cy, 61), scale=(1.4, 1.4, 2.2))
    light(s, 'Seal', 'PointLight', (cx, cy, 190), rot(), {'intensity': 6.0, 'color': (120, 215, 255), 'attenuation_radius': 900.0, 'shadow': False})
    for i, yaw in enumerate((45, 135, 225, 315)):
        rad = math.radians(yaw)
        ex, ey = cx + math.cos(rad) * 330, cy + math.sin(rad) * 330
        mesh(s, 'Seal', HTB + 'SM_Prop_04', (ex, ey, 0), rot(yaw=yaw), collide=True)                                 # emitter drum
        mesh(s, 'Seal', MSS + 'Props/CableCover/SM_cable_cover_100_01', (cx + math.cos(rad) * 300, cy + math.sin(rad) * 300, 0), rot(yaw=yaw))
    for a in eas.get_all_level_actors():
        if a.get_class().get_name() == 'PlayerStart' and a.actor_has_tag(TAG):
            # In front of the seal, not on it: the player is thrown out of the light, facing the way out.
            a.set_actor_location(unreal.Vector(cx + 260, cy, 100), False, False)
            a.set_actor_rotation(rot(yaw=0), False)
    log.append('arrival: seal plinth, emitters, cable trays, player start in front of the seal')


# Review spots: (name, x, y, floor z, yaw). The game's smoke block "tour" puts the player on each one and takes a
# screenshot through the real game camera (gb.Smoke tour).
TOUR = [('01_arrival', -500, 60, 0, 0), ('02_arrival_back', -150, 0, 0, 180), ('03_corridor', 300, -50, 0, 0),
        ('04_corridor_alcove', 1500, 0, 0, 90), ('05_enter_bay', 2050, -200, 0, 12), ('06_west_stair', 2000, -100, 0, 35),
        ('07_catwalk', 2700, 200, 300, -5), ('08_bridge', 4250, -100, 300, -90), ('09_lookout', 4250, -600, 300, 150),
        ('10_bay_floor_east', 4600, -150, 0, 180), ('11_branch', 3380, 2200, 0, -90), ('12_under_catwalk', 3380, 500, 0, -90)]


async def stage_tour():
    s = 'tour'
    for name, x, y, z, yaw in TOUR:
        a = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z), rot(yaw=yaw))
        a.tags = [unreal.Name(TAG), unreal.Name('stage_' + s), unreal.Name('GBTour'), unreal.Name('tour_' + name)]
        a.set_folder_path(unreal.Name('Base/tour'))
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(1900, 650, 300), rot())
    nav.set_actor_scale3d(unreal.Vector(80.0, 40.0, 6.0))    # the volume's brush is a 2 m cube
    tag(nav, s, 'Gameplay')
    log.append('tour: %d review spots, nav bounds' % len(TOUR))


def enable_collision():
    """Kit pieces block exactly as they look: complex collision used as simple on every mesh the map uses, except
    decals and loose cables. (The kits' own simple hulls close doorways: found on the deleted site map.)"""
    done = 0
    for a in eas.get_all_level_actors():
        if not (isinstance(a, unreal.StaticMeshActor) and a.actor_has_tag(TAG)):
            continue
        c = a.static_mesh_component
        sm = c.static_mesh
        if not sm or sm.get_path_name().startswith('/Engine/'):
            continue
        name = sm.get_name()
        if any(k in name for k in ('Decal', 'Top_Cables', 'Pipe_02', 'Chair', 'Top_Chain', 'Volume', 'cable_cover', 'lamp_')):
            c.set_collision_profile_name('NoCollision')
            continue
        bs = sm.get_editor_property('body_setup')
        if bs and bs.get_editor_property('collision_trace_flag') != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
            bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            eal.save_asset(sm.get_path_name().split('.')[0], only_if_is_dirty=False)
            done += 1
        c.set_collision_profile_name('BlockAll')
        a.modify()
    log.append('collision: complex-as-simple set on %d mesh assets' % done)


ALL = [('world', stage_world), ('corridor', stage_corridor), ('hall', stage_hall), ('control', stage_control),
       ('hall_design', stage_hall_design), ('arrival', stage_arrival), ('tour', stage_tour)]

# ---- run ----
if not eal.does_asset_exist(MAP):
    les.new_level(MAP, False)
    les.save_current_level()
elif ues.get_editor_world().get_path_name().split('.')[0] != MAP:
    les.load_level(MAP)
await asyncio.sleep(0)
ensure_steel()
for name, fn in ALL:
    if STAGES and name not in STAGES:
        continue
    removed = clear(name)
    await fn()
    await asyncio.sleep(0)
enable_collision()
les.save_current_level()
'\n'.join(log)
