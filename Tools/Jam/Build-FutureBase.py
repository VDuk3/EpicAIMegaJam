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
            dark = dark_instance(slot.material_interface) if slot.material_interface else None
            if dark:
                c.set_material(i, asset(dark))
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


PANEL_TINT = (0.34, 0.36, 0.40)   # the pack's white wall and ceiling panels, brought down to painted grey-blue steel
NO_TINT = ('glass', 'lights', 'cable', 'decal', 'numbers', 'red', 'sky', 'sun', 'landscape', 'rocks', 'shape_u')


def dark_instance(mi):
    """Our darker instance of one of the pack's materials (made on first use), or None for the ones left alone
    (glass, lamps, signs)."""
    name = mi.get_name()
    if name in STEEL:
        return STEEL[name]
    if not name.startswith('MI_') or any(k in name for k in NO_TINT):
        return None
    mel = unreal.MaterialEditingLibrary
    ours = 'MI_GB_' + name[3:]
    path = KIT_MATS + '/' + ours
    if not eal.does_asset_exist(path):
        new = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ours, KIT_MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(new, mi)
    new = unreal.load_asset(path)
    base = mel.get_material_instance_vector_parameter_value(mi, 'BaseColor01')
    mel.set_material_instance_vector_parameter_value(new, 'BaseColor01', unreal.LinearColor(base.r * PANEL_TINT[0], base.g * PANEL_TINT[1], base.b * PANEL_TINT[2], 1.0))
    mel.update_material_instance(new)
    eal.save_asset(path, only_if_is_dirty=False)
    STEEL[name] = path
    return path


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
        elif it['kind'] == 'bp':
            cls = unreal.load_class(None, it['class'])
            if not cls:
                continue
            a = eas.spawn_actor_from_class(cls, unreal.Vector(*loc), r)
            a.set_actor_scale3d(unreal.Vector(*it['scale']))
            tag(a, stage, folder)
        else:
            d = dict(it)
            d['boost'] = light_boost
            light(stage, folder, it['class'], loc, r, d)
        n += 1
    return n


def door(stage, path, loc, yaw, slide, centre, distance=450.0, mats=None, flag=None):
    """A working door leaf (AGBDoor): slides by `slide` (its own axes) while the player is within `distance` of
    its doorway's middle (`centre`, its own axes)."""
    a = eas.spawn_actor_from_class(unreal.GBDoor, unreal.Vector(*loc), rot(yaw=yaw))
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    c.set_static_mesh(asset(path))
    for i, m in enumerate(mats or []):
        if m:
            c.set_material(i, asset(m))
    a.set_editor_property('slide_offset', unreal.Vector(*slide))
    a.set_editor_property('centre_offset', unreal.Vector(*centre))
    a.set_editor_property('open_distance', distance)
    if flag:
        a.set_editor_property('required_flag', unreal.Name(flag))
    tag(a, stage, 'Doors')
    return a


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
    ps.tags = list(ps.tags) + [unreal.Name('GBStartFuture')]      # UGBEraSubsystem: this map starts in the future
    ues.get_editor_world().get_world_settings().set_editor_property('kill_z', -2500.0)
    log.append('world: post process, player start')


async def stage_corridor():
    s = 'corridor'
    htb = kit('htb_demo')
    items = []
    for it in select(htb, x=(880, 2905)):
        name = it.get('mesh', '')
        x0, y0 = it.get('bmin', it['loc'])[0] - HTB_ORIGIN[0], it.get('bmin', it['loc'])[1] - HTB_ORIGIN[1]
        x1, y1 = it.get('bmax', it['loc'])[0] - HTB_ORIGIN[0], it.get('bmax', it['loc'])[1] - HTB_ORIGIN[1]
        if name.endswith('SM_Door_01'):
            continue                                   # the right alcove's door stands open: the air plant is behind it
        if 'SM_Grate_02' in name and 540 <= x0 <= 560 and y1 < 0:
            continue                                   # this wall panel of the first bay becomes the store room's door
        if it['kind'] == 'mesh' and 90 <= x0 and x1 <= 1110 and y1 <= -495 and y0 < -640:
            continue                                   # what the demo kept behind that wall; the store room stands there now
        items.append(it)
    n = place_section(s, 'Kit', items, HTB_ORIGIN)
    box(s, 'Collision', (-20, 2020), (-520, 520), (-60, -2))
    # The store room's door: the same frame the kit uses in the right alcove, in place of one wall panel.
    mesh(s, 'Doors', HTB + 'SM_Door_01_Frame', (550, -600, 0), rot(yaw=180),
         mats=['/Game/High_Tech_Base/Materials/MI_Tile_Machine_01', '/Game/High_Tech_Base/Materials/MI_Tile_Grate', '/Game/High_Tech_Base/Materials/MI_Rubber_02'])
    # The service pit under the alcove's bridge is 2.9 m deep. Crate steps on both sides of the bridge, under the
    # gaps the railing leaves at the corridor's edge: one crate, then two stacked, then the floor (0.8 m, one jump).
    for x in (1400, 1700):
        mesh(s, 'Steps', HTB + 'SM_Prop_02', (x, 670, -288), rot(yaw=5), collide=True)
        mesh(s, 'Steps', HTB + 'SM_Prop_02', (x, 560, -288), collide=True)
        mesh(s, 'Steps', HTB + 'SM_Prop_02', (x, 560, -183), rot(yaw=-6), collide=True)
    # Thresholds (owner: a hole between the corridor and the store room). The kit's corridor floor stops at its wall
    # line and each room's floor starts behind its own wall: a steel plate bridges every doorway.
    plate(s, 550, 750, -710, -495, 1.0)
    plate(s, 1450, 1650, 890, 1070, 1.0)
    log.append('corridor: %d pieces from the kit demo (two bays), door frame for the store room, pit steps, thresholds' % n)


async def stage_hall():
    s = 'hall'
    htb = kit('htb_demo')
    items = [it for it in select(htb, x=(2905, 99999)) if not it.get('mesh', '').endswith('.SM_Door_02')]
    n = place_section(s, 'Kit', items, HTB_ORIGIN)
    # The freight door at the end of the branch is a working door (owner): it lifts when the player comes near.
    door(s, HTB + 'SM_Door_02', (3805, 2450, 0), 180, slide=(0, 0, 430), centre=(425, 25, 150), distance=600,
         mats=['/Game/High_Tech_Base/Materials/MI_Door_03', '/Game/High_Tech_Base/Materials/MI_Tile_Machine_02'])
    log.append('hall: %d pieces from the kit demo (hangar shell), working freight door' % n)


async def stage_north():
    """Beyond the freight door the way goes on (owner): one more bay of the kit's corridor, to a second freight
    door that stays shut. That door is where the base continues in the next slice."""
    s = 'north'
    htb = kit('htb_demo')
    items = [it for it in select(htb, x=(880, 1900)) if 'SM_Prop_03' not in it.get('mesh', '') and not it.get('mesh', '').endswith('.SM_Door_01')]
    n = place_section(s, 'Kit', items, HTB_ORIGIN, to=(3380.0, 2460.0, 0.0), yaw=90.0)
    box(s, 'Collision', (2860, 3900), (2450, 3500), (-60, -2))
    hall = select(htb, x=(2905, 99999))
    for it in hall:
        if it.get('mesh', '').endswith('.SM_Door_02_Side') or it.get('mesh', '').endswith('.SM_Door_02'):
            loc = (it['loc'][0] - HTB_ORIGIN[0], it['loc'][1] - HTB_ORIGIN[1] + 1030.0, it['loc'][2] - HTB_ORIGIN[2])
            mesh(s, 'End', it['mesh'], loc, rot(*it['rot']), it['scale'], it['mats'])
    log.append('north: %d pieces (one corridor bay beyond the freight door), closed freight door at its end' % n)


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


def plate(s, x0, x1, y0, y1, z):
    """A solid steel threshold plate from (x0, y0) to (x1, y1), walking surface at z, cut to size from one 2 x 2 m tile."""
    a = mesh(s, 'Thresholds', MSS + 'Floor/SM_floor_200x200_01', (x0, y1, z - DECK), scale=((x1 - x0) / 200.0, (y1 - y0) / 200.0, 1.0), collide=True)
    return a


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
        p = (x + math.cos(rad) * 150 * k, y + math.sin(rad) * 150 * k, z - DECK + 100 * k)
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
    deck(s, 2800, 4800, 100, 300, CAT_Z)
    for x in (2800, 2990, 3800, 4200, 4750):
        post(s, x, 110, CAT_Z)
    rail(s, 2800, 100, 4200, 100, CAT_Z)                 # lane side, west part
    rail(s, 4350, 100, 4750, 100, CAT_Z)                 # lane side, east of the cross bridge
    rail(s, 3004, 300, 3804, 300, CAT_Z)                 # over the branch mouth, north side
    # West stair: straight, against the wall, the first thing seen on entering the bay.
    # It starts 2.5 m inside the bay (owner: the wing of the corridor's last frame stood in front of its first step).
    stairs(s, 2350, 200, 0, 0, 3, rails=(False, True))
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
    # A way out of the service trench (owner: a player who falls in early has no double jump): a crate as a step at
    # each end, where the railing leaves a gap.
    for x in (2750, 5250):
        mesh(s, 'Steps', crate, (x, -700, -98), collide=True)
    log.append('hall design: catwalk, two stairs, cross bridge and lookout, 10 crates, 2 machine blocks, trench steps')


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


# ---- the two side rooms: a module of the Modular SciFi Station demo, placed as its author built it ----
MOD_BOX = (-2160.0, -1040.0, -1760.0, -840.0)     # the module's walls in the demo map (x0, x1, y0, y1)
MOD_DOOR = (-1050.0, -1300.0, 170.0)              # outer threshold of its lower door, at floor-surface height
LAMP_OF = {'BP_Wall_Lamp_120_C': ('SM_lamp_120_01', (186, 228, 255), 0.7, 550.0), 'BP_Door_Lamp_01_C': ('SM_lamp_120_01', (186, 228, 255), 0.7, 550.0),
           'BP_Tunnel_Lamp_120_C': ('SM_lamp_120_01', (186, 228, 255), 0.7, 550.0), 'BP_Wall_Lamp_80_red_C': ('SM_lamp_80_01', (255, 85, 64), 0.9, 450.0),
           'BP_Ceiling_Lamp_200_C': ('SM_lamp_200_02', (186, 228, 255), 1.4, 800.0)}


def module_room(stage, to, yaw, two_story):
    """Places the demo's stair module with its lower door's threshold on our point `to`, turned `yaw`.
    two_story: the whole module (lower floor, the stair, the gallery 4 m up, its roof).
    Otherwise only its lower story, closed by the module's own roof assembly brought down 4 m."""
    rad = math.radians(yaw)
    cs, sn = math.cos(rad), math.sin(rad)
    turn = rot(yaw=yaw)
    n = 0
    for it in kit('mss_demo'):
        if it['kind'] == 'mesh':
            cx, cy = (it['bmin'][0] + it['bmax'][0]) / 2.0, (it['bmin'][1] + it['bmax'][1]) / 2.0
        else:
            cx, cy = it['loc'][0], it['loc'][1]
        if not (MOD_BOX[0] <= cx <= MOD_BOX[1] and MOD_BOX[2] <= cy <= MOD_BOX[3]):
            continue
        name = it.get('mesh', it.get('class', '')).split('.')[-1]
        if any(k in name for k in ('hydraulic', 'antenna', 'radar', 'wall_roof', 'tunnel_connection', 'rock')):
            continue                                   # stilts, roof gear and the outer skin's trim: nothing of the inside
        z = it['loc'][2]
        dz = 0.0
        if z < 150 or z >= 1000:
            continue
        if not two_story:
            if z >= 940:
                dz = -400.0                            # the roof slab with its pipes and trusses becomes this room's ceiling
            elif z >= 550 or 'stairs' in name or name.startswith('SM_railing'):
                continue
        dx, dy = it['loc'][0] - MOD_DOOR[0], it['loc'][1] - MOD_DOOR[1]
        loc = (to[0] + dx * cs - dy * sn, to[1] + dx * sn + dy * cs, to[2] + z + dz - MOD_DOOR[2])
        r = unreal.MathLibrary.compose_rotators(rot(*it['rot']), turn)
        if it['kind'] == 'mesh':
            path = it['mesh']
            if name == 'SM_wall_door_400_a_01' and z >= 550 and not two_story:
                path = MSS + 'Walls/A/SM_wall_400_a_02'
            mesh(stage, 'Module', path, loc, r, it['scale'], it['mats'])
            n += 1
        elif name in LAMP_OF:
            lamp, color, cd, radius = LAMP_OF[name]
            folder = 'Lamps/Meshes/' if True else ''
            mesh(stage, 'Lamps', MSS + folder + lamp, loc, r)
            light(stage, 'Lamps', 'PointLight', (loc[0], loc[1], loc[2] - 25), rot(), {'intensity': cd, 'color': color, 'attenuation_radius': radius, 'shadow': False})
            n += 1
    return n


async def stage_rooms():
    """Plan 3.5 and its second room. Both are the same kind of module, as rooms of one base would be; one keeps
    its two floors, the other is a single low store."""
    s = 'rooms'
    # Air plant, behind the right alcove's door (bridge over the service pit, then the door at Y 900..950).
    # Two floors joined by a stair: vent drums and cable trays below, the control gallery 4 m up.
    n1 = module_room(s, (1550.0, 950.0, 0.0), -90.0, True)
    # Store room, behind the new door of the first bay (wall at Y -600). One story.
    n2 = module_room(s, (650.0, -650.0, 0.0), 90.0, False)
    # What a store holds, where a storeman would put it: stacked against the back wall, a clear lane from the door.
    crate = HTB + 'SM_Prop_02'
    for x, y, yaw, z in ((480, -1640, 0, 0), (590, -1645, 6, 0), (535, -1642, -8, 105), (700, -1640, 0, 0),
                         (320, -1200, 90, 0), (320, -1090, 84, 0), (322, -1146, 95, 105)):
        mesh(s, 'Store', crate, (x, y, z + 2), rot(yaw=yaw), collide=True)
    for i, (x, y) in enumerate(((985, -820), (990, -880), (940, -850))):
        mesh(s, 'Store', '/Game/SciFiWorld/Meshes/SM_LabBarrels01_%d' % (i + 1), (x, y, 0), rot(yaw=40 * i), collide=True)
    # Cabinets along the east wall (glass-door units from the lab kit, dark skin) and the storeman's desk by the door.
    lab = '/Game/SciFiWorld/Meshes/'
    dark = ['/Game/SciFiWorld/Materials/MI_LabFridge01B']
    for y in (-1080, -1260, -1440):
        mesh(s, 'Store', lab + 'SM_LabFridge01_2_Base', (1040, y, 0), rot(yaw=180), mats=dark, collide=True)
        mesh(s, 'Store', lab + 'SM_LabFridge01_2_Shelves', (1040, y, 0), rot(yaw=180), mats=dark)
    mesh(s, 'Store', lab + 'SM_LabDesk03_2', (300, -760, 0), rot(yaw=0), mats=['/Game/SciFiWorld/Materials/MI_LabDesk03B'], collide=True)
    mesh(s, 'Store', HTB + 'SM_Prop_04', (400, -1600, 0), collide=True)
    mesh(s, 'Store', HTB + 'SM_Prop_04', (860, -1610, 0), rot(yaw=30), collide=True)
    log.append('rooms: air plant (%d pieces, two floors) and store room (%d pieces, one story), 7 crates, 3 barrels' % (n1, n2))


LAB_GATE = (1454.0, 400.0, 0.0)     # outer face of the lab room's gate, at floor height, in the lab kit's demo map
LAB_Z = 410.0                       # the air plant's gallery


async def stage_lab():
    """The specimen lab (owner: a door from the gallery to a medium room with some fighting). It is the lab kit's
    own room (dark skin), placed whole as its author built it, 4 m above the corridor, with its gate against the
    gallery's far door. What came through the anomaly is kept here; the first fight of the future happens here."""
    s = 'lab'
    items = [it for it in kit('lab_roomB') if 'MGate' not in it.get('class', '')]
    n = place_section(s, 'Lab', items, LAB_GATE, to=(1550.0, 2075.0, LAB_Z), yaw=-90.0)
    plate(s, 1450, 1650, 1940, 2140, LAB_Z + 1.0)      # from the gallery through the door wall to the lab's gate
    enc = eas.spawn_actor_from_class(unreal.GBEncounter, unreal.Vector(1500, 3800, LAB_Z + 150), rot())
    enc.set_editor_property('encounter_id', unreal.Name('Opening_Future'))
    enc.set_editor_property('presence', unreal.GBEraPresence.FUTURE_ONLY)
    enc.set_editor_property('trigger_extent', unreal.Vector(900, 1000, 300))
    tag(enc, s, 'Gameplay')
    log.append('lab: %d pieces and lights from the lab kit room, encounter Opening_Future' % n)


# Review spots: (name, x, y, floor z, yaw). The game's smoke block "tour" puts the player on each one and takes a
# screenshot through the real game camera (gb.Smoke tour).
TOUR = [('01_arrival', -500, 60, 0, 0), ('02_arrival_back', -150, 0, 0, 180), ('03_corridor', 300, -50, 0, 0),
        ('04_corridor_alcove', 1500, 0, 0, 90), ('05_enter_bay', 2050, -200, 0, 12), ('06_west_stair', 2000, -100, 0, 35),
        ('07_catwalk', 2700, 200, 300, -5), ('08_bridge', 4250, -100, 300, -90), ('09_lookout', 4250, -600, 300, 150),
        ('10_bay_floor_east', 4600, -150, 0, 180), ('11_branch', 3380, 2200, 0, -90), ('12_under_catwalk', 3380, 500, 0, -90),
        ('13_alcove_bridge', 1550, 620, 0, 90), ('14_air_plant', 1350, 1400, 0, 20), ('15_air_plant_gallery', 1550, 1150, 410, 90),
        ('16_store_door', 650, -350, 0, -90), ('17_store', 650, -1000, 0, -90),
        ('18_lab_gate', 1550, 2350, 410, 90), ('19_lab', 1500, 3000, 410, 90), ('20_north_corridor', 3380, 2750, 0, 90),
        ('21_alcove_pit', 1250, 700, -290, 20)]


async def stage_tour():
    s = 'tour'
    for name, x, y, z, yaw in TOUR:
        a = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z), rot(yaw=yaw))
        a.tags = [unreal.Name(TAG), unreal.Name('stage_' + s), unreal.Name('GBTour'), unreal.Name('tour_' + name)]
        a.set_folder_path(unreal.Name('Base/tour'))
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(1900, 1650, 300), rot())
    nav.set_actor_scale3d(unreal.Vector(44.0, 40.0, 8.0))    # the volume's brush is a 2 m cube
    tag(nav, s, 'Gameplay')
    log.append('tour: %d review spots, nav bounds' % len(TOUR))


# Walks checked for holes: (name, [(x, y, z), ...]). A point every 20 cm must have floor within 45 cm of the line.
WALKS = [('arrival to corridor', [(-500, 60, 0), (300, 0, 0)]), ('corridor to store room', [(650, -300, 0), (650, -950, 0)]),
         ('corridor over the bridge into the air plant', [(1550, 300, 0), (1550, 1180, 0)]),
         ('corridor to bay', [(1800, 0, 0), (2500, -150, 0)]), ('bay to branch to freight door and on', [(3380, 100, 0), (3380, 3350, 0)]),
         ('gallery to lab', [(1550, 1960, 410), (1550, 2700, 410)]), ('bay floor to west stair', [(2250, 200, 0), (2360, 200, 0)])]


def check_walks():
    world = ues.get_editor_world()
    bad = []
    for name, pts in WALKS:
        for (x0, y0, z0), (x1, y1, z1) in zip(pts[:-1], pts[1:]):
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 20.0))
            run = []
            for i in range(n + 1):
                t = i / float(n)
                x, y, z = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t
                hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 60), unreal.Vector(x, y, z - 45),
                                                             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
                if not hit:
                    run.append((int(x), int(y)))
            if run:
                bad.append('%s: no floor from %s to %s (%d points)' % (name, run[0], run[-1], len(run)))
    log.append('walks: ' + ('no holes on %d walks' % len(WALKS) if not bad else 'HOLES -> ' + ' | '.join(bad)))


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
       ('north', stage_north), ('hall_design', stage_hall_design), ('arrival', stage_arrival), ('rooms', stage_rooms),
       ('lab', stage_lab), ('tour', stage_tour)]

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
await asyncio.sleep(0.5)
check_walks()
les.save_current_level()
'\n'.join(log)
