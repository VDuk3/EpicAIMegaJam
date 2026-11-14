# Dress-SiteMap.py - puts the kits' look on the site greybox built by Build-SiteMap.py (run that first).
# Runs inside the Unreal Editor (Python), e.g. through the MCP Python tool:
#   g = {'unreal': unreal, '__name__': 'gb_dress'}; exec(open(r'C:/GameJam/Tools/Jam/Dress-SiteMap.py').read(), g)
#   g['run']()            # or g['run'](('materials',)) etc.
#
# Rules (architect, 2026-11-13, from Design/Levels/WORLD_PLAN.md sections 3, 6 and 8):
# - Collision stays on the greybox boxes. A box that gets a kit look is hidden (visibility off, collision kept); the
#   kit meshes are placed WITHOUT collision, so footing and the navmesh never depend on a kit's own collision
#   (two kit pieces close their own doorways with one hull, the HTB bridge is solid underneath).
# - Boxes that stay as the look (future walls, floors, ceilings, containers; castle ceilings and galleries) get a
#   world-aligned material, so a 48 m wall does not stretch one texture over its face.
# - Every actor made here carries the tag GBDress; a run deletes the GBDress actors and re-places them, and resets the
#   greybox boxes' visibility first. Build-SiteMap.py's GBGen actors are never deleted here.

import json
import math
import os
import unreal

LAYOUT_PATH = r'C:/GameJam/Design/Levels/world_layout.json'
CONTENT_DIR = r'C:/GameJam/Glyphbound/Content'
TAG = 'GBDress'
GEN_TAG = 'GBGen'
MAT_DIR = '/Game/Glyphbound/Materials/World'
WA_MASTER = MAT_DIR + '/M_GB_WorldAligned'
WA_TEXTURE_FN = '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture.WorldAlignedTexture'
WA_NORMAL_FN = '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal.WorldAlignedNormal'

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
mel = unreal.MaterialEditingLibrary
log_lines = []


def log(msg):
    log_lines.append(msg)
    unreal.log('[Dress-SiteMap] ' + msg)


def load_layout():
    with open(LAYOUT_PATH, 'r', encoding='utf-8') as f:
        return json.load(f)


# ---------------------------------------------------------------- assets by short name

_paths = None


def asset_path(short):
    """'/Game/...' path of a package found by its file name under the kits we use."""
    global _paths
    if _paths is None:
        _paths = {}
        for kit in ('CastleTown', 'High_Tech_Base', 'SciFiWorld', 'MaterialsScifi'):
            root = os.path.join(CONTENT_DIR, kit)
            for dirpath, _, files in os.walk(root):
                for f in files:
                    if f.endswith('.uasset'):
                        rel = os.path.relpath(os.path.join(dirpath, f[:-7]), CONTENT_DIR).replace('\\', '/')
                        _paths.setdefault(f[:-7], '/Game/' + rel)
    return _paths.get(short)


_loaded = {}


def load(short_or_path):
    path = short_or_path if short_or_path.startswith('/') else asset_path(short_or_path)
    if not path:
        log('WARNING: no asset named ' + short_or_path)
        return None
    if path not in _loaded:
        _loaded[path] = unreal.load_asset(path)
    return _loaded[path]


# ---------------------------------------------------------------- materials

WA_SETS = {
    # name: (basecolor, normal, orm, emissive or None, texture size cm, tint)
    'CastleStone': ('T_StoneWall_A_basecolor', 'T_StoneWall_A_normal', 'T_StoneWall_A_ARM', None, 400.0),
    'CastleWood': ('T_Wood_02_basecolor', 'T_Wood_02_normal', 'T_Wood_02_ORMH', None, 300.0),
    'CastleFloor': ('T_StoneDamage_basecolor', 'T_StoneDamage_normal', 'T_StoneDamage_ARM', None, 300.0),
    'SciFiWall': ('T_Scifi_Panels_10_basecolor', 'T_Scifi_Panels_10_normal', 'T_Scifi_Panels_10_ORMH', None, 400.0),
    'SciFiFloor': ('T_Scifi_Panels_09_basecolor', 'T_Scifi_Panels_09_normal', 'T_Scifi_Panels_09_ORMH', None, 300.0),
    'SciFiCeiling': ('T_Scifi_Panels_07_basecolor', 'T_Scifi_Panels_07_normal', 'T_Scifi_Panels_07_ORMH', None, 400.0),
    'SciFiAccent': ('T_Scifi_Panels_11_basecolor', 'T_Scifi_Panels_11_normal', 'T_Scifi_Panels_11_ORMH', 'T_Scifi_Panels_11_emissive', 300.0),
    'SciFiContainer': ('T_Scifi_Panels_12_basecolor', 'T_Scifi_Panels_12_normal', 'T_Scifi_Panels_12_ORMH', None, 250.0),
}


def ensure_world_aligned_master():
    if unreal.EditorAssetLibrary.does_asset_exist(WA_MASTER):
        return unreal.load_asset(WA_MASTER)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset('M_GB_WorldAligned', MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    size = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 0)
    size.set_editor_property('parameter_name', 'TextureSize')
    size.set_editor_property('default_value', 400.0)
    ones = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -1400, 120)
    ones.set_editor_property('constant', unreal.LinearColor(1, 1, 1, 1))
    size3 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1200, 60)
    mel.connect_material_expressions(size, '', size3, 'A')
    mel.connect_material_expressions(ones, '', size3, 'B')

    def tex_param(name, default_short, y):
        p = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -1000, y)
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('texture', load(default_short))
        return p

    def wa_call(fn_path, tex, y):
        call = mel.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -700, y)
        call.set_material_function(unreal.load_asset(fn_path))
        mel.connect_material_expressions(tex, '', call, 'TextureObject')
        mel.connect_material_expressions(size3, '', call, 'TextureSize')
        return call

    base = tex_param('BaseColorTex', 'T_StoneWall_A_basecolor', -300)
    base_wa = wa_call(WA_TEXTURE_FN, base, -300)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -450)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    base_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -350)
    mel.connect_material_expressions(base_wa, 'XYZ Texture', base_mul, 'A')
    mel.connect_material_expressions(tint, '', base_mul, 'B')
    mel.connect_material_property(base_mul, '', unreal.MaterialProperty.MP_BASE_COLOR)

    normal = tex_param('NormalTex', 'T_StoneWall_A_normal', 0)
    normal_wa = wa_call(WA_NORMAL_FN, normal, 0)
    mel.connect_material_property(normal_wa, 'XYZ Texture', unreal.MaterialProperty.MP_NORMAL)

    orm = tex_param('ORMTex', 'T_StoneWall_A_ARM', 300)
    orm_wa = wa_call(WA_TEXTURE_FN, orm, 300)
    for channel, prop, y in (('R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 250),
                             ('G', unreal.MaterialProperty.MP_ROUGHNESS, 350),
                             ('B', unreal.MaterialProperty.MP_METALLIC, 450)):
        mask = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -400, y)
        mask.set_editor_property('r', channel == 'R')
        mask.set_editor_property('g', channel == 'G')
        mask.set_editor_property('b', channel == 'B')
        mask.set_editor_property('a', False)
        mel.connect_material_expressions(orm_wa, 'XYZ Texture', mask, '')
        mel.connect_material_property(mask, '', prop)

    emis = tex_param('EmissiveTex', 'T_Default_BaseColor', 650)
    emis_wa = wa_call(WA_TEXTURE_FN, emis, 650)
    strength = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 820)
    strength.set_editor_property('parameter_name', 'EmissiveStrength')
    strength.set_editor_property('default_value', 0.0)
    emis_mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 700)
    mel.connect_material_expressions(emis_wa, 'XYZ Texture', emis_mul, 'A')
    mel.connect_material_expressions(strength, '', emis_mul, 'B')
    mel.connect_material_property(emis_mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    log('created ' + WA_MASTER)
    return mat


_mis = {}


def wa_material(name):
    if name in _mis:
        return _mis[name]
    path = MAT_DIR + '/MI_GB_WA_' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        master = ensure_world_aligned_master()
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mi = tools.create_asset('MI_GB_WA_' + name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        bc, nm, orm, em, size = WA_SETS[name]
        mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTex', load(bc))
        mel.set_material_instance_texture_parameter_value(mi, 'NormalTex', load(nm))
        mel.set_material_instance_texture_parameter_value(mi, 'ORMTex', load(orm))
        mel.set_material_instance_scalar_parameter_value(mi, 'TextureSize', size)
        if em:
            mel.set_material_instance_texture_parameter_value(mi, 'EmissiveTex', load(em))
            mel.set_material_instance_scalar_parameter_value(mi, 'EmissiveStrength', 6.0)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        log('created MI_GB_WA_' + name)
    _mis[name] = mi
    return mi


# ---------------------------------------------------------------- levels and actors

def set_current(key):
    les.set_current_level_by_name({'persistent': 'Site', 'past': 'Site_Past', 'future': 'Site_Future'}[key])


def level_key_of(actor):
    path = actor.get_path_name()
    for key, marker in (('past', '/Site_Past.'), ('future', '/Site_Future.'), ('persistent', '/Site.')):
        if marker in path:
            return key
    return None


def tags_of(actor):
    return [str(t) for t in actor.tags]


_gen = None


def gen_actor(key, label):
    """The GBGen greybox actor of a level by its label."""
    global _gen
    if _gen is None:
        _gen = {}
        for a in eas.get_all_level_actors():
            if GEN_TAG in tags_of(a):
                _gen[(level_key_of(a), a.get_actor_label())] = a
    return _gen.get((key, label))


def hide_box(actor):
    if actor:
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_visibility(False, True)
        comp.set_editor_property('cast_shadow', False)


def skin_box(actor, material_name):
    if actor:
        actor.get_component_by_class(unreal.StaticMeshComponent).set_material(0, wa_material(material_name))


def reset():
    doomed = [a for a in eas.get_all_level_actors() if TAG in tags_of(a)]
    if doomed:
        eas.destroy_actors(doomed)
    n = 0
    for a in eas.get_all_level_actors():
        if GEN_TAG in tags_of(a) and isinstance(a, unreal.StaticMeshActor):
            comp = a.static_mesh_component
            comp.set_visibility(True, True)
            comp.set_editor_property('cast_shadow', True)
            n += 1
    log('reset: removed %d dress actors, %d greybox boxes visible again' % (len(doomed), n))


def rot2(x, y, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return x * c - y * s, x * s + y * c


def spawn_mesh(key, folder, label, mesh, location, yaw=0.0, scale=(1.0, 1.0, 1.0), material=None):
    set_current(key)
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('can_character_step_up_on', unreal.CanBeCharacterBase.ECB_NO)
    comp.set_editor_property('can_ever_affect_navigation', False)
    if material:
        for i in range(comp.get_num_materials()):
            comp.set_material(i, material)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_editor_property('tags', [unreal.Name(TAG)])
    actor.set_folder_path(unreal.Name('Dress/' + folder))
    actor.set_actor_label(label)
    return actor


def mesh_bounds(mesh):
    box = mesh.get_bounding_box()
    return box.min, box.max


def place_fit(key, folder, label, short, rect, z0, z1, yaw=None, stretch=True, material=None):
    """Places a mesh so its bounds fill rect (x0, x1, y0, y1) and z0..z1. yaw None = turn it so the mesh's longer
    side runs along the rect's longer side. stretch False = scale 1 (uniform down-scale only if it does not fit)."""
    mesh = load(short)
    if mesh is None:
        return None
    mn, mx = mesh_bounds(mesh)
    sx, sy, sz = mx.x - mn.x, mx.y - mn.y, mx.z - mn.z
    x0, x1, y0, y1 = rect
    rx, ry, rz = x1 - x0, y1 - y0, max(z1 - z0, 1.0)
    if yaw is None:
        yaw = 0.0 if ((rx >= ry) == (sx >= sy)) else 90.0
    along_x = abs(math.cos(math.radians(yaw))) > 0.5
    lx, ly = (rx, ry) if along_x else (ry, rx)
    if stretch:
        scale = (lx / max(sx, 1.0), ly / max(sy, 1.0), rz / max(sz, 1.0))
    else:
        s = min(1.0, lx / max(sx, 1.0), ly / max(sy, 1.0), rz / max(sz, 1.0))
        scale = (s, s, s)
    cx_local = (mn.x + mx.x) * 0.5 * scale[0]
    cy_local = (mn.y + mx.y) * 0.5 * scale[1]
    ox, oy = rot2(cx_local, cy_local, yaw)
    loc = unreal.Vector((x0 + x1) * 0.5 - ox, (y0 + y1) * 0.5 - oy, z0 - mn.z * scale[2])
    return spawn_mesh(key, folder, label, mesh, loc, yaw, scale, material)


def place_tiled(key, folder, label, short, rect, z0, z1, count=None, material=None):
    """Splits rect along its long side into count pieces (by default as many as the mesh's own length fits) and fits
    the mesh in each."""
    mesh = load(short)
    if mesh is None:
        return 0
    mn, mx = mesh_bounds(mesh)
    piece = max(mx.x - mn.x, mx.y - mn.y)
    x0, x1, y0, y1 = rect
    along_x = (x1 - x0) >= (y1 - y0)
    length = (x1 - x0) if along_x else (y1 - y0)
    n = count or max(1, int(round(length / max(piece, 1.0))))
    for i in range(n):
        if along_x:
            sub = [x0 + length * i / n, x0 + length * (i + 1) / n, y0, y1]
        else:
            sub = [x0, x1, y0 + length * i / n, y0 + length * (i + 1) / n]
        place_fit(key, folder, '%s_%d' % (label, i), short, sub, z0, z1, None, True, material)
    return n


def place_stacked(key, folder, label, short, rect, z0, z1, count):
    for i in range(count):
        h = (z1 - z0) / count
        place_fit(key, folder, '%s_%d' % (label, i), short, rect, z0 + h * i, z0 + h * (i + 1))
    return count


STAIR_YAW = {'+Y': 0.0, '-Y': 180.0, '+X': -90.0, '-X': 90.0}   # the castle stairs climb towards local +Y


# ---------------------------------------------------------------- the past (castle)

def dress_past_shell(layout):
    for i, p in enumerate(layout['castleWallPieces']):
        mesh = load(p['mesh'])
        o = p['origin']
        spawn_mesh('past', 'Shell', 'Wall_%02d_%s' % (i, p.get('door') or 'plain'), mesh, unreal.Vector(o[0], o[1], o[2]), p['yaw'])
    for a in eas.get_all_level_actors():
        if GEN_TAG in tags_of(a) and level_key_of(a) == 'past':
            label = a.get_actor_label()
            if label.startswith('Past_Wall_') or label.startswith('Past_Lintel_'):
                hide_box(a)
            elif label.startswith('Past_Ceiling_'):
                skin_box(a, 'CastleWood')
    log('past shell: %d wall pieces' % len(layout['castleWallPieces']))


def castle_floor_cells(layout):
    rects = [r['rect'] for r in layout['rooms'] if r['eras'] in ('both', 'past')]
    cells = []
    for ix in range(-4, 7):
        for iy in range(-5, 5):
            cx, cy = ix * 1200 + 600 - 400, iy * 1200 + 600
            if any(r[0] - 600 < cx < r[1] + 600 and r[2] - 600 < cy < r[3] + 600 for r in rects):
                cells.append((cx, cy))
    return cells


def skin_floor(era, layout, material_name):
    """A thin skinned box per room of the era, its top 1 cm above the shared slab. (The castle kit's 12 m floor tile,
    SM_CastleTower_Floor_A_05, has round holes in it: it is a tower floor, not a hall floor.)"""
    cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
    n = 0
    for room in layout['rooms']:
        if room['eras'] not in ('both', era):
            continue
        rect = room.get('futureRect') if era == 'future' and room.get('futureRect') else room['rect']
        x0, x1, y0, y1 = rect
        spawn_mesh(era, 'Floor', 'FloorSkin_' + room['id'], cube, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, -4.0),
                   0.0, ((x1 - x0) / 100.0, (y1 - y0) / 100.0, 0.1), wa_material(material_name))
        n += 1
    return n


def dress_past_floor(layout):
    log('past floor: %d room skins' % skin_floor('past', layout, 'CastleFloor'))


def dress_structure_past(s):
    m = s.get('mesh', '')
    if m.startswith('past:'):
        m = m.split(';')[0][5:].strip()
    rect, z0, z1, sid = s['rect'], s['z0'], s['z1'], s['id']
    key = 'past'
    box_key = 'persistent' if s['era'] == 'both' else 'past'
    box = gen_actor(box_key, sid)
    folder = 'Structures/' + s['zone']
    if s['kind'] == 'stairs':
        place_fit(key, folder, sid, 'SM_Stairs_8M' if 'SM_Stairs_8M' in m else 'SM_Stairs_4M', rect, z0, z1, STAIR_YAW[s['asc']])
        hide_box(box)
    elif 'SM_PillarStem_01' in m:
        stem, top = load('SM_PillarStem_01'), load('SM_PillarTop_01')
        cx, cy = (rect[0] + rect[1]) * 0.5, (rect[2] + rect[3]) * 0.5
        spawn_mesh(key, folder, sid + '_stem', stem, unreal.Vector(cx, cy, z0 - 18.0))
        spawn_mesh(key, folder, sid + '_top', top, unreal.Vector(cx - 55.0, cy + 55.0, z0 + 965.0))
        hide_box(box)
    elif 'SM_Podium' in m:
        place_fit(key, folder, sid, 'SM_Podium', rect, z0, z1)
        hide_box(box)
    elif 'SM_Short_Walls_Wall_2M' in m:
        place_tiled(key, folder, sid, 'SM_Short_Walls_Wall_2M', rect, z0, z1, 3)
        hide_box(box)
    elif 'SM_Bookshelf_A_01' in m:
        place_tiled(key, folder, sid, 'SM_Bookshelf_A_01', rect, z0, z1, 3)
        hide_box(box)
    elif 'SM_HallTable_01' in m:
        place_fit(key, folder, sid, 'SM_HallTable_01', rect, z0, z1)
        hide_box(box)
    elif 'SM_Crenel_A_01' in m:
        place_tiled(key, folder, sid, 'SM_Crenel_A_01', rect, z0, z1)
        hide_box(box)
    elif s['kind'] == 'prop':
        short = m.split()[0]
        place_fit(key, folder, sid, short, rect, z0, z1, None, False)
    elif m.startswith('BOX'):
        if s['era'] == 'both':
            # The shared box lives in the persistent level: hide it, each era gets its own skinned copy.
            hide_box(box)
            x0, x1, y0, y1 = rect
            spawn_mesh(key, folder, sid + '_skin', unreal.load_asset('/Engine/BasicShapes/Cube.Cube'),
                       unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), 0.0,
                       ((x1 - x0) / 100.0, (y1 - y0) / 100.0, (z1 - z0) / 100.0), wa_material('CastleStone'))
        else:
            skin_box(box, 'CastleStone')
        if 'floor tiles' in m or 'Floor_A_01' in m:
            tile = load('SM_CastleTower_Floor_A_01')
            nx, ny = max(1, int(round((rect[1] - rect[0]) / 400.0))), max(1, int(round((rect[3] - rect[2]) / 400.0)))
            for ix in range(nx):
                for iy in range(ny):
                    sub = [rect[0] + (rect[1] - rect[0]) * ix / nx, rect[0] + (rect[1] - rect[0]) * (ix + 1) / nx,
                           rect[2] + (rect[3] - rect[2]) * iy / ny, rect[2] + (rect[3] - rect[2]) * (iy + 1) / ny]
                    place_fit(key, folder, '%s_tile_%d_%d' % (sid, ix, iy), 'SM_CastleTower_Floor_A_01', sub, z1 - 49.0, z1 + 1.0, 0.0)
        if 'SM_FirePit_01' in m:
            place_fit(key, folder, sid + '_firepit', 'SM_FirePit_01', [rect[0], rect[0] + 90, rect[2], rect[2] + 90], z1, z1 + 22, None, False)
            place_fit(key, folder, sid + '_cauldron', 'SM_ConicalCauldron_02', [(rect[0] + rect[1]) / 2 - 60, (rect[0] + rect[1]) / 2 + 60, (rect[2] + rect[3]) / 2 - 60, (rect[2] + rect[3]) / 2 + 60], z1, z1 + 144, None, False)


# ---------------------------------------------------------------- the future (base)

def dress_future_shell(layout):
    n = 0
    for a in eas.get_all_level_actors():
        if GEN_TAG in tags_of(a) and level_key_of(a) == 'future':
            label = a.get_actor_label()
            if label.startswith('Future_Wall_') or label.startswith('Future_Lintel_'):
                skin_box(a, 'SciFiWall')
                n += 1
            elif label.startswith('Future_Ceiling_'):
                skin_box(a, 'SciFiCeiling')
    skin_floor('future', layout, 'SciFiFloor')
    log('future shell: %d wall boxes skinned' % n)


def dress_structure_future(s):
    m = s.get('mesh', '')
    if 'future:' in m:
        m = m.split('future:')[1].strip()
    rect, z0, z1, sid = s['rect'], s['z0'], s['z1'], s['id']
    key = 'future'
    box_key = 'persistent' if s['era'] == 'both' else 'future'
    box = gen_actor(box_key, sid)
    folder = 'Structures/' + s['zone']
    if s['era'] == 'both' and box is not None:
        # The shared box is in the persistent level: hide it and give the future its own skinned copy of the shape.
        if 'wedge' in m:
            t = box.get_actor_transform()
            spawn_mesh(key, folder, sid + '_skin', unreal.load_asset('/Engine/BasicShapes/Cube.Cube'), t.translation,
                       t.rotation.rotator().yaw, (t.scale3d.x, t.scale3d.y, t.scale3d.z), wa_material('SciFiFloor'))
            # The ramp's pitch is part of the rotation, not only the yaw: copy the whole rotation.
            last = [a for a in eas.get_all_level_actors() if a.get_actor_label() == sid + '_skin' and TAG in tags_of(a)]
            for a in last:
                a.set_actor_rotation(t.rotation.rotator(), False)
        elif 'BOX' in m:
            mesh = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
            x0, x1, y0, y1 = rect
            spawn_mesh(key, folder, sid + '_skin', mesh, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), 0.0,
                       ((x1 - x0) / 100.0, (y1 - y0) / 100.0, (z1 - z0) / 100.0), wa_material('SciFiWall'))
    if 'SM_Prop_HS' in m:
        place_fit(key, folder, sid, 'SM_Prop_HS', rect, z0, z1)
        hide_box(box)
    elif 'SM_Prop_03' in m:
        place_fit(key, folder, sid, 'SM_Prop_03', rect, z0, z1)
        hide_box(box)
    elif 'SM_Hero_Floor_01' in m:
        place_tiled(key, folder, sid, 'SM_Hero_Floor_01', rect, z0, z1)
        hide_box(box)
    elif 'SM_Bridge_01' in m:
        place_tiled(key, folder, sid, 'SM_Bridge_01', rect, z0, z1, 10)
        hide_box(box)
    elif 'SM_Wall_03' in m:
        place_stacked(key, folder, sid, 'SM_Wall_03', rect, z0, z1, 2)
        if s['era'] != 'both':
            hide_box(box)
    elif 'SM_LabCapsule11' in m:
        place_fit(key, folder, sid + '_capsule', 'SM_LabCapsule11_1_Capsule', rect, z0, z1, None, True)
        place_fit(key, folder, sid + '_glass', 'SM_LabCapsule11_1_Glass', rect, z0, z1, None, True)
    elif 'SM_LabCapsule05' in m:
        place_fit(key, folder, sid + '_capsule', 'SM_LabCapsule05_Capsule', rect, z0, z1, None, False)
        place_fit(key, folder, sid + '_glass', 'SM_LabCapsule05_Glass', rect, z0, z1, None, False)
    elif 'SM_ControlPanel01' in m:
        place_fit(key, folder, sid, 'SM_ControlPanel01', rect, z0, z1, None, False)
    elif 'SM_LabBarrels01_1' in m:
        place_tiled(key, folder, sid, 'SM_LabBarrels01_1', rect, z0, z1, 3)
        hide_box(box)
    elif m.startswith('BOX'):
        if s['era'] == 'both':
            return
        if 'container' in m:
            skin_box(box, 'SciFiContainer')
        elif 'emissive' in m:
            skin_box(box, 'SciFiAccent')
        else:
            skin_box(box, 'SciFiWall')


def dress_structures(layout):
    n = 0
    for s in layout['structures']:
        if s.get('sealed') or s['kind'] == 'feature' or s.get('removedBy'):
            continue
        if s['era'] in ('past', 'both'):
            dress_structure_past(s)
            n += 1
        if s['era'] in ('future', 'both'):
            dress_structure_future(s)
            n += 1
    log('structures dressed: %d' % n)


def hide_slab():
    slab = gen_actor('persistent', 'Floor_Slab')
    hide_box(slab)


def run(phases=('materials', 'past_shell', 'past_floor', 'future_shell', 'structures')):
    layout = load_layout()
    les.load_level('/Game/Glyphbound/Maps/Site')
    reset()
    if 'materials' in phases:
        for name in WA_SETS:
            wa_material(name)
    if 'past_shell' in phases:
        dress_past_shell(layout)
    if 'past_floor' in phases:
        dress_past_floor(layout)
        hide_slab()
    if 'future_shell' in phases:
        dress_future_shell(layout)
    if 'structures' in phases:
        dress_structures(layout)
    set_current('persistent')
    les.save_all_dirty_levels()
    counts = {}
    for a in eas.get_all_level_actors():
        if TAG in tags_of(a):
            k = level_key_of(a)
            counts[k] = counts.get(k, 0) + 1
    log('dress actors per level: %s' % counts)
    return '\n'.join(log_lines)
