# Build-SiteMap.py - builds the Glyphbound site (one persistent map, _Past and _Future sub-levels) from
# Design/Levels/world_layout.json. Runs inside the Unreal Editor (Python), e.g. through the MCP Python tool:
#   exec(open(r'C:/GameJam/Tools/Jam/Build-SiteMap.py').read())
#
# Idempotent: every actor it makes carries the tag GBGen; a run first deletes the GBGen actors of the three
# levels, then places everything again. Hand-placed actors (no GBGen tag) are never touched.
#
# Layout (decided by the architect, 2026-11-13):
#   /Game/Glyphbound/Maps/Site         persistent: floor slab, shared ("both") structures, player start, nav bounds,
#                                      portals, encounters, checkpoints, era blocks (locks, pylons), chests
#   /Game/Glyphbound/Maps/Site_Past    past-only geometry and lighting (UGBEraSubsystem shows it in the past)
#   /Game/Glyphbound/Maps/Site_Future  future-only geometry and lighting
# Greybox: engine cubes with the engine's world-aligned grid material; the kit meshes come in the dressing pass.

import json
import math
import unreal

LAYOUT_PATH = r'C:/GameJam/Design/Levels/world_layout.json'
MAP_DIR = '/Game/Glyphbound/Maps'
PERSISTENT = MAP_DIR + '/Site'
SUBLEVELS = {'past': MAP_DIR + '/Site_Past', 'future': MAP_DIR + '/Site_Future'}
TAG = 'GBGen'
CUBE = '/Engine/BasicShapes/Cube.Cube'
GRID_MAT = '/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial'
CEILING_THICK = 50.0
RAMP_THICK = 20.0
BARRIER_THICK = 60.0
KILL_Z = -2000.0

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
log_lines = []


def log(msg):
    log_lines.append(msg)
    unreal.log('[Build-SiteMap] ' + msg)


def load_layout():
    with open(LAYOUT_PATH, 'r', encoding='utf-8') as f:
        return json.load(f)


# ---------------------------------------------------------------- maps

def ensure_maps():
    """Creates the three maps when missing (not World Partition), adds the sub-levels to the persistent map."""
    for key, path in SUBLEVELS.items():
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            les.new_level(path, False)
            les.save_current_level()
            log('created ' + path)
    if not unreal.EditorAssetLibrary.does_asset_exist(PERSISTENT):
        les.new_level(PERSISTENT, False)
        les.save_current_level()
        log('created ' + PERSISTENT)
    les.load_level(PERSISTENT)
    world = ues.get_editor_world()
    for key, path in SUBLEVELS.items():
        if unreal.GameplayStatics.get_streaming_level(world, unreal.Name(path)) is None:
            # Dynamic streaming: UGBEraSubsystem::ApplyEra loads both at BeginPlay and flips their visibility.
            unreal.EditorLevelUtils.add_level_to_world(world, path, unreal.LevelStreamingDynamic)
            log('added sub-level ' + path)
    return world


def level_key_of(actor):
    path = actor.get_path_name()
    if '/Site_Past.' in path:
        return 'past'
    if '/Site_Future.' in path:
        return 'future'
    if '/Site.' in path:
        return 'persistent'
    return None


def clear_generated():
    doomed = [a for a in eas.get_all_level_actors() if TAG in [str(t) for t in a.tags] and level_key_of(a)]
    if doomed:
        eas.destroy_actors(doomed)
    log('removed %d generated actors' % len(doomed))


def set_current(key):
    if key == 'persistent':
        les.set_current_level_by_name('Site')
    else:
        les.set_current_level_by_name('Site_Past' if key == 'past' else 'Site_Future')


# ---------------------------------------------------------------- spawning helpers

_cube = None
_grid = None


def cube_mesh():
    global _cube, _grid
    if _cube is None:
        _cube = unreal.load_asset(CUBE)
        _grid = unreal.load_asset(GRID_MAT)
    return _cube, _grid


def tag(actor, folder, label):
    actor.set_editor_property('tags', [unreal.Name(TAG)])
    actor.set_folder_path(unreal.Name('Gen/' + folder))
    actor.set_actor_label(label)
    return actor


def spawn_box(folder, label, x0, x1, y0, y1, z0, z1, rot=None):
    mesh, mat = cube_mesh()
    centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5)
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, centre, rot or unreal.Rotator(0, 0, 0))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector(max(abs(x1 - x0), 1) / 100.0, max(abs(y1 - y0), 1) / 100.0, max(abs(z1 - z0), 1) / 100.0))
    return tag(actor, folder, label)


def spawn_rect(folder, label, rect, z0, z1):
    return spawn_box(folder, label, rect[0], rect[1], rect[2], rect[3], z0, z1)


def spawn_ramp(folder, label, rect, z0, z1, asc):
    """A walkable slab whose top runs from z0 at the low end to z1 at the high end of rect, climbing towards asc."""
    mesh, mat = cube_mesh()
    x0, x1, y0, y1 = rect
    along_x = asc in ('+X', '-X')
    run = (x1 - x0) if along_x else (y1 - y0)
    width = (y1 - y0) if along_x else (x1 - x0)
    rise = z1 - z0
    length = math.hypot(run, rise)
    slope = math.degrees(math.atan2(rise, run))
    yaw = {'+X': 0.0, '-X': 180.0, '+Y': 90.0, '-Y': -90.0}[asc]
    # Centre of the top surface, then half the slab thickness down along the slab's normal.
    cx, cy, cz = (x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5
    n_horizontal = -math.sin(math.radians(slope)) * RAMP_THICK * 0.5
    n_vertical = -math.cos(math.radians(slope)) * RAMP_THICK * 0.5
    dir_x = math.cos(math.radians(yaw))
    dir_y = math.sin(math.radians(yaw))
    centre = unreal.Vector(cx - dir_x * n_horizontal, cy - dir_y * n_horizontal, cz + n_vertical)
    rot = unreal.Rotator(roll=0.0, pitch=slope, yaw=yaw)
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, centre, rot)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector(length / 100.0, width / 100.0, RAMP_THICK / 100.0))
    return tag(actor, folder, label)


def presence_enum(era):
    return {'past': unreal.GBEraPresence.PAST_ONLY, 'future': unreal.GBEraPresence.FUTURE_ONLY}.get(era, unreal.GBEraPresence.BOTH)


def set_era(actor, era, required=None, removed=None):
    comp = actor.get_component_by_class(unreal.GBEraComponent)
    if comp is None:
        log('WARNING: %s has no GBEraComponent' % actor.get_actor_label())
        return
    comp.set_editor_property('presence', presence_enum(era))
    comp.set_editor_property('required_flag', unreal.Name(required or 'None'))
    comp.set_editor_property('removed_by_flag', unreal.Name(removed or 'None'))


# ---------------------------------------------------------------- the build

def build_shell(layout):
    floor = layout['shell']['floor']
    set_current('persistent')
    spawn_rect('Shell', 'Floor_Slab', floor['rect'], floor['z0'], floor['z1'])
    for era in ('past', 'future'):
        set_current(era)
        shell = layout['shell'][era]
        for i, w in enumerate(shell['walls']):
            spawn_wall(w, 'Shell', '%s_Wall_%02d' % (era.capitalize(), i))
        for i, w in enumerate(shell['lintels']):
            spawn_wall(w, 'Shell', '%s_Lintel_%s' % (era.capitalize(), w.get('door', i)))


def spawn_wall(w, folder, label):
    half = w['thick'] * 0.5
    if w['axis'] == 'X':   # a wall on the line X = at, running along Y
        return spawn_box(folder, label, w['centre'] - half, w['centre'] + half, w['from'], w['to'], w['z0'], w['z1'])
    return spawn_box(folder, label, w['from'], w['to'], w['centre'] - half, w['centre'] + half, w['z0'], w['z1'])


def build_ceilings(layout):
    for room in layout['rooms']:
        for era in ('past', 'future'):
            if room['eras'] not in ('both', era):
                continue
            ceil = room.get('ceilPast' if era == 'past' else 'ceilFuture') or 0
            if ceil <= 0:
                continue   # open sky
            rect = room.get('futureRect') if era == 'future' and room.get('futureRect') else room['rect']
            set_current(era)
            spawn_rect('Ceilings', '%s_Ceiling_%s' % (era.capitalize(), room['id']), rect, ceil, ceil + CEILING_THICK)


def build_structures(layout):
    counts = {}
    for s in layout['structures']:
        if s.get('sealed'):
            continue
        kind = s['kind']
        if kind == 'prop':
            continue   # dressing only, placed in the dressing pass
        era = s['era']
        if s.get('removedBy') or kind == 'feature':
            continue   # flag-driven pieces are era blocks (build_era_blocks)
        set_current('persistent' if era == 'both' else era)
        folder = 'Structures/' + s['zone']
        if kind == 'stairs':
            spawn_ramp(folder, s['id'], s['rect'], s['z0'], s['z1'], s['asc'])
        else:
            spawn_rect(folder, s['id'], s['rect'], s['z0'], s['z1'])
        counts[era] = counts.get(era, 0) + 1
    log('structures: %s' % counts)


def door_barrier_rect(d):
    """The opening of a door as a box BARRIER_THICK deep, centred on the wall line."""
    half_w = d['w'] * 0.5
    half_t = BARRIER_THICK * 0.5
    if d['axis'] == 'X':
        return [d['at'] - half_t, d['at'] + half_t, d['along'] - half_w, d['along'] + half_w]
    return [d['along'] - half_w, d['along'] + half_w, d['at'] - half_t, d['at'] + half_t]


def build_era_blocks(layout):
    """Door locks, pylons and the cryo tube: AGBEraBlock in the persistent level (era and flags on its component)."""
    has_block = hasattr(unreal, 'GBEraBlock')
    set_current('persistent')
    n = 0
    for d in layout['doors']:
        for i, lock in enumerate(d.get('lock') or []):
            rect = door_barrier_rect(d)
            # Several locks in one doorway sit a little apart along the wall normal, so each is its own body.
            offset = (i - (len(d['lock']) - 1) * 0.5) * (BARRIER_THICK + 10)
            if d['axis'] == 'X':
                rect = [rect[0] + offset, rect[1] + offset, rect[2], rect[3]]
            else:
                rect = [rect[0], rect[1], rect[2] + offset, rect[3] + offset]
            label = 'Lock_%s_%d_%s' % (d['id'], i, lock['kind'].replace(' ', '_').replace('(', '').replace(')', ''))
            n += place_block(has_block, 'Locks', label, rect, 0, d['h'], d['era'], None, lock.get('removedBy'), lock['kind'])
    for s in layout['structures']:
        if s['kind'] == 'feature' or s.get('removedBy'):
            n += place_block(has_block, 'Features', s['id'], s['rect'], s['z0'], s['z1'], s['era'], None, s.get('removedBy'), s.get('mesh'))
    log('era blocks: %d (%s)' % (n, 'AGBEraBlock' if has_block else 'static boxes in the era sub-level, no flags yet'))


# Era blocks: the look (and collision) of each lock kind. None = the engine cube. Paths are found by file name.
BLOCK_LOOKS = {
    'SM_Portcullis': ('SM_Portcullis', None),
    'rune ward': (None, 'MI_Hologram_6'),
    'rune-sealed plug': (None, 'MI_Hologram_7'),
    'barred door (2 x SM_Castle_Door)': ('SM_Castle_Door', None),
    'blast gate': ('SM_MGate01-500x350-1_GateL', None),
    'blast gate (second lock)': ('SM_MGate01-500x350-1_GateR', None),
    'door leaf': ('SM_Door_02', None),
    'lockdown shutter': ('SM_Door_02', 'MI_GB_WA_SciFiAccent'),
    'SM_Podium + rune glow': ('SM_Podium', None),
    'SciFi SM_LabCapsule11_1_Capsule + _Glass, scale 1.3': ('SM_LabCapsule11_1_Capsule', None),
}

_asset_index = None


def find_asset(short):
    """'/Game/...' object path of a package found by file name under Content."""
    global _asset_index
    if _asset_index is None:
        import os
        _asset_index = {}
        root = r'C:/GameJam/Glyphbound/Content'
        for dirpath, _, files in os.walk(root):
            if '__External' in dirpath:
                continue
            for f in files:
                if f.endswith('.uasset'):
                    rel = os.path.relpath(os.path.join(dirpath, f[:-7]), root).replace('\\', '/')
                    _asset_index.setdefault(f[:-7], '/Game/' + rel + '.' + f[:-7])
    path = _asset_index.get(short)
    return unreal.load_asset(path) if path else None


def fit_actor_to_rect(actor, mesh, rect, z0, z1):
    """Turns, scales and moves actor (whose root shows mesh) so the mesh bounds fill rect and z0..z1."""
    box = mesh.get_bounding_box()
    mn, mx = box.min, box.max
    sx, sy, sz = max(mx.x - mn.x, 1.0), max(mx.y - mn.y, 1.0), max(mx.z - mn.z, 1.0)
    x0, x1, y0, y1 = rect
    rx, ry, rz = x1 - x0, y1 - y0, max(z1 - z0, 1.0)
    yaw = 0.0 if ((rx >= ry) == (sx >= sy)) else 90.0
    lx, ly = (rx, ry) if yaw == 0.0 else (ry, rx)
    scale = (lx / sx, ly / sy, rz / sz)
    cxl, cyl = (mn.x + mx.x) * 0.5 * scale[0], (mn.y + mx.y) * 0.5 * scale[1]
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    ox, oy = cxl * c - cyl * s, cxl * s + cyl * c
    actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_location(unreal.Vector((x0 + x1) * 0.5 - ox, (y0 + y1) * 0.5 - oy, z0 - mn.z * scale[2]), False, False)


def place_block(has_block, folder, label, rect, z0, z1, era, required, removed, look=None):
    if has_block:
        actor = eas.spawn_actor_from_class(unreal.GBEraBlock, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        tag(actor, folder, label)
        set_era(actor, era, required, removed)
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh_short, mat_short = BLOCK_LOOKS.get(look or '', (None, None))
        mesh = find_asset(mesh_short) if mesh_short else None
        if mesh is not None:
            comp.set_static_mesh(mesh)
        else:
            mesh = comp.static_mesh
        if mat_short:
            mat = find_asset(mat_short)
            if mat:
                for i in range(comp.get_num_materials()):
                    comp.set_material(i, mat)
        fit_actor_to_rect(actor, mesh, rect, z0, z1)
        return 1
    # Fallback until the class exists: a static box in the era's sub-level (never removed).
    set_current(era if era in ('past', 'future') else 'persistent')
    spawn_rect(folder, label, rect, z0, z1)
    set_current('persistent')
    return 1


def build_gameplay(layout):
    set_current('persistent')
    start = next(s for s in layout['special'] if s['id'] == 'START')
    ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(start['pos'][0], start['pos'][1], start['pos'][2] + 100), unreal.Rotator(0, 0, start.get('yaw', 0)))
    tag(ps, 'Gameplay', 'PlayerStart')

    for p in layout['portals']:
        actor = eas.spawn_actor_from_class(unreal.GBPortal, unreal.Vector(*p['pos']), unreal.Rotator(0, 0, p.get('yaw', 0)))
        tag(actor, 'Portals', 'Portal_' + p['id'])
        set_era(actor, 'both', p.get('requiredFlag'), None)

    for e in layout['encounters']:
        trig = e.get('trigger')
        if trig:
            c = trig['c']
            extent = trig['e']
        else:
            spawns = e['spawns']
            c = [sum(s[0] for s in spawns) / len(spawns), sum(s[1] for s in spawns) / len(spawns), 0]
            extent = [1, 1, 1]   # started by interaction, never by its trigger
        actor = eas.spawn_actor_from_class(unreal.GBEncounter, unreal.Vector(*c), unreal.Rotator(0, 0, 0))
        tag(actor, 'Encounters', 'Encounter_' + e['id'])
        actor.set_editor_property('encounter_id', unreal.Name(e['id']))
        actor.set_editor_property('presence', presence_enum(e['era']))
        actor.set_editor_property('trigger_extent', unreal.Vector(*extent))
        actor.set_editor_property('spawn_points', [unreal.Vector(s[0] - c[0], s[1] - c[1], s[2] - c[2] + 100) for s in e['spawns']])
        if e.get('requires') and hasattr(actor, 'required_flags'):
            actor.set_editor_property('required_flags', [unreal.Name(f) for f in e['requires']])

    for cp in layout['checkpoints']:
        actor = eas.spawn_actor_from_class(unreal.GBCheckpointVolume, unreal.Vector(cp['pos'][0], cp['pos'][1], cp['pos'][2] + cp['extent'][2]), unreal.Rotator(0, 0, 0))
        tag(actor, 'Checkpoints', 'Checkpoint_' + cp['id'])
        box = actor.get_component_by_class(unreal.BoxComponent)
        if box:
            box.set_box_extent(unreal.Vector(*cp['extent']), True)

    build_interactables(layout)

    # Navigation: one bounds volume over the whole site (the navmesh is dynamic, built at runtime too).
    floor = layout['shell']['floor']['rect']
    x0, x1, y0, y1 = floor
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, 1200), unreal.Rotator(0, 0, 0))
    nav.set_actor_scale3d(unreal.Vector((x1 - x0 + 400) / 200.0, (y1 - y0 + 400) / 200.0, 2800 / 200.0))
    tag(nav, 'Gameplay', 'NavMeshBounds')

    ws = ues.get_editor_world().get_world_settings()
    ws.set_editor_property('kill_z', KILL_Z)


# Chest contents (provisional, WORLD_PLAN.md section 11). The Herald's rifle is its encounter's reward, so its crate
# only holds supplies.
CHEST_CONTENTS = {
    'chest_fire': [{'SpellGroup': 'Fire'}, {'WeaponId': 'Staff_Ember'}],
    'chest_lightning': [{'SpellGroup': 'Lightning'}],
    'chest_ice': [{'SpellGroup': 'Ice'}],
    'chest_air': [{'SpellGroup': 'Air'}],
    'crate_herald': [{'ItemId': 'Ammo_Magazine', 'Count': 2}, {'ItemId': 'HealthInjection'}],
    'crate_yard': [{'WeaponId': 'Shotgun'}, {'ItemId': 'HealthInjection'}],
}
ITEM_ALIASES = {'Grenade': 'ElectricGrenade'}


def make_drop(fields):
    drop = unreal.GBDropDefinition()
    drop.set_editor_property('item_id', unreal.Name(fields.get('ItemId', 'None')))
    drop.set_editor_property('weapon_id', unreal.Name(fields.get('WeaponId', 'None')))
    drop.set_editor_property('spell_group', unreal.Name(fields.get('SpellGroup', 'None')))
    drop.set_editor_property('count', int(fields.get('Count', 1)))
    drop.set_editor_property('chance', 1.0)
    return drop


def parse_pickup_items(text):
    """'Ammo_Magazine x2 + HealthInjection (before the Herald)' -> [('Ammo_Magazine', 2), ('HealthInjection', 1)]."""
    import re
    text = re.sub(r'\([^)]*\)', '', text)
    out = []
    for part in text.split('+'):
        words = part.split()
        if not words:
            continue
        name = ITEM_ALIASES.get(words[0], words[0])
        count = 1
        for w in words[1:]:
            if w.startswith('x') and w[1:].isdigit():
                count = int(w[1:])
        out.append((name, count))
    return out


def build_interactables(layout):
    special = {s['id']: s for s in layout['special']}
    seal = special['SEAL']
    actor = eas.spawn_actor_from_class(unreal.GBSeal, unreal.Vector(*seal['pos']), unreal.Rotator(0, 0, 0))
    tag(actor, 'Gameplay', 'Seal')
    final = special['FINAL']
    actor = eas.spawn_actor_from_class(unreal.GBFinalPortal, unreal.Vector(*final['pos']), unreal.Rotator(0, 0, 0))
    tag(actor, 'Gameplay', 'FinalPortal')

    encounters = {a.get_actor_label(): a for a in eas.get_all_level_actors() if isinstance(a, unreal.GBEncounter)}
    for sid, enc_id, era, prompt in (('CAGE', 'Valkyrie_Past', 'past', 'Free the Valkyrie'),
                                     ('TUBE', 'Valkyrie_Future', 'future', 'Release the cryo tube')):
        sp = special[sid]
        starter = eas.spawn_actor_from_class(unreal.GBEncounterStarter, unreal.Vector(*sp['pos']), unreal.Rotator(0, 0, 0))
        tag(starter, 'Gameplay', 'Starter_' + enc_id)
        starter.set_editor_property('encounter', encounters.get('Encounter_' + enc_id))
        starter.set_editor_property('prompt', prompt)
        set_era(starter, era, None, None)

    for c in layout['chests']:
        actor = eas.spawn_actor_from_class(unreal.GBChest, unreal.Vector(*c['pos']), unreal.Rotator(0, 0, 0))
        tag(actor, 'Chests', c['id'])
        actor.set_editor_property('contents', [make_drop(f) for f in CHEST_CONTENTS.get(c['id'], [])])
        actor.set_editor_property('opened_flag', unreal.Name('Chest.%s.Opened' % c['id']))
        set_era(actor, c['era'], c.get('requiredFlag'), None)

    n = 0
    for p in layout['pickups']:
        for i, (item, count) in enumerate(parse_pickup_items(p['item'])):
            pos = unreal.Vector(p['pos'][0] + 70 * i, p['pos'][1], p['pos'][2] + 40)
            actor = eas.spawn_actor_from_class(unreal.GBPickup, pos, unreal.Rotator(0, 0, 0))
            tag(actor, 'Pickups', '%s_%d' % (p['id'], i))
            actor.set_editor_property('drop', make_drop({'ItemId': item, 'Count': count}))
            set_era(actor, p['era'], None, None)
            n += 1
    log('interactables: seal, final portal, 2 starters, %d chests, %d pickups' % (len(layout['chests']), n))


def build_lighting():
    """Per era: sun, sky light, atmosphere, fog. Past: late afternoon; future: night (provisional, plan section 11)."""
    for era, pitch, intensity, color, fog in (
            ('past', -25.0, 6.0, unreal.LinearColor(1.0, 0.82, 0.62, 1.0), 0.02),
            ('future', -60.0, 0.6, unreal.LinearColor(0.55, 0.65, 1.0, 1.0), 0.035)):
        set_current(era)
        sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(roll=0, pitch=pitch, yaw=35))
        tag(sun, 'Lighting', era.capitalize() + '_Sun')
        light = sun.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_editor_property('intensity', intensity)
        light.set_editor_property('light_color', unreal.Color(r=int(color.r * 255), g=int(color.g * 255), b=int(color.b * 255), a=255))
        light.set_editor_property('atmosphere_sun_light', True)
        sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 2800), unreal.Rotator(0, 0, 0))
        tag(sky, 'Lighting', era.capitalize() + '_SkyLight')
        sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
        sky_comp.set_editor_property('real_time_capture', True)
        sky_comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
        atmo = eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        tag(atmo, 'Lighting', era.capitalize() + '_Atmosphere')
        fog_actor = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        tag(fog_actor, 'Lighting', era.capitalize() + '_Fog')
        fog_actor.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property('fog_density', fog)


def build_room_lights(layout):
    """Greybox work lights: a grid of shadowless point lights under each ceiling (and in the future's open yard at
    night), so every room is readable before the real lighting pass. Warm torchlight in the past, cool in the future."""
    old = [a for a in eas.get_all_level_actors() if '_Work_' in a.get_actor_label() and TAG in [str(t) for t in a.tags]]
    if old:
        eas.destroy_actors(old)
    n = 0
    for room in layout['rooms']:
        for era in ('past', 'future'):
            if room['eras'] not in ('both', era):
                continue
            ceil = room.get('ceilPast' if era == 'past' else 'ceilFuture') or 0
            if ceil <= 0 and era == 'past':
                continue   # open sky in daylight: the sun and the sky light are enough
            rect = room.get('futureRect') if era == 'future' and room.get('futureRect') else room['rect']
            x0, x1, y0, y1 = rect
            z = (ceil - 150) if ceil > 0 else 900
            z = max(min(z, 1100), 250)
            nx = max(1, int(round((x1 - x0) / 1600.0)))
            ny = max(1, int(round((y1 - y0) / 1600.0)))
            set_current(era)
            for ix in range(nx):
                for iy in range(ny):
                    px = x0 + (ix + 0.5) * (x1 - x0) / nx
                    py = y0 + (iy + 0.5) * (y1 - y0) / ny
                    light = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(px, py, z), unreal.Rotator(0, 0, 0))
                    tag(light, 'Lighting/Work', '%s_Work_%s_%d_%d' % (era.capitalize(), room['id'], ix, iy))
                    comp = light.get_component_by_class(unreal.PointLightComponent)
                    comp.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
                    comp.set_editor_property('intensity', 400.0 if era == 'past' else 300.0)
                    comp.set_editor_property('attenuation_radius', 3000.0)
                    comp.set_editor_property('cast_shadows', False)
                    comp.set_editor_property('light_color', unreal.Color(r=255, g=196, b=140, a=255) if era == 'past' else unreal.Color(r=170, g=205, b=255, a=255))
                    n += 1
    log('work lights: %d' % n)


def save_all():
    les.save_all_dirty_levels()


def run(phases=('shell', 'ceilings', 'structures', 'blocks', 'gameplay', 'lighting')):
    layout = load_layout()
    ensure_maps()
    clear_generated()
    if 'shell' in phases:
        build_shell(layout)
    if 'ceilings' in phases:
        build_ceilings(layout)
    if 'structures' in phases:
        build_structures(layout)
    if 'blocks' in phases:
        build_era_blocks(layout)
    if 'gameplay' in phases:
        build_gameplay(layout)
    if 'lighting' in phases:
        build_lighting()
        build_room_lights(layout)
    set_current('persistent')
    save_all()
    counts = {}
    for a in eas.get_all_level_actors():
        if TAG in [str(t) for t in a.tags]:
            k = level_key_of(a)
            counts[k] = counts.get(k, 0) + 1
    log('generated actors per level: %s' % counts)
    return '\n'.join(log_lines)


if __name__ == '__main__' or globals().get('GB_RUN_SITE_BUILD', True):
    result = run()
    print(result)
