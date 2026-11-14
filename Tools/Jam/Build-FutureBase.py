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
    if d.get('flicker'):
        c.set_editor_property('light_function_material', asset(FLICKER))     # a failing lamp
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


def ramp(stage, x0, x1, y_low, y_high, z_low, z_high):
    """An unseen walking slope across x0..x1 that rises along Y from (y_low, z_low) to (y_high, z_high). Steps whose
    risers stop the player's capsule are walked on this instead (their own meshes then do not collide)."""
    run, rise = y_high - y_low, z_high - z_low
    length = math.hypot(run, rise)
    angle = math.degrees(math.atan2(rise, abs(run)))
    # A positive roll lowers the +Y side, so a slope that rises towards +Y has a negative one.
    r = rot(roll=-angle if run > 0 else angle)
    drop = 4.0 / math.cos(math.radians(angle))               # the box is 8 cm thick: its top face is the slope
    a = eas.spawn_actor_from_object(asset('/Engine/BasicShapes/Cube'), unreal.Vector((x0 + x1) / 2.0, (y_low + y_high) / 2.0, (z_low + z_high) / 2.0 - drop), r)
    a.set_actor_scale3d(unreal.Vector(abs(x1 - x0) / 100.0, length / 100.0, 0.08))
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.STATIC)
    c.set_collision_profile_name('BlockAll')
    a.set_actor_hidden_in_game(True)
    tag(a, stage, 'Collision')
    return a


def decal(stage, material, loc, size, yaw=0.0, wall_yaw=None):
    """A decal on the floor (projected straight down), or on a wall when wall_yaw is given (the way the wall faces)."""
    r = rot(pitch=-90.0, yaw=yaw) if wall_yaw is None else rot(yaw=wall_yaw + 180.0, roll=yaw)
    a = eas.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(*loc), r)
    c = a.get_component_by_class(unreal.DecalComponent)
    c.set_decal_material(asset(material))
    c.set_editor_property('decal_size', unreal.Vector(60.0, size, size))
    tag(a, stage, 'Decals')
    return a


CRACKS = '/Game/Glyphbound/Materials/M_GB_Cracks'
FLICKER = '/Game/Glyphbound/Materials/M_GB_LightFlicker'


def breakable(stage, path, loc, r, scale, mats, hits, flag):
    """A piece that breaks after `hits` hits (AGBBreakable) and stays broken (progress flag)."""
    a = eas.spawn_actor_from_class(unreal.GBBreakable, unreal.Vector(*loc), r)
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    c.set_static_mesh(asset(path))
    for i, m in enumerate(mats or []):
        if m:
            c.set_material(i, asset(m))
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_editor_property('hits_to_break', hits)
    a.set_editor_property('broken_flag', unreal.Name(flag))
    a.set_editor_property('crack_material', asset(CRACKS))
    tag(a, stage, 'Breakable')
    return a


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


CRATE = '/Game/High_Tech_Base/Models/SM_Prop_02'
# The kit's cargo crate, measured from its mesh: 102 cm wide and exactly 100 cm from its flat underside to its rim.
# Crates that touch stand one width apart and a crate on another stands one height up: any more and light shows
# between them (owner, 2026-11-14: "there are gaps between the crates, all of them have that problem").
CRATE_W, CRATE_H = 102.0, 100.0


def crate(stage, folder, x, y, z, yaw=0.0, level=0, scale=1.0):
    mesh(stage, folder, CRATE, (x, y, z + CRATE_H * scale * level), rot(yaw=yaw), scale=(scale, scale, scale), collide=True)


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
        crate(s, 'Steps', x, 560 + CRATE_W, -290)
        crate(s, 'Steps', x, 560, -290)
        crate(s, 'Steps', x, 560, -290, yaw=-6, level=1)
    # Thresholds (owner: a hole between the corridor and the store room). The kit's corridor floor stops at its wall
    # line and each room's floor starts behind its own wall: a steel plate bridges every doorway.
    plate(s, 550, 750, -710, -495, 1.0)
    plate(s, 1450, 1650, 890, 1070, 1.0)
    log.append('corridor: %d pieces from the kit demo (two bays), door frame for the store room, pit steps, thresholds' % n)


async def stage_hall():
    s = 'hall'
    htb = kit('htb_demo')
    items = [it for it in select(htb, x=(2905, 99999)) if not it.get('mesh', '').endswith(('.SM_Door_02', '.SM_Hangar_Door_01'))]
    n = place_section(s, 'Kit', items, HTB_ORIGIN)
    # The grated shutter at the bay's east end, past the east stair, is a weak wall (owner, 2026-11-14): it breaks
    # after four hits, cracks spreading over it with each one, and the way down to the deep wing is behind it.
    breakable(s, HTB + 'SM_Hangar_Door_01', (5300, -500, 0), rot(yaw=180), (1, 1, 1.13),
              ['/Game/High_Tech_Base/Materials/MI_Tile_Grate_Ceiling_06'], 4, 'FutureBayShutterBroken')
    # The freight door at the end of the branch is a working door (owner): it lifts when the player comes near.
    door(s, HTB + 'SM_Door_02', (3805, 2450, 0), 180, slide=(0, 0, 430), centre=(425, 25, 150), distance=600,
         mats=['/Game/High_Tech_Base/Materials/MI_Door_03', '/Game/High_Tech_Base/Materials/MI_Tile_Machine_02'])
    log.append('hall: %d pieces from the kit demo (hangar shell), working freight door' % n)


async def stage_north():
    """Beyond the freight door the way goes on (owner): one more bay of the kit's corridor, to a second freight
    door that stays shut. That door is where the base continues in the next slice."""
    s = 'north'
    htb = kit('htb_demo')
    # Only the bay itself. The demo's control room (its wall mesh, floor, roof, decals) has its pivots just inside this
    # range of X; in the first version they came along, cut across the branch and walled the player in (owner).
    items = [it for it in select(htb, x=(880, 1900))
             if not it.get('mesh', '').endswith('.SM_Door_01') and not (it['kind'] == 'mesh' and it['bmin'][0] < 885)]
    n = place_section(s, 'Kit', items, HTB_ORIGIN, to=(3380.0, 2460.0, 0.0), yaw=90.0)
    box(s, 'Collision', (2860, 3900), (2450, 3500), (-60, -2))
    # The door line itself has no floor piece in the kit (the builder's walk check found the gap once the control
    # room's floor no longer covered it): a threshold strip under each freight door.
    for yy in (2396, 3426):
        for xx in range(2955, 3805, 170):
            plate(s, xx, xx + 170, yy, yy + 74, 0.5)
    hall = select(htb, x=(2905, 99999))
    for it in hall:
        if it.get('mesh', '').endswith('.SM_Door_02_Side'):
            loc = (it['loc'][0] - HTB_ORIGIN[0], it['loc'][1] - HTB_ORIGIN[1] + 1030.0, it['loc'][2] - HTB_ORIGIN[2])
            mesh(s, 'End', it['mesh'], loc, rot(*it['rot']), it['scale'], it['mats'])
    # The second freight door works too: the long service corridor starts behind it.
    door(s, HTB + 'SM_Door_02', (3805, 3480, 0), 180, slide=(0, 0, 430), centre=(425, 25, 150), distance=600,
         mats=['/Game/High_Tech_Base/Materials/MI_Door_03', '/Game/High_Tech_Base/Materials/MI_Tile_Machine_02'])
    log.append('north: %d pieces (one corridor bay beyond the freight door), second working freight door' % n)


SHIP = '/Game/Glyphbound/Environment/ShipCorridor/'


async def stage_long():
    """The long corridor behind the second freight door (owner: "a long corridor, with cables on the floors and pipes
    on the ceiling; there is already an asset like that"). It is that asset: the Spaceship Corridor model, exported
    from Blender (Tools/Jam/corridor_blender_export.py) and imported with the plain materials the owner approved
    (Tools/Jam/import_corridor.py). 5.3 m wide, 3.3 m high, 40.5 m long, from Y 3500 to Y 7546; its near end is open
    and its far end has the model's own double door. Its two leaves are meshes of their own and slide apart when the
    player comes near: the sub-boss hall is behind them (stage boss).
    The model's Y axis is mirrored by the import, so it is turned half a circle to put its open end at the start.
    Its walking grid is 14 cm above its own base, so the model sits 13 cm low and the grid meets the base's floor."""
    s = 'long'
    y0 = 3500.0
    origin = (3380.0, y0 + 2256.0, -13.0)
    mesh(s, 'Ship', SHIP + 'SM_ShipCorridor_Body', origin, rot(yaw=180), collide=True)
    mesh(s, 'Ship', SHIP + 'SM_ShipCorridor_Details', origin, rot(yaw=180))
    for leaf in ('SM_ShipCorridor_DoorA', 'SM_ShipCorridor_DoorB'):
        b = asset(SHIP + leaf).get_bounding_box()
        side = 1.0 if (b.min.x + b.max.x) > 0 else -1.0             # each leaf slides away from the middle, into the wall
        door(s, SHIP + leaf, origin, 180, slide=(side * 238.0, 0.0, 0.0), centre=(0.0, (b.min.y + b.max.y) / 2.0, 150.0), distance=450)
    # The floor is a real grid with holes and loose cables lie on it: an unseen plate just above it keeps walking
    # smooth. The crate (in the details mesh, which does not collide) gets a box.
    box(s, 'Collision', (3380 - 160, 3380 + 160), (y0, y0 + 4046), (-10, 2), visible=False)
    box(s, 'Collision', (3213, 3317), (6015, 6119), (0, 66), visible=False)       # the one crate left (the upper one is out of the export)
    # The model is a shell seen from inside: whoever gets inside a side bench falls out through its underside (a review
    # spot placed there did). A floor under the whole width catches that.
    box(s, 'Collision', (3380 - 270, 3380 + 270), (y0, y0 + 4046), (-45, -16), visible=False)
    # Bulkhead: the 8.5 m freight door narrows to the corridor's 5.3 m.
    box(s, 'Backing', (2940, 3118), (3482, 3504), (0, 620))
    box(s, 'Backing', (3642, 3820), (3482, 3504), (0, 620))
    box(s, 'Backing', (3118, 3642), (3482, 3504), (314, 620))
    # Light: the model's own strip and screens glow; these lamps sit on that strip, and the red ones behind the roof
    # vents are where the model has its red lights.
    for i in range(6):
        y = y0 + 350 + i * 680
        light(s, 'Lamps', 'RectLight', (3380, y, 288), rot(pitch=-90), {'intensity': 0.5, 'color': (255, 240, 220), 'attenuation_radius': 550.0,
                                                                    'source_width': 20.0, 'source_height': 400.0, 'shadow': False})
        for side in (-115, 115) if i < 5 else ():      # the sixth pair would land behind the far door
            light(s, 'Lamps', 'PointLight', (3380 + side, y + 340, 305), rot(), {'intensity': 0.7, 'color': (255, 60, 40), 'attenuation_radius': 300.0, 'shadow': False})
    log.append('long: the Spaceship Corridor model (body and details), 40.5 m, bulkhead, floor plate, 6 lamps')


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


def post(s, x, y, z_top, z0=0.0):
    """A truss post from the floor (z0) up to the underside of a deck at z_top (2 m and 1 m pieces stacked)."""
    z = z0
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
    deck(s, 2800, 4600, 100, 300, CAT_Z)
    for x in (2800, 2990, 3800, 4200, 4590):
        post(s, x, 110, CAT_Z)
    rail(s, 2800, 100, 4200, 100, CAT_Z)                 # lane side, west part
    rail(s, 4350, 100, 4550, 100, CAT_Z)                 # lane side, east of the cross bridge
    rail(s, 3004, 300, 3804, 300, CAT_Z)                 # over the branch mouth, north side
    # West stair: straight, against the wall, the first thing seen on entering the bay.
    # It starts 2.5 m inside the bay (owner: the wing of the corridor's last frame stood in front of its first step).
    stairs(s, 2350, 200, 0, 0, 3, rails=(False, True))
    # East stair: straight down towards the freight end, so the catwalk is a flanking route, not a dead end.
    # Its foot is 2.5 m short of the end wall and nothing stands in front of it (owner: "the same as the other").
    stairs(s, 5050, 200, 0, 180, 3, rails=(True, False))
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
    for x, y, yaw, level in ((2900, -385, 0, 0), (2900 + CRATE_W, -385, 0, 0), (2900 + CRATE_W / 2, -385, 12, 1),   # by the railing, one stacked
                             (3550, -120, 0, 0), (3550, -120 - CRATE_W, 0, 0),                                       # lane centre, before the bridge
                             (4700, -325, 0, 0), (4700 + CRATE_W, -325, 0, 0),                                       # past the lookout
                             (3200, 1500, 0, 0), (3200 + CRATE_W, 1500, 0, 0), (3600, 1900, 15, 0)):                 # in the branch, towards the freight door
        crate(s, 'Cover', x, y, 0, yaw, level)
    # Two tall machine blocks break the 32 m sight line of the lane (the same unit that stands in the trench).
    for x, y in ((3350, 180), (5150, -330)):            # the second one stood in front of the east stair's foot
        mesh(s, 'Cover', HTB + 'SM_Prop_HS', (x, y, 124), collide=True)
    # Work lights on the wall above the catwalk and under the lookout.
    for x in (2750, 3404, 4050, 4650, 5120):          # the last one lights the east stair's foot, which was in the dark
        work_light(s, x, 290, 520, -90)
    # A way out of the service trench (owner: a player who falls in early has no double jump): a crate as a step at
    # each end, where the railing leaves a gap.
    for x in (2750, 5250):
        crate(s, 'Steps', x, -700, -100)
    log.append('hall design: catwalk, two stairs, cross bridge and lookout, 10 crates, 2 machine blocks, trench steps')


async def stage_arrival():
    """Where the player arrives (plan 3.1), inside the kit's control room: the seal on a low round plinth in the
    middle of the watch consoles, with four emitters around it and their cable trays running to it."""
    s = 'arrival'
    cx, cy = -760.0, 60.0
    mesh(s, 'Seal', MSS + 'Scaffold/SM_hydraulic_pillar_400_01', (cx, cy, 0), scale=(1, 1, 0.15), collide=True)       # plinth, 30 cm
    for yaw in (0, 90, 180, 270):
        mesh(s, 'Seal', MSS + 'Floor/SM_floor_round_02', (cx, cy, 30 - DECK + 1), rot(yaw=yaw))                     # grating disc
    # The seal is held inside a geodesic cage (the station kit's sphere) with a cold light at its heart.
    mesh(s, 'Seal', MSS + 'Props/Radar/SM_radar_sphere_01', (cx, cy, 30), scale=(0.75, 0.75, 0.75), collide=True)
    light(s, 'Seal', 'PointLight', (cx, cy, 330), rot(), {'intensity': 9.0, 'color': (120, 215, 255), 'attenuation_radius': 900.0, 'shadow': False})
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
    for x, y, yaw, level in ((480, -1640, 0, 0), (480 + CRATE_W, -1640, 0, 0), (480 + CRATE_W / 2, -1640, -8, 1), (480 + 2 * CRATE_W, -1640, 0, 0),
                             (320, -1200, 90, 0), (320, -1200 + CRATE_W, 90, 0), (320, -1200 + CRATE_W / 2, 95, 1)):
        crate(s, 'Store', x, y, 0, yaw, level)
    for x, y in ((985, -820), (975, -900)):
        mesh(s, 'Store', HTB + 'SM_Prop_04', (x, y, 0), collide=True)
    # A second stack along the east wall and the storeman's console by the door (no lab kit here: the owner keeps
    # that kit for the Valkyrie's fight).
    for x, y, yaw, level in ((990, -1150, 90, 0), (990, -1150 - CRATE_W, 90, 0), (990, -1150 - 2 * CRATE_W, 90, 0), (990, -1150 - CRATE_W / 2, 88, 1)):
        crate(s, 'Store', x, y, 0, yaw, level)
    mesh(s, 'Store', MSS + 'Props/WallAttachments/SM_control_panel_01', (256, -800, 150), rot(yaw=0))
    mesh(s, 'Store', HTB + 'SM_Prop_04', (400, -1600, 0), collide=True)
    mesh(s, 'Store', HTB + 'SM_Prop_04', (860, -1610, 0), rot(yaw=30), collide=True)
    log.append('rooms: air plant (%d pieces, two floors) and store room (%d pieces, one story), 7 crates, 3 barrels' % (n1, n2))


LAB_GATE = (1454.0, 400.0, 0.0)     # outer face of the lab room's gate, at floor height, in the lab kit's demo map
LAB_Z = 410.0                       # the air plant's gallery
LAB_DROP = ('CreatureAlien', 'LabHoloProjector', 'LabCapsule')   # the figures, their holograms and their tubes


def lab_copy(items, name, loc, turn=0.0, scale=None):
    """One more of a piece the room already has (same materials and scale), at another place of the room, in the lab
    kit's demo coordinates, turned `turn` degrees from the way its author placed it."""
    src = next(it for it in items if it.get('mesh', '').endswith('.' + name))
    it = dict(src)
    it['loc'] = [loc[0], loc[1], loc[2]]
    it['rot'] = [src['rot'][0], src['rot'][1] + turn, src['rot'][2]]
    if scale:
        it['scale'] = list(scale)
    return it


def lab_world(x, y, z=0.0, to=None):
    """A point of the lab kit's demo room in our world (the room is placed turned -90 degrees)."""
    to = to or (1550.0, 2075.0, LAB_Z)
    return (to[0] + (y - LAB_GATE[1]), to[1] - (x - LAB_GATE[0]), to[2] + z)


LAB_FLAGS = ('LabSocketA', 'LabSocketB')     # set by the two sockets on the dais; the energy gate needs both
HOLO = '/Game/Glyphbound/Materials/M_GB_EnergyGate'     # ours: the hologram pack's own materials hide what is behind them


async def stage_lab():
    """The medium room behind the gallery's far door (owner, 2026-11-14: "keep the room, but take out the figures and
    the holograms and put in assets that leave it a generic room"). It is the lab kit's own room, placed as its author
    built it, 4 m above the corridor, without the alien figures, their holograms and their tubes: those are the look
    of the Valkyrie's fight. In their place, more of what the room already has: cabinets, barrels, a work desk with
    its screens along the long wall, a second control panel on the dais. No fight here for now (owner)."""
    s = 'lab'
    room = [it for it in kit('lab_roomB') if 'MGate' not in it.get('class', '')]
    items = [it for it in room if not any(k in it.get('mesh', '') for k in LAB_DROP)]
    # The dais's end wall is one 7 m panel. It makes way for a doorway (below).
    items = [it for it in items if not (it.get('mesh', '').endswith('.SM_MWall03-700x350-1') and abs(it['loc'][0] + 1425) < 5 and abs(it['loc'][1] - 400) < 5)]
    new = []
    # The long wall had five bays, each with a tube and a hologram, between low parapets. Now: storage and a desk.
    new.append(lab_copy(room, 'SM_LabFridge01_2_Base', (-1212, 1365, 0), -90))
    for name, x, y in (('SM_LabBarrels01_3', -905, 1335), ('SM_LabBarrels01_1', -845, 1300), ('SM_LabBarrels01_5', -770, 1340), ('SM_LabBarrels01_2', -935, 1265)):
        src = next(it for it in room if it.get('mesh', '').endswith('.' + name))
        new.append(lab_copy(room, name, (x, y, src['loc'][2])))
    new.append(lab_copy(room, 'SM_LabFridge01_1_Base', (-475, 1365, 0), -90))
    new.append(lab_copy(room, 'SM_LabDesk03_2', (-50, 1440, 0), -90))                # its user sits on the room's side
    new.append(lab_copy(room, 'SM_LabDisplay01_2', (-49, 1409, 90), -90))
    new.append(lab_copy(room, 'SM_LabDisplay01_3', (-50, 1364, 90), -90))
    new.append(lab_copy(room, 'SM_Chair08', (-47, 1248, 1), -90))
    new.append(lab_copy(room, 'SM_LabFridge01_2_Base', (425, 1365, 0), -90))
    for name, x, y in (('SM_LabBarrels01_4', 215, 1335), ('SM_LabBarrels01_2', 640, 1320)):
        src = next(it for it in room if it.get('mesh', '').endswith('.' + name))
        new.append(lab_copy(room, name, (x, y, src['loc'][2])))
    # The dais had a hologram of a figure and a tube against the end wall. Now: a second control panel, and in the
    # end wall the kit's own gate frame, with a narrow wall panel each side of it (owner, 2026-11-14: "take that
    # asset off the wall and put a door that fits that wall and that room").
    new.append(lab_copy(room, 'SM_ControlPanel01', (-900, 225, 50)))
    new.append(lab_copy(room, 'SM_MGate01-500x350-1_GateFrame', (-1425, 400, 0)))
    for y in (150, 750):
        new.append(lab_copy(room, 'SM_MWall03-50x350-1', (-1425, y, 0), turn=180, scale=(1, 2, 1)))
    # The tubes lit that wall. Five soft lamps in their place.
    for x in (-1212, -840, -475, -50, 425):
        new.append({'kind': 'light', 'class': 'PointLight', 'loc': [x, 1230, 300], 'rot': [0, 0, 0], 'scale': [1, 1, 1],
                    'intensity': 2.0, 'color': [190, 228, 255], 'attenuation_radius': 650.0, 'shadow': False})
    n = place_section(s, 'Lab', items + new, LAB_GATE, to=(1550.0, 2075.0, LAB_Z), yaw=-90.0)
    plate(s, 1450, 1650, 1940, 2140, LAB_Z + 1.0)      # from the gallery through the door wall to the lab's gate
    # The three steps up to the dais stopped the player like a low wall (owner): their risers are one 50 cm face.
    # An unseen slope lies over them and the step meshes themselves no longer collide (enable_collision).
    lo, hi = lab_world(-266, 50), lab_world(-378, 750, 50)
    ramp(s, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2])
    # The doorway in the end wall is shut by a sheet of energy (owner: "an energy, holographic door": it will go out
    # when the two things the enemies drop are set in the two devices with the glowing white ball). What is behind
    # stays in sight through it: the Valkyrie's room (stage valk).
    gx, gy, gz = lab_world(-1425, 400, 50 + 150)
    gate = eas.spawn_actor_from_class(unreal.GBEnergyGate, unreal.Vector(gx, gy, gz), rot(yaw=-90))
    gate.set_actor_scale3d(unreal.Vector(0.06, 4.4, 3.0))
    gate.get_component_by_class(unreal.StaticMeshComponent).set_material(0, asset(HOLO))
    gate.set_editor_property('required_flags', [unreal.Name(f) for f in LAB_FLAGS])
    tag(gate, s, 'Gate')
    for flag, y in zip(LAB_FLAGS, (575, 225)):          # the two control panels on the dais are the devices
        sx, sy, sz = lab_world(-900, y, 50 + 100)
        sock = eas.spawn_actor_from_class(unreal.GBKeySocket, unreal.Vector(sx, sy, sz), rot())
        sock.set_editor_property('item_id', unreal.Name('PowerCell'))
        sock.set_editor_property('flag', unreal.Name(flag))
        tag(sock, s, 'Gate')
    log.append('lab: %d pieces and lights (the lab kit room without %d figures, holograms and tubes; %d generic pieces in their place), ramp over the dais steps, energy gate with 2 sockets'
               % (n, len(room) - len(items), len(new)))


VALK_TO = (1550.0, 4320.0, LAB_Z + 50.0)     # the second lab room: its inner doorway (demo x 700) lands on Y 5074, 1.2 m behind the gate
BATTLE = {}


def battle_instance(path):
    """A copy of one of the lab kit's materials with its glow turned down to a fifth: the Valkyrie's room after a
    fight has most of its light panels dead. Made on first use; materials without a glow are left as they are."""
    if not path:
        return path
    if path in BATTLE:
        return BATTLE[path]
    mel = unreal.MaterialEditingLibrary
    mi = unreal.load_asset(path.split('.')[0])
    result = path
    if isinstance(mi, unreal.MaterialInstanceConstant) and 'EmissionPower' in [str(n) for n in mel.get_scalar_parameter_names(mi)]:
        ours = 'MI_GB_Battle_' + mi.get_name()[3:]
        dst = KIT_MATS + '/' + ours
        if not eal.does_asset_exist(dst):
            new = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ours, KIT_MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mel.set_material_instance_parent(new, mi)
        new = unreal.load_asset(dst)
        mel.set_material_instance_scalar_parameter_value(new, 'EmissionPower', mel.get_material_instance_scalar_parameter_value(mi, 'EmissionPower') * 0.2)
        mel.update_material_instance(new)
        eal.save_asset(dst, only_if_is_dirty=False)
        result = dst
    BATTLE[path] = result
    return result


SCORCH = '/Game/SciFi_Impacts/Decals/MI_ScorchDecal_%d'
DIRT = '/Game/SciFi_Impacts/Alleyway/Decals/MI_DirtDecal_0%d'


async def stage_valk():
    """The Valkyrie's room, behind the energy gate (owner, 2026-11-14: "further on, where you do use the room with
    all the lab assets, with its materials somewhat corroded or destroyed, more as if there had been a battle than as
    if it were abandoned; the Valkyrie will be there and she has to be seen even though the player has not opened
    the door, to wake the player's curiosity at the start").
    It is the lab kit's room once more, this time whole: the figures, their holograms and their tubes. It starts at
    the kit's inner doorway (its entry hall is left out) 1.2 m behind the gate, on the same axis, so the big tube
    against its far wall is straight ahead of anyone who looks through the gate: that tube is the Valkyrie's.
    After a battle: four fifths of the room's own glow is gone, scorch marks and grime on floor and walls, chairs
    and barrels knocked over, red emergency lamps, and a cold light on her tube. The Valkyrie herself is not in the
    map yet (she has no model); the alien that floated in that tube is taken out to leave it for her."""
    s = 'valk'
    room = [it for it in kit('lab_roomB') if 'MGate' not in it.get('class', '')]
    items = []
    n_fallen = 0
    for i, src in enumerate(room):
        cx = (src['bmin'][0] + src['bmax'][0]) / 2.0 if src.get('bmin') else src['loc'][0]
        name = src.get('mesh', src.get('class', ''))
        if cx > 705 or 'CreatureAlien06-Pose2' in name:
            continue                                   # the entry hall, and the alien in the Valkyrie's tube
        it = dict(src)
        if it['kind'] == 'mesh':
            it['mats'] = [battle_instance(m) for m in it.get('mats', [])]
            # Knocked over: every other chair and every third barrel, each its own way (the index decides, so a rebuild gives the same room).
            if ('Chair' in name and i % 2 == 0) or ('Barrels' in name and i % 3 == 0):
                it['rot'] = [80.0 if 'Chair' in name else 0.0, src['rot'][1] + 37.0 * (i % 7), 90.0 if 'Barrels' in name else 0.0]
                it['loc'] = [src['loc'][0] + 25.0 * ((i % 5) - 2), src['loc'][1] + 30.0 * ((i % 3) - 1), src['loc'][2] + (22.0 if 'Barrels' in name else 35.0)]
                n_fallen += 1
        else:
            it['intensity'] = src.get('intensity', 3.0) * 0.35
        items.append(it)
    # Her tube's glass (the demo's glass is a separate piece, not in the extracted room).
    tube = next(it for it in room if 'LabCapsule11_1_Capsule' in it.get('mesh', ''))
    glass = dict(tube)
    glass['mesh'] = '/Game/SciFiWorld/Meshes/SM_LabCapsule11_1_Glass'
    glass['mats'] = []
    items.append(glass)
    n = place_section(s, 'Lab', items, LAB_GATE, to=VALK_TO, yaw=-90.0)
    # The 1.2 m between the gate and the room's own doorway: floor, dark sides and lintel.
    g = lab_world(-1425, 400, 50)
    front = lab_world(700, 400, 0, VALK_TO)
    plate(s, g[0] - 250, g[0] + 250, g[1] - 6, front[1] + 30, g[2] + 1.0)
    box(s, 'Passage', (g[0] - 270, g[0] - 250), (g[1] + 4, front[1]), (g[2], g[2] + 380))
    box(s, 'Passage', (g[0] + 250, g[0] + 270), (g[1] + 4, front[1]), (g[2], g[2] + 380))
    box(s, 'Passage', (g[0] - 270, g[0] + 270), (g[1] + 4, front[1]), (g[2] + 350, g[2] + 380))
    # Light. Cold on her tube, so it is the brightest thing seen through the gate; red lamps where the fight was.
    tx, ty, tz = lab_world(tube['loc'][0] + 190, tube['loc'][1], tube['loc'][2] + 260, VALK_TO)
    light(s, 'Lamps', 'PointLight', (tx, ty, tz), rot(), {'intensity': 9.0, 'color': (150, 225, 255), 'attenuation_radius': 1100.0, 'shadow': False})
    light(s, 'Lamps', 'SpotLight', lab_world(-800, 400, 480, VALK_TO), rot(pitch=-35, yaw=90), {'intensity': 22.0, 'color': (170, 230, 255), 'attenuation_radius': 1600.0, 'shadow': False})
    for x, y, on in ((300, -200, True), (-300, 1000, True), (-1100, -150, False), (350, 900, False)):
        light(s, 'Lamps', 'PointLight', lab_world(x, y, 300, VALK_TO), rot(),
              {'intensity': 3.5, 'color': (255, 70, 45), 'attenuation_radius': 800.0, 'shadow': False, 'flicker': not on})
    # Marks of the fight: scorches where shots landed, grime where things burned.
    marks = ((250, 300, 190, 1), (-120, 620, 240, 3), (480, 760, 160, 5), (-520, -60, 210, 2), (60, -150, 170, 6), (-300, 250, 260, 4),
             (420, 80, 150, 7), (-860, 380, 230, 1), (-1150, 560, 180, 3), (-640, 900, 200, 5), (130, 980, 220, 2), (-1000, 150, 170, 6))
    for k, (x, y, size, which) in enumerate(marks):
        z = 50 if (-1425 < x < -375 and 50 < y < 750) else 0
        decal(s, SCORCH % which, lab_world(x, y, z + 5, VALK_TO), size, yaw=47.0 * k)
    for k, (x, y, size, which) in enumerate(((0, 400, 420, 1), (-700, 400, 380, 3), (300, 900, 340, 2), (-200, -100, 360, 4))):
        z = 50 if (-1425 < x < -375 and 50 < y < 750) else 0
        decal(s, DIRT % which, lab_world(x, y, z + 5, VALK_TO), size, yaw=71.0 * k)
    log.append('valk: the Valkyrie\'s room, %d pieces and lights of the lab kit room (whole, without its entry hall), dimmed materials, %d things knocked over, %d scorch and 4 grime marks'
               % (n, n_fallen, len(marks)))


BOSS = (1980.0, 4780.0, 7600.0, 11200.0)     # the sub-boss hall's inner wall faces: x0, x1, y0, y1 (28 x 36 m)
GAL_Z = 400.0                                # its galleries: 4 m up, 4 m wide
HTB_MAT = '/Game/High_Tech_Base/Materials/'
WALL_MATS = [HTB_MAT + 'MI_Tile_Grate_Ceiling_04', HTB_MAT + 'MI_Sci_Fi_Hard_Surface_01', HTB_MAT + 'MI_Rubber_03']


def hall_wall(s, face, a, z, bounds=None):
    """One 4 x 6 m wall module of the base's kit on a wall of a hall with inner faces `bounds` (x0, x1, y0, y1; the
    sub-boss hall by default). face: S, N, W or E; a: where the module starts along that wall. The module spans its
    own X 0..400 and its front is 38 cm towards its own +Y."""
    x0, x1, y0, y1 = bounds or BOSS
    if face == 'S':
        loc, yaw = (a, y0 - 38, z), 0
    elif face == 'N':
        loc, yaw = (a + 400, y1 + 38, z), 180
    elif face == 'W':
        loc, yaw = (x0 - 38, a + 400, z), -90
    else:
        loc, yaw = (x1 + 38, a, z), 90
    mesh(s, 'Walls', HTB + 'SM_Wall_01', loc, rot(yaw=yaw), mats=WALL_MATS, collide=True)


BIG = 1.38                                   # the size the kit's author gives its crates in his own hall
BIG_W, BIG_H = CRATE_W * BIG, CRATE_H * BIG  # 141 cm wide, 138 cm high


def boss_crate(s, x, y, yaw=0.0, level=0, z=0.0):
    """A cargo crate at the author's size; level 1 stands on another, flush (its underside on the lower one's rim)."""
    crate(s, 'Crates', x, y, z, yaw, level, BIG)


WALL3_MATS = [HTB_MAT + 'MI_Rubber_03', HTB_MAT + 'MI_Sci_Fi_Hard_Surface_02', HTB_MAT + 'MI_Tile_Grate_Ceiling_04']


def hall_corners(s, bounds, z_list):
    """The kit's corner pillar in each corner of a hall. Its wall module is set back 19 cm below its top band, so two
    walls that meet leave an open slot down the corner (owner, 2026-11-14: "all the corners have gaps between
    walls"). The pillar (1.42 m square) stands over the slot, 62 cm into the room on each side."""
    x0, x1, y0, y1 = bounds
    for cx, cy in ((x0 - 9, y0 - 9), (x1 + 9, y0 - 9), (x0 - 9, y1 + 9), (x1 + 9, y1 + 9)):
        for z in z_list:
            mesh(s, 'Walls', HTB + 'SM_Wall_03', (cx + 69.7, cy + 30.0, z), mats=WALL3_MATS, collide=True)


def floor_fit(s, x0, x1, y0, y1, z):
    """The kit's freight-floor strip (7.5 x 21 m) laid to fill exactly x0..x1, y0..y1: whole strips, each stretched a
    little so the last one ends on the wall line and nothing sticks out into the next room."""
    nx = max(1, int(math.ceil((x1 - x0) / 750.0 - 0.01)))
    ny = max(1, int(math.ceil((y1 - y0) / 2100.0 - 0.01)))
    wx, wy = (x1 - x0) / nx, (y1 - y0) / ny
    for i in range(nx):
        for j in range(ny):
            mesh(s, 'Floor', HTB + 'SM_Floor_02', (x0 + i * wx, y0 + j * wy, z), rot(yaw=-90), scale=(wy / 2100.0, wx / 750.0, 1.0),
                 mats=[HTB_MAT + 'MI_R5', HTB_MAT + 'MI_R3'], collide=True)
    box(s, 'Collision', (x0 - 20, x1 + 20), (y0 - 20, y1 + 20), (z - 60, z - 2))


def roof_fit(s, x0, x1, y0, y1, z, mat='MI_Tile_Grate_Ceiling_01'):
    """The kit's ceiling plane (17.5 x 22 m) laid to fill exactly x0..x1, y0..y1."""
    nx = max(1, int(math.ceil((x1 - x0) / 1750.0 - 0.01)))
    ny = max(1, int(math.ceil((y1 - y0) / 2200.0 - 0.01)))
    wx, wy = (x1 - x0) / nx, (y1 - y0) / ny
    for i in range(nx):
        for j in range(ny):
            mesh(s, 'Roof', HTB + 'SM_Roof_01', (x0 + (i + 1) * wx, y0 + j * wy, z), rot(yaw=-90), scale=(wy / 2200.0, wx / 1750.0, 1.0), mats=[HTB_MAT + mat])


async def stage_boss():
    """The sub-boss hall, behind the long corridor's far door (owner, 2026-11-14: "a wide, tall hall, with railed
    galleries where enemies can walk and shoot, and a door on the other side; some crates, some of them stacked,
    always within the player's jump, and by jumping on some crates the player can get onto the side galleries").
    28 x 36 m, 12 m high, from the base's own kit: two tiers of its wall modules, the freight branch's floor and
    lit ceiling, a freight door in the far wall that stays shut until the sub-boss is beaten.
    Galleries 4 m up along both side walls, joined over the entry by a bridge (the player comes in under it): the
    same grated deck, posts and mesh railing as the transfer bay's catwalk. No stair: the way up is the crates.
    The player's jump rises 2 m (Player.json), and every step here is 1.4 m or less: floor -> crate (1.38 m) ->
    two crates (2.76 m) -> gallery (4 m), through a gap in the railing. Each gallery has two such piles."""
    s = 'boss'
    x0, x1, y0, y1 = BOSS
    cx = (x0 + x1) / 2.0
    # Shell. The module in front of the corridor's door and the three behind the freight door's surround are left out.
    for z in (0.0, 600.0):
        for a in range(int(x0), int(x1), 400):
            if not (z == 0 and a == 3180):
                hall_wall(s, 'S', a, z)
            if not (z == 0 and a in (2780, 3180, 3580)):
                hall_wall(s, 'N', a, z)
        for a in range(int(y0), int(y1), 400):
            hall_wall(s, 'W', a, z)
            hall_wall(s, 'E', a, z)
    hall_corners(s, BOSS, (0.0, 600.0))
    # Floor: the freight branch's floor strips (7.5 x 21 m), four abreast, two along. Roof: the kit's ceiling plane.
    for fx in (cx - 1500, cx - 750, cx, cx + 750):
        for fy in (y0 - 60, y0 + 2040):
            mesh(s, 'Floor', HTB + 'SM_Floor_02', (fx, fy, 0), rot(yaw=-90), mats=[HTB_MAT + 'MI_R5', HTB_MAT + 'MI_R3'], collide=True)
    box(s, 'Collision', (x0 - 60, x1 + 60), (y0 - 54, y1 + 60), (-60, -2))
    for rx in (cx, cx + 1750):
        for ry in (y0 - 100, y0 + 2100):
            mesh(s, 'Roof', HTB + 'SM_Roof_01', (rx, ry, 1200), rot(yaw=-90), mats=[HTB_MAT + 'MI_Tile_Grate_Ceiling_01'])
    for ly in (y0 - 100, y0 + 1800):
        mesh(s, 'Roof', HTB + 'SM_Lighting_Ceiling_01', (cx - 575, ly, 1125), mats=[HTB_MAT + 'MI_R3', HTB_MAT + 'MI_Tile_Grate_Ceiling_03', HTB_MAT + 'MI_Emissive_04'])
    for ly in (8000, 8800, 9600, 10400, 11000):
        light(s, 'Lamps', 'RectLight', (cx, ly, 1110), rot(pitch=-90), {'intensity': 30.0, 'color': (255, 232, 205), 'attenuation_radius': 2200.0,
                                                                    'source_width': 300.0, 'source_height': 600.0, 'shadow': False})
    # Entry: a dark frame around the corridor's door, which is narrower and lower than the wall module left out.
    box(s, 'Frame', (3165, 3180), (y0 - 54, y0 + 4), (0, 600))
    box(s, 'Frame', (3580, 3595), (y0 - 54, y0 + 4), (0, 600))
    box(s, 'Frame', (3165, 3595), (y0 - 54, y0 + 4), (298, 600))
    # Exit: the kit's freight door in its surround. It opens when the sub-boss is beaten (flag; nothing sets it yet).
    mesh(s, 'Exit', HTB + 'SM_Door_02_Side', (cx - 600, y1 + 49, 0), rot(yaw=0), mats=[HTB_MAT + 'MI_R6_Inst', HTB_MAT + 'MI_Metallic_Floor', HTB_MAT + 'MI_Decal_01'], collide=True)
    door(s, HTB + 'SM_Door_02', (cx + 425, y1 + 49, 0), 180, slide=(0, 0, 430), centre=(425, 25, 150), distance=600,
         mats=[HTB_MAT + 'MI_Door_03', HTB_MAT + 'MI_Tile_Machine_02'], flag='FutureSubBossDefeated')
    for dx in (-520, 520):
        light(s, 'Lamps', 'PointLight', (cx + dx, y1 - 120, 520), rot(), {'intensity': 5.0, 'color': (255, 90, 40), 'attenuation_radius': 900.0, 'shadow': False})
    # Galleries and the bridge over the entry.
    deck(s, x0, x0 + 400, y0, y1, GAL_Z)
    deck(s, x1 - 400, x1, y0, y1, GAL_Z)
    deck(s, x0 + 400, x1 - 400, y0, y0 + 400, GAL_Z)
    for py in range(int(y0) + 400, int(y1) + 1, 400):
        post(s, x0 + 390, min(py, y1 - 10), GAL_Z)
        post(s, x1 - 390, min(py, y1 - 10), GAL_Z)
    for px in (2780, 3160, 3600, 3980):                  # two of them flank the entry
        post(s, px, y0 + 390, GAL_Z)
    wi, ei, by = x0 + 400, x1 - 400, y0 + 400            # the galleries' inner edges and the bridge's
    rail(s, wi, by, ei, by, GAL_Z)
    for a, b in ((by, 9000), (9200, 10600), (10800, y1)):          # west railing, open at its two crate piles
        rail(s, wi, a, wi, b, GAL_Z)
    for a, b in ((by, 8400), (8600, 10000), (10200, y1)):          # east railing, the same
        rail(s, ei, a, ei, b, GAL_Z)
    # The ways up: at each gap, two crates stacked against the gallery and one beside them as the first step.
    half = BIG_W / 2.0 + 1.0
    for gx, sx, gy, step in ((wi + half, wi + half + BIG_W, 9100, None), (wi + half, None, 10700, 10700 - BIG_W),
                             (ei - half, ei - half - BIG_W, 10100, None), (ei - half, None, 8500, 8500 + BIG_W)):
        boss_crate(s, gx, gy, 0, 0)
        boss_crate(s, gx, gy, 4, 1)
        if sx:
            boss_crate(s, sx, gy, 0, 0)                  # the step stands out into the hall, against the stack
        else:
            boss_crate(s, gx, step, 0, 0)                # the step stands along the wall, against the stack
    # Cover on the floor, in groups a loader would leave, some stacked. The middle of the hall stays clear for the fight.
    for x, y, yaw, level in ((3000, 8500, 0, 0), (3000 + BIG_W, 8500, 0, 0), (3000 + BIG_W / 2, 8500, -5, 1),
                             (3900, 9000, 0, 0), (3900, 9000 + BIG_W, 0, 0), (3900, 9000 + BIG_W / 2, 12, 1),
                             (2800, 9900, 0, 0), (2800 + BIG_W, 9900, 0, 0),
                             (3700, 10500, 0, 0), (3700 + BIG_W, 10500, 0, 0), (3700 + BIG_W / 2, 10500, -4, 1), (3700 + BIG_W / 2, 10500 + BIG_W, 0, 0),
                             (2900, 10650, 15, 0)):
        boss_crate(s, x, y, yaw, level)
    # A stepped pile in the far corner: one, two and three crates. Its top is above the east gallery's railing.
    for k, levels in enumerate((1, 2, 3)):
        for level in range(levels):
            boss_crate(s, 4250 - (2 - k) * BIG_W, 10990, 3 * level, level)
    # Wall lamps: under the galleries, so nobody fights in a black strip, and above them.
    for ly in range(8400, 11000, 800):
        work_light(s, x0 + 12, ly, 330, 0)
        work_light(s, x1 - 12, ly, 330, 180)
        if ly + 400 < y1 - 300:                          # the last pair would hang in the corner itself
            work_light(s, x0 + 12, ly + 400, 820, 0, intensity=5.0, radius=1200.0)
            work_light(s, x1 - 12, ly + 400, 820, 180, intensity=5.0, radius=1200.0)
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(cx, (y0 + y1) / 2.0, 300), rot())
    nav.set_actor_scale3d(unreal.Vector(15.0, 19.0, 8.0))
    tag(nav, s, 'Gameplay')
    # No sub-boss yet. When it comes: its encounter in the middle, (3380, 9500, 150), FUTURE_ONLY, and the exit's flag.
    log.append('boss: sub-boss hall 28 x 36 x 12 m, corner pillars, two side galleries and a bridge 4 m up, 4 crate piles up to them, a stepped pile, 13 cover crates, exit door shut')


DEEP_Z = -600.0                               # the deep wing's floor: 6 m under the base's
WELL = (5300.0, 6900.0, -600.0, 600.0)        # the stairwell behind the bay's shutter (inner faces x0, x1, y0, y1)
WORN_X = (6954.0, 11000.0)                    # the worn corridor: its open end and its far door
NEST = (11054.0, 12254.0, -600.0, 600.0)      # the small room that will be infested
DARK = (12254.0, 14762.0, -200.0, 200.0)      # the dark corridor, wall face to wall face
PIT = (14762.0, 18362.0, -1400.0, 1400.0)     # the hall of the tentacle boss: a dead end
RUBBLE = '/Game/CastleTown/Candle/StaticMeshes/Rubble/'
DWS = '/Game/DeepWaterStation/Meshes/'


def shell(s, bounds, tiers, skip=()):
    """Walls of the base's kit around `bounds`, one 6 m tier per entry of `tiers` (its floor height). `skip` lists
    the modules left out as (face, start along the wall, tier height)."""
    x0, x1, y0, y1 = bounds
    for z in tiers:
        for a in range(int(x0), int(x1), 400):
            for face in ('S', 'N'):
                if (face, a, z) not in skip:
                    hall_wall(s, face, a, z, bounds)
        for a in range(int(y0), int(y1), 400):
            for face in ('W', 'E'):
                if (face, a, z) not in skip:
                    hall_wall(s, face, a, z, bounds)
    hall_corners(s, bounds, tiers)


def sized(s, folder, path, loc, longest, r=None, collide=True):
    """A mesh scaled so that its longest side is `longest` cm (for pieces of another kit whose size is not known here)."""
    b = asset(path).get_bounding_box()
    k = longest / max(b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z, 1.0)
    return mesh(s, folder, path, loc, r, scale=(k, k, k), collide=collide)


def ship_mats(mesh_path, looks):
    """Materials for one placed copy of a Spaceship Corridor mesh: `looks` maps the start of a slot's name (letters
    and digits only, lower case) to a material; slots not named keep the mesh's own."""
    out = []
    for slot in asset(mesh_path).static_materials:
        key = ''.join(c for c in str(slot.material_slot_name).lower() if c.isalnum())
        out.append(next((m for k, m in looks if key.startswith(k)), None))
    return out


async def stage_deep():
    """The deep wing (owner, 2026-11-14), behind the bay's breakable shutter: "a place that is already under water":
    stairs down; a corridor with failing light, a little dark, the destroyed-looking corridor asset "used worn, with
    one or two lights flickering, a bit scary"; a small room that will be infested (no enemies placed); a corridor in
    complete darkness, of medium length, later full of smoke; and at its end a large hall, with galleries around it
    but not all the way, with what is damaged or broken: the tentacle boss's. A dead end.
    Everything is 6 m below the base's floor. Walls, floors and ceilings are the base's kit; the worn corridor is the
    Spaceship Corridor model again with worn materials (M_ShipCorridor_Worn) and its windows glowing like deep water."""
    s = 'deep'
    Z = DEEP_Z
    # The wing has its own exposure. The base's setting brightens whatever it finds in a dark place until it is
    # white (the first look in the game: glowing windows and a dead light strip, both blown out). Two stops less
    # here, so dark stays dark and the few lamps are what the player sees by.
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector((WELL[0] + PIT[1]) / 2.0, 0, Z + 300), rot())
    ppv.set_actor_scale3d(unreal.Vector((PIT[1] - WELL[0]) / 200.0 + 0.3, 16.0, 9.0))
    st = ppv.get_editor_property('settings')
    st.set_editor_property('override_auto_exposure_bias', True)
    st.set_editor_property('auto_exposure_bias', 2.0)
    ppv.set_editor_property('settings', st)
    ppv.set_editor_property('priority', 1.0)
    ppv.set_editor_property('blend_radius', 250.0)
    tag(ppv, s, 'Light')
    # ---- the stairwell: a landing at the base's level, a 4 m stair down its middle, the lower floor
    x0, x1, y0, y1 = WELL
    shell(s, WELL, (Z, 0.0), skip=[('W', a, 0.0) for a in (-600, -200, 200)] + [('E', -200, Z)])
    for ya, yb in ((-600, -500), (500, 600)):                       # the bay's own end pieces are open at the back
        box(s, 'Backing', (x0 - 12, x0), (ya, yb), (0, 600))
    box(s, 'Backing', (x0 - 12, x0), (-600, 600), (562, 600))
    floor_fit(s, x0, x1, y0, y1, Z)
    floor_fit(s, x0, x0 + 400, y0, y1, 0.0)                         # the landing
    roof_fit(s, x0, x1, y0, y1, 600.0)
    for y, rails in ((-100, (True, False)), (100, (False, True))):
        stairs(s, x0 + 1300, y, Z, 180, 6, rails=rails)
    rail(s, x0 + 400, -600, x0 + 400, -200, 0.0)
    rail(s, x0 + 400, 200, x0 + 400, 600, 0.0)
    for y in (-590, -300, 300, 590):
        post(s, x0 + 390, y, 0.0, Z)
    box(s, 'Frame', (x1 - 4, x1 + 54), (-215, -200), (Z, 0))        # the worn corridor's mouth in the east wall
    box(s, 'Frame', (x1 - 4, x1 + 54), (200, 215), (Z, 0))
    box(s, 'Frame', (x1 - 4, x1 + 54), (-215, 215), (Z + 318, 0))
    teal = (120, 215, 200)
    # With the wing's exposure a lamp has to be about four times a lamp of the base to light the same.
    work_light(s, x0 + 200, y1 - 12, 330, -90, color=teal, intensity=45.0, radius=1200.0)
    work_light(s, x0 + 200, y0 + 12, 330, 90, color=teal, intensity=45.0, radius=1200.0)
    work_light(s, x0 + 1000, y0 + 12, Z + 380, 90, color=teal, intensity=45.0, radius=1300.0)
    work_light(s, x0 + 1000, y1 - 12, Z + 380, -90, color=teal, intensity=45.0, radius=1300.0)
    light(s, 'Lamps', 'RectLight', (x0 + 850, 0, 560), rot(pitch=-90), {'intensity': 160.0, 'color': (190, 235, 225), 'attenuation_radius': 1700.0,
                                                                      'source_width': 200.0, 'source_height': 500.0, 'shadow': False})
    light(s, 'Lamps', 'PointLight', (x1 - 250, 0, Z + 330), rot(), {'intensity': 6.0, 'color': teal, 'attenuation_radius': 800.0, 'shadow': False, 'flicker': True})
    # Deep Water Station (owner: "here we could use the Deep Water Station assets to make stairs going down"). The
    # pack has no stair, only ladders, which the player cannot climb, so the stair is the base's own. What the pack
    # gives this stairwell is the sea: portholes with the glow of deep water behind them, its tube lamps, a rack of
    # divers' gas cylinders, and shellfish and stones that have got in.
    water = SHIP + 'MI_ShipCorridor_Water'
    for px in (x0 + 700, x0 + 1050, x0 + 1400):
        for wall_y, behind in ((y1 - 4, 14), (y0 + 4, -14)):
            mesh(s, 'Sea', DWS + 'SM_WinMetal02', (px, wall_y, Z + 230), rot(yaw=90))
            mesh(s, 'Sea', '/Engine/BasicShapes/Cylinder', (px, wall_y + behind, Z + 230), rot(pitch=90, yaw=90), scale=(1.3, 1.3, 0.02), mats=[water])
    for px in (x0 + 875, x0 + 1225):
        mesh(s, 'Sea', DWS + 'SM_Lamp05', (px, y1 - 2, Z + 240), rot(yaw=90))
        mesh(s, 'Sea', DWS + 'SM_Lamp05', (px, y0 + 2, Z + 240), rot(yaw=-90))
    mesh(s, 'Sea', DWS + 'SM_GasCylinderA02', (x1 - 420, y0 + 340, Z + 36), collide=True)
    mesh(s, 'Sea', DWS + 'SM_GasCylinderA03', (x1 - 420, y1 - 120, Z + 17), collide=True)
    for k, (mx, my, size) in enumerate(((x0 + 120, y0 + 110, 1.5), (x0 + 210, y0 + 80, 1.0), (x0 + 90, y0 + 230, 1.2), (x1 - 110, y1 - 500, 1.3),
                                        (x1 - 190, y1 - 420, 0.9), (x0 + 520, y1 - 90, 1.4), (x0 + 610, y1 - 150, 1.0))):
        mesh(s, 'Sea', DWS + 'SM_Mollusk02', (mx, my, Z), rot(yaw=51.0 * k), scale=(size, size, size))
    for k, (mx, my) in enumerate(((x0 + 160, y0 + 170), (x1 - 150, y1 - 470), (x0 + 560, y1 - 120))):
        mesh(s, 'Sea', DWS + 'SM_RockSet05', (mx, my, Z + 30), rot(yaw=80.0 * k), scale=(0.6, 0.6, 0.6))
    # ---- the worn corridor
    origin = (WORN_X[0] + 2256.0, 0.0, Z - 13.0)                    # turned 90: its open end to the stairwell, its door 40 m east
    hull = SHIP + 'MI_ShipCorridor_HullWorn'
    worn = [('proceduralimperfections', hull), ('roofgridmetal', SHIP + 'MI_ShipCorridor_GridWorn'), ('metalpipes', SHIP + 'MI_ShipCorridor_PipesWorn'),
            ('emmision', SHIP + 'MI_ShipCorridor_LightDead'), ('glass', SHIP + 'MI_ShipCorridor_Water'),
            ('screen', SHIP + 'MI_ShipCorridor_Glass'), ('flickeringemmision', SHIP + 'MI_ShipCorridor_LightFlicker')]
    for part in ('SM_ShipCorridor_Body', 'SM_ShipCorridor_Details'):
        mesh(s, 'Ship', SHIP + part, origin, rot(yaw=90), mats=ship_mats(SHIP + part, worn), collide=part.endswith('Body'))
    for leaf in ('SM_ShipCorridor_DoorA', 'SM_ShipCorridor_DoorB'):
        b = asset(SHIP + leaf).get_bounding_box()
        side = 1.0 if (b.min.x + b.max.x) > 0 else -1.0
        door(s, SHIP + leaf, origin, 90, slide=(side * 238.0, 0.0, 0.0), centre=(0.0, (b.min.y + b.max.y) / 2.0, 150.0), distance=450, mats=[hull])
    box(s, 'Collision', (WORN_X[0], WORN_X[1]), (-160, 160), (Z - 10, Z + 2), visible=False)
    box(s, 'Collision', (WORN_X[0], WORN_X[1]), (-270, 270), (Z - 45, Z - 16), visible=False)
    box(s, 'Collision', (origin[0] + 259, origin[0] + 363), (63, 167), (Z, Z + 66), visible=False)     # its crate
    plate(s, WELL[1] - 6, WORN_X[0] + 6, -200, 200, Z + 1.0)       # under the stairwell's wall (the builder's walk check found the gap)
    plate(s, WORN_X[1] - 6, NEST[0] + 6, -200, 200, Z + 1.0)       # under the small room's wall
    for lx in (WORN_X[0] + 1000, WORN_X[0] + 3050):                 # the two lamps that still try to work
        light(s, 'Lamps', 'RectLight', (lx, 0, Z + 288), rot(pitch=-90), {'intensity': 22.0, 'color': (255, 225, 190), 'attenuation_radius': 1000.0,
                                                                         'source_width': 20.0, 'source_height': 300.0, 'shadow': False, 'flicker': True})
    for lx in range(int(WORN_X[0]) + 500, int(WORN_X[1]), 1000):    # what the deep water gives through the windows, so the way is never blind
        light(s, 'Lamps', 'PointLight', (lx, 0, Z + 200), rot(), {'intensity': 3.0, 'color': (70, 190, 200), 'attenuation_radius': 700.0, 'shadow': False})
    # ---- the small room (the infestation comes with its enemies, later)
    shell(s, NEST, (Z,), skip=[('W', -200, Z), ('E', -200, Z)])
    floor_fit(s, NEST[0], NEST[1], NEST[2], NEST[3], Z)
    roof_fit(s, NEST[0], NEST[1], NEST[2], NEST[3], 0.0)
    box(s, 'Frame', (NEST[0] - 54, NEST[0] + 4), (-215, -200), (Z, 0))          # the corridor's door, narrower and lower than the module left out
    box(s, 'Frame', (NEST[0] - 54, NEST[0] + 4), (200, 215), (Z, 0))
    box(s, 'Frame', (NEST[0] - 54, NEST[0] + 4), (-215, 215), (Z + 298, 0))
    box(s, 'Frame', (NEST[1] - 4, NEST[1] + 54), (-200, 200), (Z + 350, 0))     # the way on is as low as the dark corridor
    for x, y, yaw, level in ((NEST[0] + 180, 420, 0, 0), (NEST[0] + 180 + CRATE_W, 420, 0, 0), (NEST[0] + 180, 420, 8, 1), (NEST[1] - 200, -430, 20, 0)):
        crate(s, 'Props', x, y, Z, yaw, level)
    for x, y in ((NEST[1] - 260, 380), (NEST[1] - 190, 440), (NEST[0] + 300, -470)):
        mesh(s, 'Props', HTB + 'SM_Prop_04', (x, y, Z), collide=True)
    mesh(s, 'Props', HTB + 'SM_Prop_04', (NEST[0] + 520, -380, Z + 36), rot(roll=90, yaw=40), collide=True)      # one drum on its side
    for k, (mx, my, size) in enumerate(((NEST[0] + 120, -500, 1.6), (NEST[0] + 230, -520, 1.1), (NEST[0] + 150, -400, 1.3), (NEST[1] - 130, 520, 1.5),
                                        (NEST[1] - 240, 500, 1.0), (NEST[1] - 120, 400, 1.2), (NEST[0] + 620, 540, 1.4), (NEST[1] - 500, -540, 1.2))):
        mesh(s, 'Sea', DWS + 'SM_Mollusk02', (mx, my, Z), rot(yaw=47.0 * k), scale=(size, size, size))      # the first of what infests the room
    for k, (x, y, size, which) in enumerate(((NEST[0] + 600, 0, 380, 2), (NEST[0] + 300, -300, 300, 4), (NEST[1] - 350, 250, 340, 1))):
        decal(s, DIRT % which, (x, y, Z + 5), size, yaw=63.0 * k)
    light(s, 'Lamps', 'PointLight', ((NEST[0] + NEST[1]) / 2.0, 0, Z + 480), rot(), {'intensity': 14.0, 'color': (255, 80, 50), 'attenuation_radius': 1100.0, 'shadow': False, 'flicker': True})
    light(s, 'Lamps', 'PointLight', (NEST[0] + 250, 350, Z + 300), rot(), {'intensity': 6.0, 'color': (255, 90, 55), 'attenuation_radius': 900.0, 'shadow': False})      # one that holds
    # ---- the dark corridor: 25 m, 4 m wide, 3.5 m high, no lamp at all
    for a in range(int(DARK[0]) + 54, int(DARK[1]) - 54, 400):
        hall_wall(s, 'S', a, Z, DARK)
        hall_wall(s, 'N', a, Z, DARK)
    floor_fit(s, DARK[0], DARK[1], DARK[2], DARK[3], Z)
    box(s, 'Ceiling', (DARK[0], DARK[1]), (DARK[2] - 60, DARK[3] + 60), (Z + 350, Z + 372))
    # ---- the hall of the tentacle boss
    x0, x1, y0, y1 = PIT
    G = Z + 400.0                                                    # its galleries, 4 m up
    shell(s, PIT, (Z, 0.0), skip=[('W', -200, Z)])
    box(s, 'Frame', (x0 - 54, x0 + 4), (-200, 200), (Z + 350, 0))
    floor_fit(s, x0, x1, y0, y1, Z)
    roof_fit(s, x0, x1, y0, y1, 600.0)
    # Galleries along the walls, never all the way round: the south one stops half way, the west one is a stub over
    # the south corner, the north and east ones join. Where a gallery ends it has no railing: it was torn off.
    deck(s, x0, x0 + 2000, y0, y0 + 400, G)                          # south, 20 m, ends in the open
    deck(s, x0, x0 + 400, y0 + 400, y0 + 1000, G)                    # west stub
    deck(s, x0 + 1200, x1, y1 - 400, y1, G)                          # north, 24 m
    deck(s, x1 - 400, x1, y0 + 800, y1 - 400, G)                     # east, joined to the north one
    for px in range(int(x0) + 400, int(x0) + 2001, 400):
        post(s, px, y0 + 390, G, Z)
    for px in range(int(x0) + 1200, int(x1) - 399, 400):
        post(s, px, y1 - 390, G, Z)
    for py in range(int(y0) + 800, int(y1) - 399, 400):
        post(s, x1 - 390, py, G, Z)
    post(s, x0 + 390, y0 + 1000, G, Z)
    rail(s, x0 + 400, y0 + 400, x0 + 1400, y0 + 400, G)             # south: its last 6 m have lost their railing
    rail(s, x0 + 400, y0 + 400, x0 + 400, y0 + 800, G)              # west stub: the last 2 m too
    rail(s, x0 + 1600, y1 - 400, x0 + 2400, y1 - 400, G)            # north: railing in two runs, a gap between them
    rail(s, x0 + 2800, y1 - 400, x1 - 400, y1 - 400, G)
    rail(s, x1 - 400, y0 + 1200, x1 - 400, y1 - 400, G)             # east: its south end is open
    # What fell: deck tiles and railing from the torn ends, on the floor under them.
    tile, railing = MSS + 'Floor/SM_floor_200x200_02', MSS + 'Railing/SM_railing_200_02'
    mesh(s, 'Wreck', tile, (x0 + 2050, y0 + 260, Z + 95), rot(pitch=-38, yaw=12), collide=True)          # one end still on the gallery's edge line
    mesh(s, 'Wreck', tile, (x0 + 2260, y0 + 520, Z + 4), rot(yaw=31), collide=True)
    mesh(s, 'Wreck', railing, (x0 + 1750, y0 + 640, Z + 8), rot(roll=86, yaw=-24), collide=True)
    mesh(s, 'Wreck', railing, (x1 - 620, y0 + 900, Z + 8), rot(roll=84, yaw=70), collide=True)
    mesh(s, 'Wreck', tile, (x1 - 560, y0 + 700, Z + 60), rot(pitch=24, roll=18, yaw=-40), collide=True)
    # The castle above has come through the roof: its stone lies in a heap off the middle, under a cold shaft of light.
    hx, hy = x0 + 2300, 350.0
    for k, (dx, dy, size, yaw, which) in enumerate(((0, 0, 420, 0, 'SM_Rubble_02'), (190, 120, 300, 70, 'SM_Rubble_02'), (-210, 90, 340, 160, 'SM_Rubble_02'),
                                                    (60, -230, 280, 250, 'SM_Rubble_02'), (-120, -140, 260, 30, 'SM_PillarTop_Broken'), (260, -90, 220, 200, 'SM_PillarTop_Broken'))):
        sized(s, 'Rubble', RUBBLE + which, (hx + dx, hy + dy, Z), size, rot(yaw=yaw))
    light(s, 'Lamps', 'SpotLight', (hx, hy, 560), rot(pitch=-90), {'intensity': 260.0, 'color': (170, 215, 255), 'attenuation_radius': 1500.0, 'shadow': False})
    # Crates thrown about (one way up to the south gallery is still standing), scorch and grime.
    half = BIG_W / 2.0 + 1.0
    boss_crate(s, x0 + 900, y0 + 400 + half, 0, 0, Z)
    boss_crate(s, x0 + 900, y0 + 400 + half, 5, 1, Z)
    boss_crate(s, x0 + 900, y0 + 400 + half + BIG_W, 0, 0, Z)
    for x, y, r in ((x0 + 1400, -300, rot(roll=90, yaw=20)), (x1 - 900, -700, rot(pitch=90, yaw=75)), (x0 + 2900, 800, rot(roll=180, yaw=40)), (x1 - 1300, 500, rot(yaw=33))):
        mesh(s, 'Crates', CRATE, (x, y, Z + (BIG_W / 2.0 if abs(r.roll) == 90 or abs(r.pitch) == 90 else (BIG_H if abs(r.roll) == 180 else 0.0))), r, scale=(BIG, BIG, BIG), collide=True)
    for k, (x, y, size, which) in enumerate(((x0 + 700, 200, 300, 1), (x0 + 1500, -600, 360, 3), (x0 + 2000, 900, 280, 5), (x0 + 2700, -200, 420, 2),
                                             (x1 - 800, 300, 340, 6), (x1 - 500, -900, 300, 4), (x0 + 1100, 700, 260, 7), (x1 - 1500, -400, 380, 1))):
        decal(s, SCORCH % which, (x, y, Z + 5), size, yaw=53.0 * k)
    for k, (x, y, size, which) in enumerate(((x0 + 1800, 0, 600, 1), (x1 - 1000, 600, 520, 3), (x0 + 900, -900, 480, 2))):
        decal(s, DIRT % which, (x, y, Z + 5), size, yaw=77.0 * k)
    # Light: what is left of it. Two of the hall's lamps, one of them failing, and red where the galleries broke.
    light(s, 'Lamps', 'RectLight', (x0 + 900, -500, 560), rot(pitch=-90), {'intensity': 80.0, 'color': (255, 232, 205), 'attenuation_radius': 2200.0,
                                                                          'source_width': 300.0, 'source_height': 600.0, 'shadow': False, 'flicker': True})
    light(s, 'Lamps', 'RectLight', (x1 - 900, 300, 560), rot(pitch=-90), {'intensity': 70.0, 'color': (255, 232, 205), 'attenuation_radius': 2400.0,
                                                                         'source_width': 300.0, 'source_height': 600.0, 'shadow': False})
    light(s, 'Lamps', 'RectLight', (x0 + 1900, -700, 560), rot(pitch=-90), {'intensity': 45.0, 'color': (255, 225, 200), 'attenuation_radius': 2200.0,
                                                                           'source_width': 300.0, 'source_height': 600.0, 'shadow': False})
    for x, y in ((x0 + 2000, y0 + 420), (x1 - 420, y0 + 800), (x0 + 400, y0 + 1000)):
        light(s, 'Lamps', 'PointLight', (x, y, G + 150), rot(), {'intensity': 14.0, 'color': (255, 70, 45), 'attenuation_radius': 1000.0, 'shadow': False})
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector((WELL[0] + PIT[1]) / 2.0, 0, Z + 300), rot())
    nav.set_actor_scale3d(unreal.Vector((PIT[1] - WELL[0]) / 200.0 + 1.0, 15.0, 8.0))
    tag(nav, s, 'Gameplay')
    # No enemies and no boss here yet (owner). The boss's encounter will stand in the middle of the last hall.
    log.append('deep: stairwell 16 x 12 m with a landing and a 4 m stair 6 m down, worn corridor 40 m (2 failing lamps), small room 12 x 12 m, dark corridor 25 m, tentacle hall 36 x 28 x 12 m with broken galleries and a rubble heap')


# Review spots: (name, x, y, floor z, yaw). The game's smoke block "tour" puts the player on each one and takes a
# screenshot through the real game camera (gb.Smoke tour). On a spot whose name has "castwall" it also casts the ice
# wall at full strength and checks that it stands on that floor; on "breakwall" it hits the nearest breakable four
# times and checks that it broke; on "gatetest" it hands the player a power cell by a socket and checks the socket took
# it, and on "gatecheck" that the energy gate went out.
TOUR = [('01_arrival', -500, 60, 0, 0), ('02_arrival_back', -150, 0, 0, 180), ('03_corridor', 300, -50, 0, 0),
        ('04_corridor_alcove', 1500, 0, 0, 90), ('05_enter_bay', 2050, -200, 0, 12), ('06_west_stair', 2000, -100, 0, 35),
        ('07_catwalk', 2700, 200, 300, -5), ('08_bridge', 4250, -100, 300, -90), ('09_lookout', 4250, -600, 300, 150),
        ('10_bay_floor_east', 4600, -150, 0, 180), ('11_branch', 3380, 2200, 0, -90), ('12_under_catwalk', 3380, 500, 0, -90),
        ('13_alcove_bridge', 1550, 620, 0, 90), ('14_air_plant', 1350, 1400, 0, 20), ('15_air_plant_gallery', 1550, 1150, 410, 90),
        ('16_store_door', 650, -350, 0, -90), ('17_store', 650, -1000, 0, -90),
        ('18_lab_gate', 1550, 2350, 410, 90), ('19_lab', 1500, 3000, 410, 90), ('19b_lab_wall', 1750, 3950, 410, 0), ('19c_lab_dais', 1550, 3650, 410, 90),
        ('19d_lab_back', 1550, 4300, 460, -90), ('19e_lab_steps', 1380, 3700, 410, 60), ('19f_lab_gate', 1550, 4550, 460, 90), ('19g_lab_gate_near', 1550, 4850, 460, 90),
        ('19h_gatetest_a', 1725, 4320, 460, 90), ('19i_gatetest_b', 1375, 4320, 460, 90), ('19j_gatecheck', 1550, 4600, 460, 90),
        ('40_valk_door', 1550, 5150, 460, 90), ('41_valk_room', 1300, 5800, 460, 70), ('42_valk_tube', 1550, 6500, 510, 90), ('43_valk_back', 1550, 6900, 510, -90),
        ('21_north_corridor', 3380, 2750, 0, 90), ('22_from_north_back', 3380, 3000, 0, -90),
        ('22b_long_door', 3380, 3150, 0, 90), ('23_long_start', 3380, 3600, 0, 90), ('23b_long_exit', 3380, 3950, 0, -90), ('24_long_mid', 3380, 5300, 0, 90), ('24b_long_screen', 3380, 5600, 0, 0), ('24c_long_castwall', 3380, 4600, 0, 90), ('25_long_end', 3380, 7000, 0, 90), ('25b_long_back', 3380, 7300, 0, -90), ('25c_long_far_door', 3380, 7250, 0, 90), ('25d_long_crates', 3420, 5800, 0, 130),
        ('28_boss_entry', 3380, 7720, 0, 90), ('29_boss_centre', 3380, 9500, 0, 90), ('29b_boss_back', 3380, 10900, 0, -90), ('29c_boss_floor_west', 3300, 8800, 0, 160),
        ('30_boss_west_gallery', 2180, 9100, 400, 0), ('31_boss_east_gallery', 4580, 10100, 400, 140), ('32_boss_bridge', 3380, 7800, 400, 90),
        ('33_boss_corner_pile', 3500, 10400, 0, 50),
        ('26_east_stair', 5200, -100, 0, 150), ('27_alcove_pit', 1250, 700, -290, 20),
        ('49_bay_shutter', 4800, 0, 0, 0), ('50_bay_breakwall', 4800, 0, 0, 0), ('51_well_top', 5450, 0, 0, 0), ('52_well_bottom', 6750, 300, -600, 190), ('52b_well_ports', 6350, -150, -600, 80), ('53_worn_start', 7150, 0, -600, 0),
        ('54_worn_mid', 8900, 0, -600, 0), ('55_worn_end', 10700, 0, -600, 0), ('56_nest', 11250, 0, -600, 0), ('57_nest_back', 12100, -350, -600, 150),
        ('58_dark', 12500, 0, -600, 0), ('59_dark_end', 14450, 0, -600, 0), ('60_pit_entry', 14950, 0, -600, 0), ('61_pit_middle', 16300, -300, -600, 30),
        ('62_pit_back', 18000, 0, -600, 180), ('63_pit_north_gallery', 17000, 1200, -200, -120), ('64_pit_south_end', 16500, -1000, -600, 200)]


TOUR_ONLY = ()        # while reviewing one part: beginnings of spot names; empty = every spot (always, before a commit)


async def stage_tour():
    s = 'tour'
    for name, x, y, z, yaw in TOUR:
        if TOUR_ONLY and not name.startswith(TOUR_ONLY):
            continue
        a = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z), rot(yaw=yaw))
        a.tags = [unreal.Name(TAG), unreal.Name('stage_' + s), unreal.Name('GBTour'), unreal.Name('tour_' + name)]
        a.set_folder_path(unreal.Name('Base/tour'))
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(1900, 2700, 300), rot())
    nav.set_actor_scale3d(unreal.Vector(44.0, 50.0, 8.0))    # the volume's brush is a 2 m cube
    tag(nav, s, 'Gameplay')
    log.append('tour: %d review spots, nav bounds' % len(TOUR))


# Walks checked for holes: (name, [(x, y, z), ...]). A point every 20 cm must have floor within 45 cm of the line.
WALKS = [('arrival to corridor', [(-500, 60, 0), (300, 0, 0)]), ('corridor to store room', [(650, -300, 0), (650, -950, 0)]),
         ('corridor over the bridge into the air plant', [(1550, 300, 0), (1550, 1180, 0)]),
         ('corridor to bay', [(1800, 0, 0), (2500, -150, 0)]), ('bay to branch to freight door and on', [(3380, 100, 0), (3380, 3350, 0)]),
         ('gallery into the lab room', [(1550, 1960, 410), (1550, 3600, 410)]),
         ('lab floor up the steps onto the dais and to the energy gate', [(1550, 3700, 410), (1550, 3797, 410), (1550, 3904, 460), (1550, 4900, 460)]),
         ('long corridor through its far door into the sub-boss hall', [(3380, 7300, 0), (3380, 8300, 0)]),
         ('bay through the shutter and down the stairs', [(5000, -100, 0), (5690, -100, 0), (6610, -100, -600), (6890, 0, -600)]),
         ('deep wing: worn corridor, small room, dark corridor, the last hall', [(6890, 0, -600), (11100, 0, -600), (12300, 0, -600), (14800, 0, -600), (16000, 0, -600)]),
         ('second freight door into the long corridor', [(3380, 3350, 0), (3380, 7450, 0)]), ('bay floor to west stair', [(2250, 200, 0), (2360, 200, 0)])]


def check_walks():
    world = ues.get_editor_world()
    bad = []
    doors = [a for a in eas.get_all_level_actors() if a.get_class().get_name() in ('GBDoor', 'GBBreakable')]    # they open, or break, in the game
    for name, pts in WALKS:
        for (x0, y0, z0), (x1, y1, z1) in zip(pts[:-1], pts[1:]):
            # Chest height, straight along the walk: a wall across a doorway shows up here (the muster hall's did not
            # show in the floor check, and the owner found its door walled up).
            wall = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x0, y0, z0 + 110), unreal.Vector(x1, y1, z1 + 110),
                                                          unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, doors, unreal.DrawDebugTrace.NONE, True)
            if wall:
                t = wall.to_tuple()
                bad.append('%s: BLOCKED at (%d, %d) by %s' % (name, t[4].x, t[4].y, t[9].get_actor_label() if t[9] else '?'))
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 20.0))
            run = []
            for i in range(n + 1):
                t = i / float(n)
                x, y, z = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t
                hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 60), unreal.Vector(x, y, z - 45),
                                                             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, doors, unreal.DrawDebugTrace.NONE, True)
                if not hit or abs(hit.to_tuple()[4].z - z) > 30:      # no floor, or a floor that is not where the walk says
                    run.append((int(x), int(y)))
            if run:
                bad.append('%s: no floor from %s to %s (%d points)' % (name, run[0], run[-1], len(run)))
    log.append('walks: ' + ('clear on %d walks' % len(WALKS) if not bad else 'PROBLEMS -> ' + ' | '.join(bad)))


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
        if any(k in name for k in ('Decal', 'Top_Cables', 'Pipe_02', 'Chair', 'Top_Chain', 'Volume', 'cable_cover', 'lamp_', 'ShipCorridor_Details', 'MStair01-Steps', 'MFloor02-Steps', 'Mollusk', 'WinMetal', 'SM_Lamp0', 'RockSet')):
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
       ('lab', stage_lab), ('valk', stage_valk), ('long', stage_long), ('boss', stage_boss), ('deep', stage_deep), ('tour', stage_tour)]

# ---- run ----
if not eal.does_asset_exist(MAP):
    les.new_level(MAP, False)
    les.save_current_level()
elif ues.get_editor_world().get_path_name().split('.')[0] != MAP:
    les.load_level(MAP)
await asyncio.sleep(0)
ensure_steel()
if not STAGES or 'lab' in STAGES:
    clear('guard')          # the muster hall stood where the lab room is (2026-11-13 to 2026-11-14)
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
