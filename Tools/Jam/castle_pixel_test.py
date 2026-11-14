# castle_pixel_test.py - the pixel-art test of the castle's first zone (owner, 2026-11-14: "pixel-art test, only in the
# first zone"). The zone is the gatehouse and the lower ward: everything of the map before the upper ward's edge.
# Run in the Editor, on the map PastCastle: node Tools/Jam/mcp_call.js py Tools/Jam/castle_pixel_test.py
# Settings: Glyphbound/Saved/castle_pixel.json {"variant": "tiles" | "uv" | "both_sides", "res": 64, "steps": 6}
#
# Two ways to make the pack's photographic stone pixel art, so the owner can choose:
#   tiles  the game's own way for structures of the past (GDD 3, the test arena): pixel tiles drawn for the castle
#          (Tools/Jam/make_castle_tiles.js), laid in world space by M_PixelSurface, one tile per kind of material.
#   uv     the pack's own pictures, cut down to large pixels on the piece's own UVs and to a few steps of colour
#          (M_GBPixelUV, built here). The drawn stones stay where the modelled stones are.
#   both_sides  tiles on the King's Hall side of the axis (Y < 0), uv on the other: both in one look at the ward.
# Nothing of the pack is changed: every piece of the zone gets materials of ours as overrides.
import json
import math
import os

import unreal

CFG_PATH = os.path.join(unreal.Paths.project_saved_dir(), 'castle_pixel.json')
CFG = json.load(open(CFG_PATH)) if os.path.exists(CFG_PATH) else {}
VARIANT = CFG.get('variant', 'tiles')
RES = float(CFG.get('res', 64))
STEPS = float(CFG.get('steps', 6))
ZONE_X = 2900.0
STAGES = ('stage_gate', 'stage_walls', 'stage_wards', 'stage_camp', 'stage_ground')
DIR = '/Game/Glyphbound/Materials/CastlePixel'
TILE_SRC = 'C:/GameJam/Glyphbound/SourceArt/PixelArt/CastleTiles/'
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = []

# tile -> centimetres of world one tile covers (32 px): 16 px to the metre for stone, a little finer for wood and iron
TILES = {'castle_wall': 200.0, 'castle_dressed': 200.0, 'castle_dark': 200.0, 'castle_cobble': 200.0, 'castle_wood': 160.0,
         'castle_iron': 160.0, 'castle_earth': 400.0, 'castle_cloth': 120.0}


def tile_for(name):
    """Which tile a material of the pack becomes, by what it is."""
    n = name.lower()
    if any(k in n for k in ('wood', 'plank', 'varnish')):
        return 'castle_wood'
    if any(k in n for k in ('metal', 'iron')):
        return 'castle_iron'
    if 'cobble' in n:
        return 'castle_cobble'
    if any(k in n for k in ('cloth', 'sack', 'rope')):
        return 'castle_cloth'
    if any(k in n for k in ('ground', 'landscape', 'basicshape', 'worldgrid')):
        return 'castle_earth'
    if any(k in n for k in ('brickkit', 'slab', 'stairs', 'floor')):
        return 'castle_dressed'
    if any(k in n for k in ('stonefence', 'rock', 'firepit')):
        return 'castle_dark'
    return 'castle_wall'


def import_tiles():
    for name in TILES:
        task = unreal.AssetImportTask()
        task.filename = TILE_SRC + name + '.png'
        task.destination_path = DIR
        task.destination_name = 'T_' + name
        task.replace_existing = True
        task.automated = True
        task.save = False
        tools.import_asset_tasks([task])
        t = unreal.load_asset(DIR + '/T_' + name)
        t.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
        t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        t.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
        t.set_editor_property('srgb', True)
        EAL.save_asset(DIR + '/T_' + name, only_if_is_dirty=False)


def instance(name, parent):
    path = DIR + '/' + name
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if mi.get_editor_property('parent') != parent:
        MEL.set_material_instance_parent(mi, parent)
    return mi


def tile_instances():
    parent = unreal.load_asset('/Game/Glyphbound/Materials/M_PixelSurface')
    out = {}
    for name, size in TILES.items():
        mi = instance('MI_Pix_' + name, parent)
        MEL.set_material_instance_texture_parameter_value(mi, 'Texture', unreal.load_asset(DIR + '/T_' + name))
        MEL.set_material_instance_scalar_parameter_value(mi, 'TileSize', size)
        MEL.update_material_instance(mi)
        EAL.save_asset(DIR + '/MI_Pix_' + name, only_if_is_dirty=False)
        out[name] = mi
    return out


def small_copy(tex):
    """A copy of one of the pack's pictures cut down to RES pixels: not a virtual texture, not filtered, no mips. It
    is the large-pixel version of that picture, and it weighs almost nothing in the package."""
    name = 'T_px%d_%s' % (int(RES), tex.get_name())
    path = DIR + '/' + name
    if not EAL.does_asset_exist(path):
        EAL.duplicate_asset(tex.get_path_name().split('.')[0], path)
        t = unreal.load_asset(path)
        t.set_editor_property('virtual_texture_streaming', False)
        t.set_editor_property('max_texture_size', int(RES))
        t.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
        t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property('never_stream', True)
        EAL.save_asset(path, only_if_is_dirty=False)
    return unreal.load_asset(path)


def build_uv_master():
    """M_GBPixelUV: the piece's own picture in large pixels, on the piece's own UVs. The picture is a small copy
    (small_copy) read without filtering. A second small picture (occlusion, red channel) darkens it: the pack's stone
    has its joints there, not in its colour. Then the brightness is cut to Steps levels (in gamma space, so the steps
    look even) and the colour is scaled to it, which keeps the hue: cutting each channel on its own turned near-grey
    stone into speckles of yellow, purple and green."""
    path = DIR + '/M_GBPixelUV'
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset('M_GBPixelUV', DIR, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        e = MEL.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, ao, b, names):
        for nm in ([names] if isinstance(names, str) else names):
            if MEL.connect_material_expressions(a, ao, b, nm):
                return
        raise RuntimeError('cannot connect %s -> %s %r (inputs %s)' % (a.get_class().get_name(), b.get_class().get_name(), names,
                                                                      list(MEL.get_material_expression_input_names(b))))
    white = unreal.load_asset(DIR + '/T_castle_cloth')          # any non-virtual picture of ours, as the parameters' default
    tex = node(unreal.MaterialExpressionTextureObjectParameter, -1400, -400, parameter_name='Texture', texture=white,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    shade_tex = node(unreal.MaterialExpressionTextureObjectParameter, -1400, 300, parameter_name='Shade', texture=SHADE_DEFAULT,
                     sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    shade_amount = node(unreal.MaterialExpressionScalarParameter, -900, 500, parameter_name='ShadeAmount', default_value=0.0)
    steps = node(unreal.MaterialExpressionScalarParameter, -700, 200, parameter_name='Steps', default_value=6.0)
    tint = node(unreal.MaterialExpressionVectorParameter, 100, -500, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    sample = node(unreal.MaterialExpressionTextureSample, -1100, -400, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                  sampler_source=unreal.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET)
    link(tex, '', sample, ['Tex', 'TextureObject'])
    shade = node(unreal.MaterialExpressionTextureSample, -1100, 300, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
                 sampler_source=unreal.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET)
    link(shade_tex, '', shade, ['Tex', 'TextureObject'])
    one = node(unreal.MaterialExpressionConstant, -900, 400, r=1.0)
    shade_mix = node(unreal.MaterialExpressionLinearInterpolate, -750, 350)
    link(one, '', shade_mix, 'A')
    link(shade, 'R', shade_mix, 'B')
    link(shade_amount, '', shade_mix, 'Alpha')
    shaded = node(unreal.MaterialExpressionMultiply, -900, -300)
    link(sample, 'RGB', shaded, 'A')
    link(shade_mix, '', shaded, 'B')
    gamma = node(unreal.MaterialExpressionPower, -750, -300, const_exponent=0.4545)
    link(shaded, '', gamma, ['Base', 'A'])
    weights = node(unreal.MaterialExpressionConstant3Vector, -750, -150, constant=unreal.LinearColor(0.30, 0.59, 0.11, 0.0))
    luma = node(unreal.MaterialExpressionDotProduct, -600, -200)
    link(gamma, '', luma, 'A')
    link(weights, '', luma, 'B')
    m1 = node(unreal.MaterialExpressionMultiply, -450, -200)
    link(luma, '', m1, 'A')
    link(steps, '', m1, 'B')
    rnd = node(unreal.MaterialExpressionRound, -320, -200)
    link(m1, '', rnd, ['', 'Input'])
    d1 = node(unreal.MaterialExpressionDivide, -200, -200)
    link(rnd, '', d1, 'A')
    link(steps, '', d1, 'B')
    safe = node(unreal.MaterialExpressionMax, -450, -80, const_b=0.02)
    link(luma, '', safe, 'A')
    ratio = node(unreal.MaterialExpressionDivide, -80, -150)
    link(d1, '', ratio, 'A')
    link(safe, '', ratio, 'B')
    stepped = node(unreal.MaterialExpressionMultiply, 40, -300)
    link(gamma, '', stepped, 'A')
    link(ratio, '', stepped, 'B')
    sat = node(unreal.MaterialExpressionSaturate, 160, -300)
    link(stepped, '', sat, ['', 'Input'])
    lin = node(unreal.MaterialExpressionPower, 280, -300, const_exponent=2.2)
    link(sat, '', lin, ['Base', 'A'])
    trgb = node(unreal.MaterialExpressionComponentMask, 280, -450, r=True, g=True, b=True, a=False)
    link(tint, '', trgb, ['', 'Input'])
    col = node(unreal.MaterialExpressionMultiply, 420, -350)
    link(lin, '', col, 'A')
    link(trgb, '', col, 'B')
    rough = node(unreal.MaterialExpressionConstant, 420, 100, r=0.9)
    MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    errors = MEL.recompile_material(mat)
    if errors:
        log.append('M_GBPixelUV does not compile: %s' % [str(e) for e in errors])
    EAL.save_asset(path, only_if_is_dirty=False)
    return mat


_uv = {}


def uv_instance(orig, master, tiles):
    """The pixel version of one of the pack's materials: small copies of its own colour picture and of its occlusion,
    through M_GBPixelUV. A material with no colour picture that can be found takes its tile instead."""
    key = orig.get_path_name()
    if key in _uv:
        return _uv[key]
    base = None
    shade = None
    if isinstance(orig, unreal.MaterialInstance):
        best = -1
        for p in MEL.get_texture_parameter_names(orig):
            t = MEL.get_material_instance_texture_parameter_value(orig, p)
            if not t or 'blend' in str(p).lower() or t.blueprint_get_size_x() < 256:
                continue
            tn, pn = t.get_name().lower(), str(p).lower()
            if any(k in tn for k in ('normal', '_n_', 'orm', 'arm', 'ocg', 'mask', 'rough', 'height', 'grunge', 'noise')) or tn.endswith('_n'):
                continue
            score = 2 if any(k in tn for k in ('basecolor', 'basecolour', 'base_color', 'albedo', '_bc', 'diffuse', 'colour', 'color')) else 0
            score += 1 if any(k in pn for k in ('base', 'albedo', 'diffuse', 'color', 'colour', 'rock')) else 0
            if score > best:
                best, base = score, t
        for want in ('ocg', 'arm', 'orm'):                # the joints: a cavity mask if the material has one, else its occlusion
            for p in MEL.get_texture_parameter_names(orig):
                t = MEL.get_material_instance_texture_parameter_value(orig, p)
                if t and want in t.get_name().lower() and 'blend' not in str(p).lower() and t.blueprint_get_size_x() >= 256 \
                        and not t.get_editor_property('srgb'):
                    shade = t
                    break
            if shade:
                break
    if base is None:
        _uv[key] = tiles[tile_for(orig.get_name())]
        log.append('uv: %s has no picture of its own -> tile %s' % (orig.get_name(), tile_for(orig.get_name())))
        return _uv[key]
    mi = instance('MI_PixUV_' + orig.get_name(), master)
    MEL.set_material_instance_texture_parameter_value(mi, 'Texture', small_copy(base))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Steps', STEPS)
    if shade:
        MEL.set_material_instance_texture_parameter_value(mi, 'Shade', small_copy(shade))
        MEL.set_material_instance_scalar_parameter_value(mi, 'ShadeAmount', float(CFG.get('shade', 0.85)))
    MEL.update_material_instance(mi)
    EAL.save_asset(DIR + '/MI_PixUV_' + orig.get_name(), only_if_is_dirty=False)
    _uv[key] = mi
    return mi


# ---- run ----
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith('/Game/Glyphbound/Maps/PastCastle'):
    les.load_level('/Game/Glyphbound/Maps/PastCastle')
if not EAL.does_directory_exist(DIR):
    EAL.make_directory(DIR)
import_tiles()
tiles = tile_instances()
SHADE_DEFAULT = small_copy(unreal.load_asset('/Game/CastleTown/Textures/Stone_Fence/T_stoneSurface_ORMH'))
master = build_uv_master()
count = {'tiles': 0, 'uv': 0}
actors = 0
for a in eas.get_all_level_actors():                      # what an earlier run of this test added
    if a.actor_has_tag('GBPixelTest'):
        eas.destroy_actor(a)
flat = []
for a in eas.get_all_level_actors():
    if not (isinstance(a, unreal.StaticMeshActor) and a.actor_has_tag('GBCastle')):
        continue
    loc = a.get_actor_location()
    if loc.x > ZONE_X or not any(a.actor_has_tag(s) for s in STAGES):
        continue
    c = a.static_mesh_component
    sm = c.static_mesh
    if not sm:
        continue
    how = VARIANT if VARIANT in ('tiles', 'uv') else ('tiles' if loc.y < 0 else 'uv')
    cobble = sm.get_name().startswith('SM_Cobbles')
    if cobble:
        # A drawn cobble on a modelled cobble is two floors at once: on the tiles side the modelled piece is hidden and
        # a flat floor carries the tile.
        c.set_visibility(how != 'tiles')
        a.set_actor_hidden_in_game(how == 'tiles')
        if how == 'tiles':
            flat.append((loc.x, loc.y))
    slots = sm.static_materials
    engine = sm.get_path_name().startswith('/Engine/')
    for i in range(c.get_num_materials()):
        orig = slots[i].material_interface if i < len(slots) and not engine else None
        if engine or orig is None or how == 'tiles':
            name = 'ground' if engine else (orig.get_name() if orig else 'stone')
            c.set_material(i, tiles[tile_for(name)])
            count['tiles'] += 1
        else:
            c.set_material(i, uv_instance(orig, master, tiles))
            count['uv'] += 1
    actors += 1
for x, y in flat:                                       # the cobble piece covers 8.3 m from its pivot along X, 4.2 m to each side
    b = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Cube'), unreal.Vector(x + 410.0, y, -4.0), unreal.Rotator(0, 0, 0))
    b.set_actor_scale3d(unreal.Vector(8.3, 8.3, 0.1))
    bc = b.static_mesh_component
    bc.set_mobility(unreal.ComponentMobility.STATIC)
    bc.set_material(0, tiles['castle_cobble'])
    bc.set_collision_profile_name('NoCollision')
    b.tags = [unreal.Name('GBPixelTest')]
    b.set_folder_path(unreal.Name('Castle/pixel_test'))
les.save_current_level()
log.insert(0, 'pixel test "%s": %d pieces of the first zone; %d slots with a tile, %d slots with the pack\'s picture in pixels (%d materials of the pack converted)'
           % (VARIANT, actors, count['tiles'], count['uv'], len([k for k, v in _uv.items() if v not in tiles.values()])))
'\n'.join(log)
