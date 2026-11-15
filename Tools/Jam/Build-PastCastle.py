# Build-PastCastle.py - builds the castle of the past, slice 1 (Design/Levels/PAST_CASTLE_PLAN.md) in the Unreal Editor.
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/Build-PastCastle.py
#
# How it builds (the future base's method):
#  - "Sections" are levels of the castle pack as its author assembled them (Design/Levels/kits/castle/*.json, written by
#    castle_read.py): the keep's five parts, the gatehouse, a wall tower, the wall stair. They are placed again piece by
#    piece with the same relative positions and materials.
#  - Our own level design (wards, walls, the camp, the crypt) is placed below, each thing with a reason in the plan.
#  - Kit pieces block as they look (complex collision as simple, both sides); small props do not block.
# Frame: +X is the way the player advances (gate to keep), the keep's axis is Y = 0, the lower ward is Z = 0.
# Idempotent: every actor carries the tag GBCastle and a stage tag; a run deletes and rebuilds the stages named in
# STAGES (all of them when the list is empty).
import json
import math
import os

MAP = '/Game/Glyphbound/Maps/PastCastle'
FUTURE_MAP = '/Game/Glyphbound/Maps/FutureBase'
TAG = 'GBCastle'
STAGES = []          # e.g. ['wards'] to rebuild one stage only
_cfg = os.path.join(unreal.Paths.project_saved_dir(), 'castle_build.json')     # optional: {"stages": ["wards"]}
CFG = json.load(open(_cfg)) if os.path.exists(_cfg) else {}
STAGES = CFG.get('stages', STAGES)
KIT_DIR = 'C:/GameJam/Design/Levels/kits/castle/'
CT = '/Game/CastleTown/'
CK = CT + 'Static_Mesh/Castle_Kit/'
PROPS = CT + 'Static_Mesh/Props/'
MI = CT + 'Materials/Instances/'

# The keep's own origin in our frame: its entrance axis (its y = -1400) lies on Y = 0, its walking floor (its z = 90)
# is 30 cm above the upper ward.
KEEP = (5700.0, 1400.0, 150.0)
WARD_Y = 2600.0          # half the wards' width, wall face to wall face
TERRACE_X = 2800.0       # the edge of the upper ward
TERRACE_Z = 210.0        # the wide stair's rise (SM_Stairs_4M)
COBBLE_Z = -3.0            # the cobble piece's stone tops (measured: 90% of them under +5 cm) come up to the ground plane
STONE_FLOOR = MI + 'MI_StoneFloor_Tile_A'   # the author's terraces are timber decks; a ward is paved
FLOOR_Z = KEEP[2] + 86.0   # the keep's walking floor (measured with a trace in the Main Hall: 236)
# The crypt (plan 6.5, 6.6). Its stair starts in the Main Hall's rear corner on the King's Hall side, where four of the
# hall's 4 m floor tiles are left out: X 9290..10090, Y -1000..-200, beside the grand stair's first flight. The hall's
# back wall is at X 10090 and its foot is 3 m above the stair there; behind it lies the grand stair's own well, open
# above, and the keep's outer wall at X 10490 (its foot 3.6 m above the stair).
PIT = (9290.0, 10090.0, -1000.0, -200.0)
CRYPT_Z = FLOOR_Z - 1200.0            # four flights of 3 m
CRYPT_AREA = (9290.0, 13100.0, -7900.0, -100.0)   # no land over it: the stair and the vaults pass where the land would be
WALK_Z = 1200.0          # the wall walk: 12 m above the curtain wall's base (measured from the author's own stair and towers)

# The castle's look (owner, 2026-11-14: everything is replaced by real pixel art, which must also be light). "pack": the
# castle pack's sculpted pieces, as built until now. "pixel": each pack mesh is replaced by its pixel twin (same pivot and
# size, flat faces with drawn tiles: Tools/Jam/castle_twins.js), and our own boxes wear the drawn tiles. pixel_stages
# limits the pixel look to some stages (the first zone was shown to the owner on its own).
LOOK = CFG.get('look', 'pack')
PIXEL_STAGES = CFG.get('pixel_stages')
PX = '/Game/Glyphbound/Castle/Pixel/'
_twins = {}


# The author's loose dressed stones, bricks and slabs, dotted over the keep's faces; the builder's own thresholds use
# SM_BlockC, which stays.
PENDING_TORCHES = []
TORCH_SPACING = 900.0      # cm: in the great halls the author hung a torch every few metres; one lit torch per 9 m
FLAME = dict(size=0.19, offset=(0.0, 9.0, 68.0), light=0.4)   # the torch flame clip (TorchFire, 3 m tall as baked) at ~58 cm; its light at 60%
PIXEL_DROP = ('SM_BlockA', 'SM_BlockB', 'SM_BrickA', 'SM_SlabA', 'SM_SlabB')

def pixel(stage):
    return LOOK == 'pixel' and (PIXEL_STAGES is None or stage in PIXEL_STAGES)


def twin(path):
    """The pixel twin of a pack mesh, or None while it has none."""
    if path not in _twins:
        name = path.split('/')[-1].split('.')[0]
        t = PX + 'Meshes/SM_Px_' + (name[3:] if name.startswith('SM_') else name)
        _twins[path] = t if eal.does_asset_exist(t) else None
        if not _twins[path]:
            log.append('NO TWIN for ' + path)
    return _twins[path]


def tile_kind(mat):
    """The tile a pack material becomes (the same reading as castle_twins.js)."""
    n = mat.split('/')[-1].lower()
    for kind, keys in (('wood', ('wood', 'plank', 'varnish', 'herring')), ('iron', ('metal', 'iron')), ('cobble', ('cobble',)),
                       ('cloth', ('cloth', 'sack')), ('earth', ('ground', 'dirt')), ('dressed', ('brickkit', 'slab', 'stairs', 'floor', 'tile')),
                       ('dark', ('stonefence', 'rock', 'firepit'))):
        if any(k in n for k in keys):
            return kind
    return 'wall'


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eal = unreal.EditorAssetLibrary
log = []
_assets = {}
_kits = {}

# Things that never block: what is small, what hangs overhead, what only hides a seam.
NO_BLOCK = ('Candle', 'Cup', 'Plate', 'Book', 'Bottle', 'Goblet', 'ChainLink', 'Chain_', 'Rug', 'SeamHider', 'Moss', 'Chandelier',
            'Torch', 'Lantern', 'Flag', 'Rope', 'Mug', 'Bowl', 'Jar', 'Flagon', 'Pillow', 'Sword', 'Axe', 'Hammer', 'Shield',
            'Horse_Shoe', 'Fruit', 'Kettle', 'Pot', 'Cobble', 'Banner')


# MI_Stab_Algae (two trim slots of the gatehouse's middle arch and front top): "Failed to compile Material Instance ...
# Error on property Normal" in its layer functions; in a package it loads with an invalid shader map.
BAD_MATS = {'MI_Stab_Algae': MI + 'MI_Slabs_03'}
_bad_slots = {}


def asset(path):
    if path not in _assets:
        _assets[path] = unreal.load_asset(path.split('.')[0]) if path else None
    return _assets[path]


def tag(actor, stage, folder):
    actor.tags = [unreal.Name(TAG), unreal.Name('stage_' + stage)]
    actor.set_folder_path(unreal.Name('Castle/%s/%s' % (stage, folder)))


def clear(stage):
    n = 0
    for a in eas.get_all_level_actors():
        if a.actor_has_tag(TAG) and a.actor_has_tag('stage_' + stage):
            eas.destroy_actor(a)
            n += 1
    return n


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def mesh(stage, folder, path, loc, r=None, scale=(1, 1, 1), mats=None, shadow=True, collide=None):
    whole = None
    if pixel(stage) and path.startswith(CT):
        if 'SeamHider' in path:
            return None             # it hides the joint between two sculpted modules; the flat twins meet on the same planes
        if path.split('/')[-1] == 'SM_Torch_A':
            PENDING_TORCHES.append((stage, folder, loc, r or rot()))
            return None             # a lit torch actor (flame and light) is placed instead; see place_torches
        if path.split('/')[-1] in PIXEL_DROP:
            return None             # the author's loose stones and slabs on the towers' faces: in plain blocks they read as faults
        tw = twin(path)
        if tw:
            # an override that gives the whole piece one material (the terraces paved in stone instead of the author's
            # timber) gives the twin that tile on every slot; the author's own per-slot choices are left to the twin
            kinds = set(tile_kind(m) for m in (mats or []) if m)
            path, mats = tw, None
            whole = kinds.pop() if len(kinds) == 1 else None
    sm = asset(path)
    if not sm:
        log.append('MISSING mesh ' + path)
        return None
    a = eas.spawn_actor_from_object(sm, unreal.Vector(*loc), r or rot())
    a.set_actor_scale3d(unreal.Vector(*scale))
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.STATIC)
    for i, m in enumerate(mats or []):
        if m:
            c.set_material(i, asset(m))
    if path.startswith(PX):
        pass
    elif path not in _bad_slots:
        # A pack material that does not compile in this project (the cook says so and uses the default material) is
        # replaced, slot by slot, by a plain one of the same pack.
        _bad_slots[path] = [(i, BAD_MATS[sl.material_interface.get_name()]) for i, sl in enumerate(sm.static_materials)
                            if sl.material_interface and sl.material_interface.get_name() in BAD_MATS]
    for i, m in _bad_slots.get(path, []):
        c.set_material(i, asset(m))
    if collide is None:
        name = path.split('/')[-1]
        collide = not any(k in name for k in NO_BLOCK)
    c.set_collision_profile_name('BlockAll' if collide else 'NoCollision')
    c.set_editor_property('cast_shadow', shadow)
    if path.startswith(PX) and whole:
        for i in range(len(sm.static_materials)):
            c.set_material(i, asset(PX + 'MI_Px_' + whole))
    if path.startswith(PX) and CFG.get('compare_wall') and loc[1] > 0:
        # two sizes of the wall tile side by side for the owner: the finer one right of the road
        i = c.get_material_index('wall')
        if i >= 0:
            c.set_material(i, asset(PX + 'MI_Px_wall_fine'))
    tag(a, stage, folder)
    return a


def box(stage, folder, x, y, z, mat=None, visible=True, collide=True):
    """A plain box from (x0, x1), (y0, y1), (z0, z1)."""
    a = eas.spawn_actor_from_object(asset('/Engine/BasicShapes/Cube'), unreal.Vector((x[0] + x[1]) / 2, (y[0] + y[1]) / 2, (z[0] + z[1]) / 2), rot())
    a.set_actor_scale3d(unreal.Vector(abs(x[1] - x[0]) / 100.0, abs(y[1] - y[0]) / 100.0, abs(z[1] - z[0]) / 100.0))
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.STATIC)
    if mat and pixel(stage) and mat.startswith(CT):
        mat = PX + 'MIW_Px_' + tile_kind(mat)          # the drawn tile, laid on the world (a box has no UVs of its own)
    if mat:
        c.set_material(0, asset(mat))
    c.set_collision_profile_name('BlockAll' if collide else 'NoCollision')
    if not visible:
        a.set_actor_hidden_in_game(True)
        c.set_visibility(False)
    tag(a, stage, folder)
    return a


def point_light(stage, folder, loc, color, candelas, radius, shadow=False):
    a = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc), rot())
    c = a.light_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
    c.set_editor_property('intensity', candelas)
    c.set_editor_property('light_color', unreal.Color(r=int(color[0]), g=int(color[1]), b=int(color[2]), a=255))
    c.set_editor_property('attenuation_radius', radius)
    c.set_editor_property('cast_shadows', shadow)
    c.set_editor_property('source_radius', 12.0)
    tag(a, stage, folder)
    return a


def kit(name):
    if name not in _kits:
        _kits[name] = json.load(open(KIT_DIR + name + '.json'))['items']
    return _kits[name]


def torch(stage, folder, loc, r):
    """A wall torch (AGBTorch): the twin of the pack's torch as its holder, a baked Niagara Fluids flame
    (Content/Data/Vfx/TorchFire, made by the GBPixelVfx plugin) with its own light, which follows the flame."""
    a = eas.spawn_actor_from_class(unreal.GBTorch, unreal.Vector(*loc), r)
    holder = a.get_editor_property('holder')
    holder.set_static_mesh(asset(twin(CT + 'Static_Mesh/Props/SM_Torch_A')))
    # the light sits right over the stick: the stick's own shadow ran down the wall as a long diagonal wedge (owner)
    holder.set_editor_property('cast_shadow', False)
    a.set_editor_property('flame_size', FLAME['size'])
    a.set_editor_property('light_scale', FLAME['light'])
    # no shadows from a torch's light: it hangs a hand's breadth from the wall, and every step of the stone near it
    # threw a long grazing shadow across the wall, a diagonal line under the torch (owner: "esta raro eso")
    a.set_editor_property('light_casts_shadows', False)
    a.get_editor_property('flame').set_editor_property('relative_location', unreal.Vector(*FLAME['offset']))
    tag(a, stage, folder)
    return a


def place_torches(stage, spacing=TORCH_SPACING):
    """The author's torches of this stage, thinned: a torch is lit only if no lit torch is within spacing (the great
    halls had one every few metres; a lone torch on a stair or in a passage always stays)."""
    kept = []
    mine = [t for t in PENDING_TORCHES if t[0] == stage]
    for st, folder, loc, r in mine:
        if all(math.dist(loc, k[2]) >= spacing for k in kept):
            kept.append((st, folder, loc, r))
    for st, folder, loc, r in kept:
        torch(st, 'Torches', loc, r)
    PENDING_TORCHES[:] = [t for t in PENDING_TORCHES if t[0] != stage]
    return len(mine), len(kept)


def torch_on_wall(stage, folder, x, y, z_hint, dx, dy, height=215.0):
    """A torch on the wall found from (x, y) looking along (dx, dy): the floor under the point is found first, the
    torch hangs `height` above it, its head leaning out of the wall."""
    world = ues.get_editor_world()
    down = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z_hint + 150), unreal.Vector(x, y, z_hint - 600),
                                                  unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if not down:
        log.append('torch: no floor at (%d, %d)' % (x, y))
        return None
    z = down.to_tuple()[4].z + height
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z), unreal.Vector(x + dx * 900, y + dy * 900, z),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        log.append('torch: no wall from (%d, %d) along (%d, %d)' % (x, y, dx, dy))
        return None
    t = hit.to_tuple()
    p, nrm = t[4], t[6]
    # the torch leans out along its own +Y: turn it so +Y is the wall's normal
    yaw = math.degrees(math.atan2(nrm.y, nrm.x)) - 90.0
    return torch(stage, folder, (p.x + nrm.x * 3.0, p.y + nrm.y * 3.0, z - 40.0), rot(yaw=yaw))


def place_level(stage, folder, name, to, yaw=0.0, drop=None, keep=None, inside=None, skip=None):
    """Places the pieces of one of the author's levels again: its origin lands on `to`, turned `yaw` degrees.
    drop / keep filter by a part of the mesh name; inside = (x0, x1, y0, y1) in the level's own coordinates."""
    rad = math.radians(yaw)
    cs, sn = math.cos(rad), math.sin(rad)
    turn = rot(yaw=yaw)
    n = 0
    torches = []
    for it in kit(name):
        if it['kind'] != 'mesh':
            continue
        nm = it['mesh'].split('/')[-1]
        if it['mesh'].startswith('/Engine/') and [abs(v) for v in it['scale']] == [1, 1, 1]:
            continue                        # a leftover unit cube of the author's
        if drop and any(k in nm for k in drop):
            continue
        if keep and not any(k in nm for k in keep):
            continue
        lx, ly, lz = it['loc']
        if inside and not (inside[0] <= lx < inside[1] and inside[2] <= ly < inside[3]):
            continue
        loc = (to[0] + lx * cs - ly * sn, to[1] + lx * sn + ly * cs, to[2] + lz)
        if skip and skip(nm, loc, it['rot'][1] + yaw):
            continue
        r = unreal.MathLibrary.compose_rotators(rot(*it['rot']), turn)
        if mesh(stage, folder, it['mesh'], loc, r, it['scale'], it.get('mats')):
            n += 1
        if nm == 'SM_Torch_A':
            torches.append(loc)
    return n, torches


# ---------------------------------------------------------------------------------------------------------------
async def stage_world():
    s = 'world'
    sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 6000), rot(pitch=-38, yaw=-62))
    c = sun.light_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity', 6.0)
    c.set_editor_property('light_color', unreal.Color(r=255, g=232, b=205, a=255))
    c.set_editor_property('atmosphere_sun_light', True)
    tag(sun, s, 'Light')
    for cls in (unreal.SkyAtmosphere, unreal.ExponentialHeightFog):
        tag(eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 3000), rot()), s, 'Light')
    sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 6000), rot())
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('real_time_capture', True)
    sky.light_component.set_editor_property('intensity', 1.0)
    tag(sky, s, 'Light')
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), rot())
    ppv.set_editor_property('unbound', True)
    st = ppv.get_editor_property('settings')
    # Daylight outside, firelight inside: the eye adapts, within limits, so a hall is dim after the ward and never black.
    for name, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM), ('auto_exposure_bias', 0.0),
                        ('auto_exposure_min_brightness', 0.12), ('auto_exposure_max_brightness', 3.0),
                        ('auto_exposure_speed_up', 2.0), ('auto_exposure_speed_down', 1.5), ('bloom_intensity', 0.4)):
        st.set_editor_property('override_' + name, True)
        st.set_editor_property(name, value)
    ppv.set_editor_property('settings', st)
    tag(ppv, s, 'Light')
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-2300, 0, 110), rot(yaw=0))
    tag(ps, s, 'Gameplay')
    ues.get_editor_world().get_world_settings().set_editor_property('kill_z', -4000.0)
    log.append('world: sun, sky, fog, exposure, player start')


async def stage_ground():
    s = 'ground'
    # The land outside, the lower ward's earth, and the body of the upper ward's terrace.
    cx0, cx1, cy0, cy1 = CRYPT_AREA
    for i in range(-5, 9):
        for j in range(-5, 5):
            x0, y0 = i * 2000.0, j * 2000.0
            if x0 < cx1 and x0 + 2000.0 > cx0 and y0 < cy1 and y0 + 2000.0 > cy0:
                continue                    # over the crypt (under the keep, and behind it under the rock)
            box(s, 'Land', (x0, x0 + 2000.0), (y0, y0 + 2000.0), (-120, -20), MI + 'MI_Ground_Rock_A', collide=False)
    for x, y in (((-10000, cx0), (-10000, 10000)), ((cx1, 18000), (-10000, 10000)), ((cx0, cx1), (-10000, cy0)), ((cx0, cx1), (cy1, 10000))):
        box(s, 'Collision', x, y, (-140, -20), visible=False)
    box(s, 'Collision', (-4500, TERRACE_X), (-WARD_Y - 300, WARD_Y + 300), (-60, 0), visible=False)
    box(s, 'Land', (TERRACE_X + 20, 7600), (-WARD_Y - 200, WARD_Y + 200), (-100, TERRACE_Z - 55), MI + 'MI_BrickStoneWall_nonDisp')
    # Cobbles: the road to the gate and the whole lower ward (the pack's own 8 m cobble pieces, set just into the ground).
    n = 0
    for i in range(-4, 4):
        for j in range(-3, 4):
            x, y = i * 800.0 - 300.0, j * 800.0
            if x < -1100 and abs(y) > 500:
                continue                    # outside the gate only the road is cobbled
            mesh(s, 'Cobbles', CT + 'Static_Mesh/Cobblestone_Road/SM_Cobbles_8M', (x, y, COBBLE_Z), collide=False)
            n += 1
    # The upper ward and the two side yards: the stone floor tiles the author uses on every terrace of the keep.
    t = 0
    for i in range(0, 12):
        for j in range(-7, 7):
            x, y = TERRACE_X + i * 400.0, j * 400.0
            if abs(y + 200) > WARD_Y:
                continue
            mesh(s, 'Terrace', CK + 'Buildings/A/SM_CastleTower_Floor_A_01', (x, y, TERRACE_Z - 50.0), mats=[STONE_FLOOR, STONE_FLOOR], collide=True)
            t += 1
    log.append('ground: %d cobble pieces, %d terrace tiles' % (n, t))


def over_pit(name, loc, yaw):
    """The Main Hall's floor tiles (4 x 4 m from their pivot, turned as the author turned them) whose middle lies over
    the crypt stair."""
    if not any(k in name for k in ('Herringbone', 'Planks')):
        return False
    r = math.radians(yaw)
    mx, my = loc[0] + 200.0 * (math.cos(r) - math.sin(r)), loc[1] + 200.0 * (math.sin(r) + math.cos(r))
    return PIT[0] < mx < PIT[1] and PIT[2] < my < PIT[3] and abs(loc[2] - KEEP[2]) < 60


async def stage_keep():
    s = 'keep'
    kx, ky, kz = KEEP
    total = 0
    torches = []
    for name, ox, oy, oz, yaw in (('LI_Full_Castle', 0, 0, 0, 0), ('LI_Front_Hall', 0, 0, 0, 0), ('SM_Main_Hall', 1890, 80, 20, 0),
                                  ('LI_Kings_Hall', 2390, -6100, -500, 0), ('LI_Side_Hall', 4390, 3203, -500, 180),
                                  ('LI_Top_Level', 1990, -2400, 2400, 90)):
        n, tl = place_level(s, name, name, (kx + ox, ky + oy, kz + oz), yaw, skip=over_pit)
        total += n
        torches += tl
        await asyncio.sleep(0)
    if pixel(s):
        # owner, 2026-11-14: torches with a fluid-simulated flame; fewer lights in the great halls
        hung, lit = place_torches(s)
        log.append("keep: %d pieces from six of the author levels; of his %d torches %d are lit (one per %d m)" % (total, hung, lit, TORCH_SPACING / 100))
        return
    # A warm light at every torch the author hung (his torches were blueprints with a flame; ours are the mesh and a light).
    for x, y, z in torches:
        point_light(s, 'Torches', (x, y, z + 70.0), (255, 196, 140), 34.0, 1300.0)
    log.append('keep: %d pieces from six of the author\'s levels, %d torch lights' % (total, len(torches)))


async def stage_gate():
    s = 'gate'
    # The author's gatehouse, whole. Its passage runs along its own Y with the front arch at +Y; turned 90 degrees the
    # front faces -X (outside) and the passage's middle lands on Y = 0.
    n, _ = place_level(s, 'Gatehouse', 'LI_Front_Gate', (-375.0, -450.0, 0.0), 90.0, drop=GATE_DROP)
    log.append('gate: %d pieces' % n)


GATE_DROP = ()


WALL = CK + 'CurtainWall/SM_Curtain_Wall_01'      # 4 m long from its pivot along its own +X, 4.3 m thick, 13.6 m high
WALL_STAIR = CK + 'CurtainWall/SM_Curtain_Wall_02'  # the same wall with an opening in one side's teeth, for a stair
WALL_Y = WARD_Y + 216.0                           # the side walls' middle line
FRONT_X = -525.0                                  # the front wall's middle line (the gatehouse's too)
SIDE_X0 = FRONT_X + 345.0 - 62.0                  # where a side wall starts: the corner tower's face, 62 cm in
STAIR_PIECE = 15                                  # which piece of the King's side wall is the stair piece (X 5758..6158)
# The wall stair's own origin along the wall: its landing (its own x -400..-150) lies under the stair piece's opening,
# which, with the piece turned to face inside, is 100 to 350 cm back from the piece's far end.
STAIR_X = SIDE_X0 + 400.0 * (STAIR_PIECE + 1) - 225.0 + 275.0
END_TOWER_X = SIDE_X0 + 7200.0 + 345.0            # the keep-end towers: their far face (7646) meets the wings' wall
WALL_OUT = -1.0                                   # the piece's crenels are on its own -Y side: that side faces outward (seen in renders)


def wall_run(s, x0, y0, x1, y1, out, stair=None):
    """Curtain wall pieces from (x0, y0) to (x1, y1), a straight run along X or along Y. out = (ox, oy): the outside.
    stair = the index of the piece (counted from the low end of the run, along X only) that is the pack's stair piece:
    the same wall with no teeth on one side over 2.5 m (its own X 100..350, on its own -Y side) and a sill there at
    1214, the height the author's wall stair arrives at. It is turned so that its open side faces the inside."""
    along_x = abs(x1 - x0) > abs(y1 - y0)
    length = abs(x1 - x0) if along_x else abs(y1 - y0)
    n = int(round(length / 400.0))
    # The piece's +X is its length. Its +Y must point to the outside (times WALL_OUT).
    if along_x:
        yaw = 0.0 if out[1] * WALL_OUT > 0 else 180.0
    else:
        yaw = 90.0 if out[0] * WALL_OUT < 0 else -90.0
    rad = math.radians(yaw)
    dx, dy = math.cos(rad), math.sin(rad)             # the way the piece runs from its pivot
    for i in range(n):
        t = i * 400.0
        if along_x:
            px = (min(x0, x1) + t) if dx > 0 else (max(x0, x1) - t)
            py = y0
        else:
            px = x0
            py = (min(y0, y1) + t) if dy > 0 else (max(y0, y1) - t)
        if stair is not None and along_x and i == (stair if dx > 0 else n - 1 - stair):
            # open side (its -Y) to the inside: turned half round against a piece whose -Y faces out, and set at the
            # piece's other end, since a turned piece runs the other way from its pivot
            inside_is_minus_y = (out[1] > 0) == (yaw == 0.0)
            if inside_is_minus_y:
                mesh(s, 'Wall', WALL_STAIR, (px, py, 0.0), rot(yaw=yaw))
            else:
                mesh(s, 'Wall', WALL_STAIR, (px + 400.0 * dx, py, 0.0), rot(yaw=yaw + 180.0))
            continue
        mesh(s, 'Wall', WALL, (px, py, 0.0), rot(yaw=yaw))
    return n


STEP_STONE = MI + 'BrickKit/MI_BrickKit_03'            # the stone of the kit's own stairs
BLOCK = CT + 'Static_Mesh/Brick_Kit/SM_BlockC'       # a dressed stone block, 57 x 42 x 54 cm, from (-4, 0, -2) of its pivot


def stone_block(s, x, y, z, sx, sy, sz):
    """One of the kit's stone blocks filling the box that starts at (x, y, z) and measures sx, sy, sz."""
    kx, ky, kz = sx / 57.0, sy / 42.0, sz / 54.0
    return mesh(s, 'Mount', BLOCK, (x + 4.0 * kx, y, z + 2.0 * kz), scale=(kx, ky, kz), mats=[STEP_STONE], collide=True)


async def stage_walls():
    s = 'walls'
    n = 0
    # A wall never runs through a tower (owner, 2026-11-14: "the towers of the wall's walks are blocked by the window
    # with a piece of wall, both of them"): each run ends at a tower's face. The tower is 6.9 x 7.7 m outside and only
    # 4 x 4 m inside, with a door in each face across the wall's line and a window in the other two.
    # The two side walls: 18 pieces from the corner tower's face (62 cm into its wall's thickness) to the keep-end
    # tower's face.
    n += wall_run(s, SIDE_X0, -WALL_Y, SIDE_X0 + 7200.0, -WALL_Y, (0, -1), stair=STAIR_PIECE)
    n += wall_run(s, SIDE_X0, WALL_Y, SIDE_X0 + 7200.0, WALL_Y, (0, 1))
    # The front wall, either side of the gatehouse: 3 pieces from inside its round towers to the corner tower's face.
    n += wall_run(s, FRONT_X, -2440.0, FRONT_X, -1240.0, (-1, 0))
    n += wall_run(s, FRONT_X, 1240.0, FRONT_X, 2440.0, (-1, 0))
    # The author's wall tower at each front corner and at the keep's end of each side wall. The keep-end towers stand
    # against the wings' front wall, not in it (they stood 3.5 m inside: a piece of the King's Hall's wall, window and
    # all, filled the tower's room). A tower's floor is at its own height 1000 (measured with a trace: the architect
    # lowered the towers by 50 cm on a guess about the floor tile's thickness, and the walk check caught it).
    t = 0
    for x, y in ((FRONT_X, -WALL_Y), (FRONT_X, WALL_Y), (END_TOWER_X, -WALL_Y), (END_TOWER_X, WALL_Y)):
        k, _ = place_level(s, 'Tower', 'LI_CurtainWall_Tower_01', (x, y, WALK_Z + 3.0 - 1000.0), 0.0)
        t += k
        # A threshold in each of its two doorways: the tower's wall is 1.45 m thick there and has no floor of its own
        # (the walk check found the hole between the wall walk's end and the tower's room).
        for dx in (-350.0, 195.0):
            stone_block(s, x + dx - 20.0, y - 70.0, WALK_Z - 17.0, 195.0, 140.0, 20.0)   # 20 cm into the wall each side: no seam
    # The author's wall stair, in the side yard on the King's Hall side, its foot on the upper ward.
    k, _ = place_level(s, 'WallStair', 'LI_CurtainWall_Stairs_01', (STAIR_X, -WARD_Y - 6.0, TERRACE_Z), 0.0)
    # Its landing meets the stair piece's sill (1214), 11 cm above the wall walk: no teeth in the way.
    log.append('walls: %d wall pieces (one of them the stair piece), %d tower pieces, %d wall stair pieces' % (n, t, k))


RET = CK + 'Buildings/Hall_1/SM_Wall_1x5'          # 4 m long along its own +X, 1.25 m thick, 5 m high
BREACH = 11                                       # the retaining wall's module that has fallen (Y 1800..2200)
STAIR_MODS = (5, 6)                               # the wide stair stands in front of these two (Y -600..200)


async def stage_wards():
    s = 'wards'
    # The retaining wall of the upper ward: thirteen 4 m modules across the ward, sunk so their top is the terrace.
    for i in range(13):
        if i == BREACH or i in STAIR_MODS:
            continue                        # the breach; and the stair itself is the wall there
        y = -WARD_Y + i * 400.0
        mesh(s, 'Retaining', RET, (TERRACE_X + 5.0, y, TERRACE_Z - 500.0), rot(yaw=90))
        if True:
            mesh(s, 'Retaining', CK + 'Buildings/A/SM_CastleTower_WallTopper_A_01', (TERRACE_X - 55.0, y + 400.0, TERRACE_Z), rot(yaw=90))
    # The wide stair (8.2 m, rises 2.1 m towards its own +Y): in the middle, where the fight funnels.
    mesh(s, 'Stair', CK + 'Buildings/Courtyard/SM_Stairs_4M', (TERRACE_X - 298.0, -610.0, 0.0), rot(yaw=-90))
    # The breach: the wall has fallen outwards and the terrace's fill has run down with it. An unseen slope carries the
    # walk; rocks and a broken pillar lie on it. It is the flank of the fight.
    y0, y1 = -WARD_Y + BREACH * 400.0, -WARD_Y + (BREACH + 1) * 400.0
    slope = box(s, 'Breach', (TERRACE_X - 520.0, TERRACE_X + 60.0), (y0 + 20.0, y1 - 20.0), (-40.0, 0.0), MI + 'MI_Ground_Rock_A')
    slope.set_actor_location_and_rotation(unreal.Vector(TERRACE_X - 230.0, (y0 + y1) / 2, TERRACE_Z / 2 - 22.0), rot(pitch=20.5), False, False)
    slope.set_actor_scale3d(unreal.Vector(6.1, (y1 - y0 - 40.0) / 100.0, 0.4))
    rk = CT + 'Static_Mesh/Rock_Kit/Small/'
    for name, x, y, z, yaw, sc in (('SM_Small_Rock_E', TERRACE_X - 480, y0 + 60, -20, 30, 1.0), ('SM_Small_Rock_B', TERRACE_X - 300, y1 - 40, 20, 200, 0.9),
                                   ('SM_Small_Rock_A', TERRACE_X - 130, y0 + 30, 90, 110, 0.8), ('SM_Small_Rock_D', TERRACE_X - 560, y1 - 150, -25, 290, 1.1),
                                   ('SM_Large_Rock_C', TERRACE_X - 40, y1 - 60, 120, 75, 0.8), ('SM_Small_Rock_B', TERRACE_X - 420, y0 + 230, 10, 15, 1.0),
                                   ('SM_Small_Rock_E', TERRACE_X - 200, y0 + 180, 70, 140, 0.9), ('SM_Small_Rock_A', TERRACE_X - 330, y1 - 190, 40, 250, 1.0),
                                   ('SM_Small_Rock_D', TERRACE_X - 90, y0 + 250, 130, 60, 0.9)):
        mesh(s, 'Breach', rk + name, (x, y, z), rot(yaw=yaw), (sc, sc, sc), collide=False)
    log.append('wards: retaining wall with its breach, the wide stair')


CA = CT + 'Candle/StaticMeshes/Architecture/'
# The pack's second interior set has no materials of its own (every slot is empty). Until the past's pixel look is
# decided with the owner, it wears the castle pack's own stone.
C_WALL = [MI + 'MI_BrickStoneWall_nonDisp'] * 4
C_FLOOR = [MI + 'MI_StoneFloor_Tile_A'] * 4
C_STEP = [MI + 'MI_Stairs'] * 4
C_VAULT = [MI + 'MI_BrickStoneWall_nonDisp'] * 4
VAULT_H = 402.0          # a vault springs 4 m above the floor, so its crown meets the top of the 7.5 m walls


def turned(p, yaw, dx, dy):
    r = math.radians(yaw)
    return (p[0] + dx * math.cos(r) - dy * math.sin(r), p[1] + dx * math.sin(r) + dy * math.cos(r))


def flight(s, p, yaw, walls=True):
    """One flight of the crypt stair: 8 x 8 m, 3 m of rise. p is its low corner, yaw turns its uphill side (its own +X).
    The floor piece, a wall on each side and the stair's own vault."""
    mesh(s, 'Stair', CA + 'SM_FloorCorridorStair_01', p, rot(yaw=yaw), mats=C_STEP)
    if walls:
        mesh(s, 'Stair', CA + 'SM_CorridorWallStair_01', p, rot(yaw=yaw), mats=C_WALL)
        x, y = turned(p, yaw, 0.0, 767.0)
        mesh(s, 'Stair', CA + 'SM_CorridorWallStair_01', (x, y, p[2]), rot(yaw=yaw), mats=C_WALL)
        x, y = turned(p, yaw, 0.0, 800.0)
        mesh(s, 'Stair', CA + 'SM_CeilingVaultedStair_01', (x, y, p[2] + VAULT_H), rot(yaw=yaw), mats=C_VAULT)


def bay(s, x, y, z, west=True, east=True, north=False, south=False):
    """One 8 x 8 m bay of floor and vault whose corner of lowest X and highest Y is (x, y); walls on the named sides
    (west = low X, east = high X, north = high Y, south = low Y)."""
    mesh(s, 'Bays', CA + 'SM_FloorCorridor', (x, y, z), mats=C_FLOOR)
    mesh(s, 'Bays', CA + 'SM_CeilingVaulted_01', (x, y, z + VAULT_H), mats=C_VAULT, collide=False)
    if west:
        mesh(s, 'Bays', CA + 'SM_CorridorWall_01', (x, y - 800.0, z), rot(yaw=90), mats=C_WALL)
    if east:
        mesh(s, 'Bays', CA + 'SM_CorridorWall_01', (x + 768.0, y - 800.0, z), rot(yaw=90), mats=C_WALL)
    if north:
        mesh(s, 'Bays', CA + 'SM_CorridorWall_01', (x, y, z), mats=C_WALL)
    if south:
        mesh(s, 'Bays', CA + 'SM_CorridorWall_01', (x, y - 768.0, z), mats=C_WALL)


async def stage_crypt():
    s = 'crypt'
    x0, x1, y0, y1 = PIT
    # The stair's mouth in the Main Hall: an open flight down along the hall's back wall, with a parapet on its two
    # long sides (a player who steps off lands on the stair and walks out).
    flight(s, (x1, y1, FLOOR_Z - 300.0), 180.0, walls=False)
    for y in (y0, y1):
        box(s, 'Mouth', (x0, x1), (y - 12.0, y + 12.0), (FLOOR_Z - 320.0, FLOOR_Z + 95.0), MI + 'MI_BrickStoneWall_nonDisp')
    box(s, 'Mouth', (x0, x1 + 200.0), (y0, y1), (FLOOR_Z - 340.0, FLOOR_Z - 318.0), MI + 'MI_BrickStoneWall_nonDisp', collide=False)
    # Under the hall's back wall, across the foot of the grand stair's well (open above: low side walls only), under
    # the keep's outer wall and on into the rock: the second flight. No vault of the set here: it is 8 m high and would
    # stand in the well. Then a landing, a turn towards the King's Hall side, two more flights. Twelve metres down in
    # all; the turns hide what comes next.
    flight(s, (x1 + 800.0, y1, FLOOR_Z - 600.0), 180.0, walls=False)
    stone = MI + 'MI_BrickStoneWall_nonDisp'
    for y in (y0, y1):
        box(s, 'Mouth', (x1, x1 + 800.0), (y - 12.0, y + 12.0), (FLOOR_Z - 640.0, FLOOR_Z), stone)
    box(s, 'Mouth', (x1 + 400.0, x1 + 800.0), (y0, y1), (FLOOR_Z - 90.0, FLOOR_Z - 60.0), stone)          # a ceiling once outside the keep
    box(s, 'Mouth', (x1 + 790.0, x1 + 810.0), (y0, y1), (FLOOR_Z - 90.0, FLOOR_Z + 600.0), stone)       # the landing's wall over the stair's mouth
    lx = x1 + 800.0                                   # the landing's low-X edge
    bay(s, lx, y1, FLOOR_Z - 600.0, west=False, east=True, north=True, south=False)
    flight(s, (lx + 800.0, y0 - 800.0, FLOOR_Z - 900.0), 90.0)
    flight(s, (lx + 800.0, y0 - 1600.0, CRYPT_Z), 90.0)
    # The cell corridor: three bays; a cell opens off each one on the far side from the keep.
    cy = y0 - 1600.0                                  # the corridor's first edge (high Y)
    for k in range(3):
        y = cy - 800.0 * k
        bay(s, lx, y, CRYPT_Z, west=True, east=False)
        mesh(s, 'Cells', CA + 'SM_CorridorCellWall_01', (lx + 800.0, y, CRYPT_Z), rot(yaw=90), mats=C_WALL)
        bay(s, lx + 800.0, y, CRYPT_Z, west=False, east=True, north=True, south=(k == 2))
        if k != 1:                                    # the middle cell stands open
            mesh(s, 'Cells', CA + 'SM_CorridorCellBars_01', (lx + 800.0, y - 606.0, CRYPT_Z), rot(yaw=90), (1.45, 1.0, 1.12), mats=[MI + 'MI_Metal'] * 4)
    mesh(s, 'Cells', PROPS + 'SM_Chest_Open_01', (lx + 1350.0, cy - 1200.0, CRYPT_Z), rot(yaw=180))
    # The seal's hall: three by three bays on four pillars, entered from the corridor in the middle of one side.
    hy = cy - 2400.0
    hx = lx - 800.0
    for i in range(3):
        for j in range(3):
            bay(s, hx + 800.0 * i, hy - 800.0 * j, CRYPT_Z, west=(i == 0), east=(i == 2), north=(j == 0 and i != 1), south=(j == 2))
    for px in (hx + 800.0, hx + 1600.0):
        for py in (hy - 800.0, hy - 1600.0):
            mesh(s, 'Hall', CT + 'Candle/StaticMeshes/Architecture/SM_PillarStem_01', (px, py, CRYPT_Z - 8.0), scale=(1.6, 1.6, 0.43), mats=C_WALL)
    # The seal, at the far end, between four standing stones (the four points of the future's emitters).
    sx, sy = hx + 1200.0, hy - 1900.0
    seal = eas.spawn_actor_from_class(unreal.GBSeal, unreal.Vector(sx, sy, CRYPT_Z + 5.0), rot())
    seal.set_editor_property('travel_map_path', FUTURE_MAP)      # the future of this site is another map
    tag(seal, s, 'Gameplay')
    for dx, dy in ((-260, -260), (260, -260), (-260, 260), (260, 260)):
        mesh(s, 'Hall', CA + 'SM_PillarBlock_01', (sx + dx + 69.0, sy + dy - 110.0, CRYPT_Z), scale=(1.0, 1.0, 0.3), mats=C_WALL)
    point_light(s, 'Light', (sx, sy, CRYPT_Z + 230.0), (170, 215, 255), 120.0, 2800.0, shadow=True)
    point_light(s, 'Light', (sx, sy + 900.0, CRYPT_Z + 330.0), (150, 190, 255), 16.0, 1800.0)
    # Candle light: one flame's worth at each turn of the stair and in each bay of the corridor (in the pixel look,
    # torches on the walls instead: stage "torches").
    for x, y, z in () if pixel(s) else ((x1 - 420.0, y1 - 400.0, FLOOR_Z - 40.0), (lx + 400.0, y1 - 400.0, FLOOR_Z - 600.0 + 260.0),
                    (lx + 400.0, cy - 200.0, CRYPT_Z + 260.0), (lx + 60.0, cy - 1200.0, CRYPT_Z + 260.0), (lx + 60.0, cy - 2000.0, CRYPT_Z + 260.0),
                    (lx + 1200.0, cy - 1200.0, CRYPT_Z + 200.0)):
        point_light(s, 'Light', (x, y, z), (255, 190, 130), 14.0, 1100.0)
    # The rock around the crypt: plain slabs on every side, above and below, so no daylight reaches it. Two parts: behind
    # the keep (from its back wall), and beyond the King's Hall, where the seal's hall reaches further west.
    rock = MI + 'MI_Ground_Rock_A'
    split = hy + 100.0                               # just north of the seal's hall
    for rx0, rx1, ry0, ry1 in ((hx - 110.0, CRYPT_AREA[1], CRYPT_AREA[2], split), (x1 + 480.0, CRYPT_AREA[1], split, CRYPT_AREA[3])):
        box(s, 'Rock', (rx0, rx1), (ry0, ry1), (1050.0, 1100.0), rock, collide=False)
        box(s, 'Rock', (rx0, rx1), (ry0, ry1), (CRYPT_Z - 80.0, CRYPT_Z - 30.0), rock)
        box(s, 'Rock', (rx1 - 50.0, rx1), (ry0, ry1), (CRYPT_Z - 80.0, 1100.0), rock, collide=False)
    box(s, 'Rock', (hx - 110.0, hx - 60.0), (CRYPT_AREA[2], split), (CRYPT_Z - 80.0, 1100.0), rock, collide=False)
    box(s, 'Rock', (hx - 110.0, CRYPT_AREA[1]), (CRYPT_AREA[2], CRYPT_AREA[2] + 50.0), (CRYPT_Z - 80.0, 1100.0), rock, collide=False)
    box(s, 'Rock', (hx - 110.0, x1 + 530.0), (split - 50.0, split), (CRYPT_Z - 80.0, 1100.0), rock, collide=False)
    box(s, 'Rock', (x1 + 480.0, CRYPT_AREA[1]), (CRYPT_AREA[3] - 50.0, CRYPT_AREA[3]), (CRYPT_Z - 80.0, 1100.0), rock, collide=False)
    for ry0, ry1 in ((split, y0 - 60.0), (y1 + 60.0, CRYPT_AREA[3])):
        box(s, 'Rock', (x1 + 480.0, x1 + 530.0), (ry0, ry1), (CRYPT_Z - 80.0, 1100.0), rock, collide=False)
    log.append('crypt: stair of four flights from the Main Hall, three cells, the seal\'s hall at Z %d' % CRYPT_Z)


WINDOW_KEY = 'CastleTower_Window_A_'     # the pack's walls with a window and their pixel twins (SM_Px_...)
_wall_axes = {}


def wall_axes(sm):
    """A wall piece is flat: its long direction in its own XY, read from its vertices (the tower's pieces run at 45
    degrees). Returns (centre x, centre y, angle, along min, along max, across min, across max)."""
    key = sm.get_path_name()
    if key not in _wall_axes:
        sm.set_editor_property('allow_cpu_access', True)
        pts = []
        for sec in range(sm.get_num_sections(0)):
            r = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, sec)
            pts += [(v.x, v.y) for v in r[0]]
        n = float(len(pts))
        mx, my = sum(p[0] for p in pts) / n, sum(p[1] for p in pts) / n
        sxx = sum((p[0] - mx) ** 2 for p in pts) / n
        syy = sum((p[1] - my) ** 2 for p in pts) / n
        sxy = sum((p[0] - mx) * (p[1] - my) for p in pts) / n
        ang = 0.5 * math.atan2(2 * sxy, sxx - syy)
        c, sn = math.cos(ang), math.sin(ang)
        us = [(p[0] - mx) * c + (p[1] - my) * sn for p in pts]
        ns = [-(p[0] - mx) * sn + (p[1] - my) * c for p in pts]
        _wall_axes[key] = (mx, my, ang, min(us), max(us), min(ns), max(ns))
    return _wall_axes[key]


def window_cells(a, world, step=15.0):
    """The cells of a window wall that a straight line crosses without touching anything, from either side."""
    mx, my, ang, u0, u1, n0, n1 = wall_axes(a.static_mesh_component.static_mesh)
    c, sn = math.cos(ang), math.sin(ang)
    t = a.get_actor_transform()

    def at(u, n, z):
        return unreal.MathLibrary.transform_location(t, unreal.Vector(mx + u * c - n * sn, my + u * sn + n * c, z))
    cells = set()
    nu, nz = int((u1 - u0 - 30.0) / step), int(350.0 / step)
    for i in range(nu + 1):
        for j in range(nz + 1):
            u, z = u0 + 15.0 + i * step, 25.0 + j * step
            p, q = at(u, n0 - 40.0, z), at(u, n1 + 40.0, z)
            if unreal.SystemLibrary.line_trace_single(world, p, q, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, False):
                continue
            if unreal.SystemLibrary.line_trace_single(world, q, p, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, False):
                continue
            cells.add((i, j))
    return cells, at, (u0, n0, n1, step)


async def stage_windows():
    """No window can be passed (owner, 2026-11-14). Every wall piece with a window is crossed by lines on a 15 cm grid,
    from both sides; wherever a line goes through, an unseen blocking pane is set in the opening. Then every opening
    is crossed again and must stop the line."""
    s = 'windows'
    world = ues.get_editor_world()
    walls = [a for a in eas.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor) and a.actor_has_tag(TAG)
             and a.static_mesh_component.static_mesh and WINDOW_KEY in a.static_mesh_component.static_mesh.get_name()]
    panes = 0
    open_walls = 0
    for k, a in enumerate(walls):
        cells, at, (u0, n0, n1, step) = window_cells(a, world)
        if not cells:
            continue
        open_walls += 1
        todo = set(cells)
        while todo:                                   # one pane per opening (cells that touch)
            group, stack = [], [todo.pop()]
            while stack:
                ci, cj = stack.pop()
                group.append((ci, cj))
                for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nb = (ci + d[0], cj + d[1])
                    if nb in todo:
                        todo.remove(nb)
                        stack.append(nb)
            i0, i1 = min(g[0] for g in group), max(g[0] for g in group)
            j0, j1 = min(g[1] for g in group), max(g[1] for g in group)
            ua, ub = u0 + 15.0 + i0 * step - step, u0 + 15.0 + i1 * step + step
            za, zb = 25.0 + j0 * step - step, 25.0 + j1 * step + step
            nm = (n0 + n1) / 2.0
            pa, pb, pc = at(ua, nm, (za + zb) / 2), at(ub, nm, (za + zb) / 2), at((ua + ub) / 2, nm, (za + zb) / 2)
            yaw = math.degrees(math.atan2(pb.y - pa.y, pb.x - pa.x))
            b = eas.spawn_actor_from_object(asset('/Engine/BasicShapes/Cube'), pc, rot(yaw=yaw))
            b.set_actor_scale3d(unreal.Vector(math.hypot(pb.x - pa.x, pb.y - pa.y) / 100.0, 0.12, (zb - za) / 100.0))
            bc = b.static_mesh_component
            bc.set_mobility(unreal.ComponentMobility.STATIC)
            bc.set_collision_profile_name('BlockAll')
            bc.set_visibility(False)
            bc.set_editor_property('cast_shadow', False)
            b.set_actor_hidden_in_game(True)
            tag(b, s, 'Panes')
            panes += 1
        if k % 40 == 0:
            await asyncio.sleep(0)
    await asyncio.sleep(0.5)
    still = 0
    for a in walls:
        cells, _, _ = window_cells(a, world)
        still += 1 if cells else 0
    log.append('windows: %d wall pieces with a window, %d had an opening a line could cross, %d unseen panes set; still open after that: %d'
               % (len(walls), open_walls, panes, still))


KEG = PROPS + 'SM_Large_Keg'            # 1.13 x 1.33 m, 1.27 m high: low cover, and a step
BARREL = PROPS + 'SM_Barrel_01'         # 0.54 m across, 0.73 m high
CRATE = PROPS + 'Crates/SM_Crate_B_01'  # 0.6 m cube, pivot at a corner


def barricade(s, x, y, yaw, kinds, z=0.0):
    """A row of the camp's stores, laid along the given direction on a floor at height z: 'K' a large keg, 'B' two
    barrels, 'C' crates stacked two high."""
    rad = math.radians(yaw)
    dx, dy = math.cos(rad), math.sin(rad)
    d = 0.0
    for i, k in enumerate(kinds):
        px, py = x + dx * d, y + dy * d
        if k == 'K':
            mesh(s, 'Camp', KEG, (px, py, z), rot(yaw=yaw + (8 if i % 2 else -6)))
            d += 140.0
        elif k == 'B':
            mesh(s, 'Camp', BARREL, (px, py - 20.0 * dx, z), rot(yaw=yaw + 15))
            mesh(s, 'Camp', BARREL, (px + 45.0 * dx, py + 45.0 * dy, z), rot(yaw=yaw + 70))
            d += 110.0
        else:
            mesh(s, 'Camp', CRATE, (px + 30.0, py + 30.0, z), rot(yaw=yaw + 5))
            mesh(s, 'Camp', CRATE, (px + 30.0, py + 30.0, z + 60.0), rot(yaw=yaw - 9))
            mesh(s, 'Camp', CRATE, (px + 30.0 + 62.0 * dx, py + 30.0 + 62.0 * dy, z), rot(yaw=yaw))
            d += 135.0


async def stage_camp():
    s = 'camp'
    # The orcs' camp in the lower ward: a fire in the middle, their stores stacked as barricades between it and the
    # gate and across the way to the stair, a cart left where it was unloaded. No line runs clear from gate to stair.
    mesh(s, 'Camp', PROPS + 'SM_FirePit_01', (1400.0, 0.0, 0.0), scale=(1.6, 1.6, 1.6))
    fire = point_light(s, 'Camp', (1400.0, 0.0, 90.0), (255, 150, 70), 260.0, 1900.0, shadow=True)
    fire.light_component.set_editor_property('light_function_material', asset('/Game/Glyphbound/Materials/M_GB_LightFlicker'))
    barricade(s, 820.0, -1340.0, 0.0, 'KKCK')
    barricade(s, 1880.0, 780.0, 90.0, 'KBKK')
    barricade(s, 640.0, 1040.0, 0.0, 'CKB')
    barricade(s, 2080.0, -1000.0, 90.0, 'KKC')
    barricade(s, 1250.0, -420.0, 20.0, 'BC')
    mesh(s, 'Camp', PROPS + 'carts/SM_Cart', (560.0, -560.0, 0.0), rot(yaw=28))
    for x, y, yaw in ((1250, 210, 40), (1560, 190, 150), (1620, -170, 250), (1210, -150, 330)):
        mesh(s, 'Camp', PROPS + 'SM_Sack_01', (x, y, 0.0), rot(yaw=yaw), collide=False)
    # The orcs' stores in the far side yard, on the upper ward.
    barricade(s, 5900.0, 2250.0, 0.0, 'KKCKB', z=TERRACE_Z)      # on the upper ward (review: it stood buried at Z 0)
    mesh(s, 'Camp', PROPS + 'SM_WeaponStand', (6800.0, 2350.0, TERRACE_Z), rot(yaw=180))
    log.append('camp: fire, five barricades, cart, the stores in the side yard')


# Fights: (encounter id, trigger middle, trigger half size, floor height, spawn points (x, y, floor z)).
ENCOUNTERS_ON = True
ENCOUNTERS = [
    # the opening fight: it starts when the player is out of the gate passage; the orcs are at the fire, behind the
    # barricades and on the terrace
    ('Opening_Past', (500.0, 0.0, 150.0), (350.0, 900.0, 300.0),
     [(1650, 250, 0), (1550, -300, 0), (2050, 500, 0), (2150, -1250, 0), (1950, 1450, 0), (3300, 0, 210), (3400, 700, 210), (3400, -700, 210)]),
]

# Review spots: (name, x, y, floor z, yaw). gb.Smoke tour stands the player on each and takes a screenshot.
TOUR = [('01_start', -2300, 0, 0, 0), ('02_gate_passage', -700, 0, 0, 0), ('03_ward_in', 250, 0, 0, 0), ('04_ward_fire', 1000, 500, 0, -20),
        ('05_ward_south', 1500, -2200, 0, 60), ('06_stair_foot', 2300, -200, 0, 0), ('07_breach', 2200, 2000, 0, 0),
        ('08_terrace', 3000, -200, 210, 0), ('09_terrace_back', 4800, 0, 210, 180), ('10_side_yard_stores', 5700, 2100, 210, 0),
        ('11_side_yard_stair', 5300, -2000, 210, -10), ('12_keep_door', 4900, 0, 210, 0), ('13_front_hall', 5750, 0, 240, 0),
        ('14_front_hall_aisle', 6500, -1100, 240, 30), ('15_front_hall_back', 7400, 0, 240, 180), ('16_main_hall', 8300, 400, 260, 0),
        ('17_main_hall_side', 8600, -900, 260, 45), ('18_main_hall_up', 8700, 900, 260, -60), ('19_crypt_mouth', 8950, -600, 236, 0), ('20_stair_1', 9700, -600, 82, 0),
        ('21_landing', 11290, -600, -364, -90), ('22_stair_4', 11290, -2200, -814, -90), ('23_cells', 11290, -2900, -964, -90),
        ('24_cell_open', 11600, -3800, -964, 0), ('25_corridor_end', 11290, -4800, -964, -90), ('26_seal_hall', 11290, -5300, -964, -90),
        ('27_sealmap', 11290, -6500, -964, -90), ('28_seal_back', 11290, -7150, -964, 90),
        # the owner's notes of his first walk: the wall beside the gate from outside (both sides), the cart, the wide stair's
        # top, the wall stair and the wall walk it now leads to, the paving at the keep's entrance
        ('30_gate_wall_left', -1900, -1750, 0, 22), ('31_gate_wall_right', -1900, 1750, 0, -22), ('32_cart', 1000, -950, 0, 135),
        ('33_stair_top', 2900, -200, 210, 180), ('34_wall_stair', 5350, -1850, 210, -35), ('35_stair_landing', STAIR_X - 250, -2470, 1211, -90),
        ('36_wall_walk', STAIR_X - 290, -2816, 1203, 0), ('37_wall_walk_gate_end', 1500, -2816, 1203, 180), ('37b_corner_tower', -300, -2816, 1203, 180), ('37c_keep_end_tower', 6800, -2816, 1203, 0),
        ('37d_in_keep_end_tower', 7300, -2816, 1203, 180), ('38_entrance_right', 5380, 950, 210, 35),
        ('39_door_side', 5050, -700, 210, 25), ('39b_door_side_right', 5050, 700, 210, -25)]

WALKS = [('road through the gate into the ward', [(-2300, 0, 0), (2300, 0, 0)]),
         ('up the wide stair to the keep\'s door', [(2300, -200, 0), (2505, -200, 0), (2712, -200, 210), (4900, -200, 210), (5200, 0, 210)]),
         ('up the breach', [(2200, 2000, 0), (2800, 2000, 210), (3200, 2000, 210)]),
         ('into the Front Hall and through to the Main Hall', [(5500, 0, 240), (7400, 400, 240), (8300, 400, 260)]),
         ('terrace to the far side yard', [(4500, 2100, 210), (6800, 2100, 210)]),
         ('along the wall walk on the King\'s side, from inside the keep-end tower through the corner tower to its far door', [(7300, -2816, 1203), (-640, -2816, 1203)]),
         ('along the wall walk on the other side, tower to tower', [(7300, 2816, 1203), (-640, 2816, 1203)]),
         ('from the wall stair\'s landing through the opening onto the wall walk', [(STAIR_X - 275, -2500, 1212), (STAIR_X - 275, -2690, 1212), (STAIR_X - 275, -2816, 1203)]),
         ('across the terrace in front of the keep, wall to wall', [(4700, -2400, 210), (4700, 2400, 210)]),
         ('Main Hall down the crypt stair to the landing', [(8900, -600, 236), (9295, -600, 236), (10085, -600, -64), (10885, -600, -364), (11290, -600, -364)]),
         ('landing down to the cells and into the seal\'s hall', [(11290, -600, -364), (11290, -1005, -364), (11290, -1795, -664), (11290, -2595, -964), (11290, -6800, -964)]),
         ('back up from the seal\'s hall to the Main Hall', [(11290, -6800, -964), (11290, -2595, -964), (11290, -1795, -664), (11290, -1005, -364), (11290, -600, -364), (10885, -600, -364), (10085, -600, -64), (9295, -600, 236), (8900, -600, 236)])]


async def stage_gameplay():
    s = 'gameplay'
    if ENCOUNTERS_ON:
        for enc_id, centre, extent, spawns in ENCOUNTERS:
            a = eas.spawn_actor_from_class(unreal.GBEncounter, unreal.Vector(*centre), rot())
            a.set_editor_property('encounter_id', unreal.Name(enc_id))
            a.set_editor_property('presence', unreal.GBEraPresence.PAST_ONLY)
            a.set_editor_property('trigger_extent', unreal.Vector(*extent))
            a.set_editor_property('spawn_points', [unreal.Vector(x - centre[0], y - centre[1], z + 100.0 - centre[2]) for x, y, z in spawns])
            tag(a, s, 'Gameplay')
    for name, x, y, z, yaw in TOUR:
        a = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, z), rot(yaw=yaw))
        a.tags = [unreal.Name(TAG), unreal.Name('stage_' + s), unreal.Name('GBTour'), unreal.Name('tour_' + name)]
        a.set_folder_path(unreal.Name('Castle/tour'))
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(4000.0, 0.0, 400.0), rot())
    nav.set_actor_scale3d(unreal.Vector(72.0, 62.0, 12.0))
    nav2 = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(11300.0, -4000.0, -500.0), rot())
    nav2.set_actor_scale3d(unreal.Vector(18.0, 40.0, 8.0))
    tag(nav2, s, 'Gameplay')    # the volume's brush is a 2 m cube
    tag(nav, s, 'Gameplay')
    log.append('gameplay: %d fights, %d review spots, nav bounds' % (len(ENCOUNTERS) if ENCOUNTERS_ON else 0, len(TOUR)))


BODY_R, BODY_H, STEP_UP = 38.0, 180.0, 45.0     # the player's capsule (Player.json) and the highest step it walks up


def body_walk(world, pts):
    """Moves a body of the player's size along a walk, on the floor it finds under it: a capsule as wide as the player
    and as tall as the player above his highest step, swept from one sample to the next (30 cm). Returns what stops it:
    something across the way at any height of the body (a low lintel, a kerb above a step's height), or a rise in the
    floor higher than a step. None when the body gets through."""
    half = (BODY_H - STEP_UP) / 2.0
    prev = None
    prev_floor = None
    for (x0, y0, z0), (x1, y1, z1) in zip(pts[:-1], pts[1:]):
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 30.0))
        for i in range(n + 1):
            t = i / float(n)
            x, y, z = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 110), unreal.Vector(x, y, z - 200),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            if not hit:
                prev, prev_floor = None, None
                continue                             # the floor check reports a hole
            floor = hit.to_tuple()[4].z
            if prev_floor is not None and floor - prev_floor > STEP_UP:
                return 'a rise of %d cm in the floor at (%d, %d, %d): more than a step' % (floor - prev_floor, x, y, floor)
            centre = unreal.Vector(x, y, floor + STEP_UP + half + 2.0)
            if prev is not None:
                sweep = unreal.SystemLibrary.capsule_trace_single(world, prev, centre, BODY_R, half, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                                  True, [], unreal.DrawDebugTrace.NONE, True)
                if sweep:
                    tup = sweep.to_tuple()
                    return 'the body is stopped at (%d, %d, %d) by %s (floor there %d)' % (tup[5].x, tup[5].y, tup[5].z,
                                                                                          tup[9].get_actor_label() if tup[9] else '?', floor)
            prev, prev_floor = centre, floor
    return None


def check_walks():
    world = ues.get_editor_world()
    bad = []
    for name, pts in WALKS:
        for (x0, y0, z0), (x1, y1, z1) in zip(pts[:-1], pts[1:]):
            wall = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x0, y0, z0 + 130), unreal.Vector(x1, y1, z1 + 130),
                                                          unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
            if wall:
                t = wall.to_tuple()
                bad.append('%s: BLOCKED at (%d, %d, %d) by %s' % (name, t[4].x, t[4].y, t[4].z, t[9].get_actor_label() if t[9] else '?'))
            n = max(1, int(math.hypot(x1 - x0, y1 - y0) / 20.0))
            run = []
            for i in range(n + 1):
                t = i / float(n)
                x, y, z = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t
                hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 70), unreal.Vector(x, y, z - 55),
                                                             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
                if not hit or abs(hit.to_tuple()[4].z - z) > 40:
                    run.append((int(x), int(y), int(hit.to_tuple()[4].z) if hit else None))
            if run:
                bad.append('%s: no floor from %s to %s (%d points)' % (name, run[0], run[-1], len(run)))
        body = body_walk(world, pts)
        if body:
            bad.append('%s: %s' % (name, body))
    log.append('walks: ' + ('clear on %d walks' % len(WALKS) if not bad else 'PROBLEMS -> ' + ' | '.join(bad)))



def enable_collision():
    """Kit pieces block exactly as they look: complex collision used as simple, answering from both sides (most of the
    castle kit has no simple collision at all). The flag is saved with the mesh; double-sided takes effect the next
    time the Editor loads the asset."""
    done = 0
    seen = set()
    for a in eas.get_all_level_actors():
        if not (isinstance(a, unreal.StaticMeshActor) and a.actor_has_tag(TAG)):
            continue
        c = a.static_mesh_component
        sm = c.static_mesh
        if not sm or not sm.get_path_name().startswith(CT) or sm.get_path_name() in seen:
            continue
        seen.add(sm.get_path_name())
        if any(k in sm.get_name() for k in NO_BLOCK):
            continue
        bs = sm.get_editor_property('body_setup')
        if bs and (bs.get_editor_property('collision_trace_flag') != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
                   or not bs.get_editor_property('double_sided_geometry')):
            bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            bs.set_editor_property('double_sided_geometry', True)
            eal.save_asset(sm.get_path_name().split('.')[0], only_if_is_dirty=False)
            done += 1
    log.append('collision: complex-as-simple set on %d mesh assets (of %d in use)' % (done, len(seen)))


async def stage_torches():
    """More light in the passages (owner, 2026-11-14), each a torch on a wall: the gate passage, the crypt stair's
    landing and lower flights, the cell corridor. Found by looking for the wall from the passage's middle."""
    s = 'torches'
    if not pixel(s):
        log.append('torches: pack look, nothing to do')
        return
    x0, x1, y0, y1 = PIT
    lx, cy = x1 + 800.0, y0 - 1600.0
    spots = [(-850.0, 0.0, 0.0, 0, 1), (-850.0, 0.0, 0.0, 0, -1), (-350.0, 0.0, 0.0, 0, 1), (-350.0, 0.0, 0.0, 0, -1),   # gate passage
             (lx + 400.0, y1 - 400.0, FLOOR_Z - 600.0, 1, 0),                                                      # crypt landing
             (lx + 400.0, y0 - 400.0, FLOOR_Z - 750.0, 1, 0), (lx + 400.0, y0 - 1200.0, CRYPT_Z + 150.0, -1, 0),  # lower flights
             (lx + 400.0, cy - 400.0, CRYPT_Z, -1, 0), (lx + 400.0, cy - 1200.0, CRYPT_Z, -1, 0), (lx + 400.0, cy - 2000.0, CRYPT_Z, -1, 0)]  # cell corridor
    n = sum(1 for x, y, z, dx, dy in spots if torch_on_wall(s, 'Torches', x, y, z, dx, dy))
    log.append('torches: %d torches in the passages (of %d places)' % (n, len(spots)))


ALL = [('world', stage_world), ('ground', stage_ground), ('keep', stage_keep), ('gate', stage_gate), ('walls', stage_walls),
       ('wards', stage_wards), ('camp', stage_camp), ('crypt', stage_crypt), ('torches', stage_torches), ('gameplay', stage_gameplay)]

# ---- run ----
if not eal.does_asset_exist(MAP):
    les.new_level(MAP, False)
    les.save_current_level()
elif ues.get_editor_world().get_path_name().split('.')[0] != MAP:
    les.load_level(MAP)
await asyncio.sleep(0)
for name, fn in ALL:
    if STAGES and name not in STAGES:
        continue
    removed = clear(name)
    await fn()
    await asyncio.sleep(0)
enable_collision()
await asyncio.sleep(0.5)
if not STAGES or 'windows' in STAGES:
    clear('windows')            # after the collision flags: before them a wall piece blocks nothing
    await stage_windows()
if not CFG.get('no_walks'):
    check_walks()
les.save_current_level()
open(os.path.join(unreal.Paths.project_saved_dir(), 'castle_build_result.txt'), 'w').write('\n'.join(log))
'\n'.join(log)
