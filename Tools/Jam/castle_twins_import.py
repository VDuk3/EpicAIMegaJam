# castle_twins_import.py - turns the twins written by castle_twins.js into Unreal assets: the tiles, one master material
# with an instance per tile kind, and a static mesh per twin (SM_Px_<pack name>).
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_twins_import.py
# Settings (optional): <Saved>/castle_twins.json {"only": [names], "skip_meshes": false, "tile_cm": {"wall": 200, ...}}
# Everything goes under /Game/Glyphbound/Castle/Pixel: Tiles/T_px_<kind>, M_PxTile, MI_Px_<kind>, Meshes/SM_Px_<name>.
# A twin's materials: one slot per tile kind, named after it. Its collision: the boxes castle_twins.js merged from its
# cells (simple collision), or, when there are too many, its own faces (few and flat).
import json
import os

import unreal

SAVED = unreal.Paths.project_saved_dir()
SRC = os.path.join(SAVED, 'CastleTwins', 'out')
CFG_PATH = os.path.join(SAVED, 'castle_twins.json')
CFG = json.load(open(CFG_PATH)) if os.path.exists(CFG_PATH) else {}
DIR = '/Game/Glyphbound/Castle/Pixel'
TILES_SRC = 'C:/GameJam/Glyphbound/SourceArt/PixelArt/CastleTiles/'
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = []

# tile kind -> (tile picture, centimetres of world one tile covers)
KINDS = {'wall': ('castle_wall', 200.0), 'dressed': ('castle_dressed', 200.0), 'dark': ('castle_dark', 200.0),
         'cobble': ('castle_cobble', 200.0), 'wood': ('castle_wood', 160.0), 'iron': ('castle_iron', 160.0),
         'earth': ('castle_earth', 400.0), 'cloth': ('castle_cloth', 120.0), 'roof': ('castle_roof', 200.0)}
for k, v in CFG.get('tile_cm', {}).items():
    if k in KINDS:
        KINDS[k] = (KINDS[k][0], float(v))
EXTRA = CFG.get('extra_kinds', {})
TINT = {'wall': 1.5, 'wall_fine': 1.5, 'dressed': 1.35, 'dark': 1.4, 'cobble': 1.3}
TINT.update(CFG.get('tint', {}))          # e.g. {"wall_fine": ["castle_wall", 100]}: more instances to compare


def import_tile(pic):
    path = DIR + '/Tiles/T_px_' + pic.replace('castle_', '')
    f = TILES_SRC + pic + '.png'
    if not os.path.exists(f):
        return None
    task = unreal.AssetImportTask()
    task.filename = f
    task.destination_path = DIR + '/Tiles'
    task.destination_name = path.split('/')[-1]
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    t = unreal.load_asset(path)
    t.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
    # mips (nearest within each): without them a tile far away or at a low angle breaks into noise
    t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    t.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
    t.set_editor_property('srgb', True)
    t.set_editor_property('never_stream', True)
    eal.save_asset(path, only_if_is_dirty=False)
    return t


def master():
    path = DIR + '/M_PxTile'
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        MEL.delete_all_material_expressions(m)        # the graph always matches this script
    else:
        m = tools.create_asset('M_PxTile', DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    # Built from a description, the meshes come out with their faces culled the opposite way to their computed normals
    # (one winding showed every wall lit from behind, the other showed each wall's far side). The normals are right with
    # this winding; two-sided drawing makes the culling not matter (the twins are closed solids: a back face is always
    # hidden behind a front one).
    m.set_editor_property('two_sided', True)

    def node(cls, x, y, **props):
        e = MEL.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e
    tex = node(unreal.MaterialExpressionTextureObjectParameter, -900, -300, parameter_name='Texture',
               texture=unreal.load_asset('/Engine/EngineMaterials/DefaultDiffuse'), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    size = node(unreal.MaterialExpressionScalarParameter, -1100, 0, parameter_name='TileSize', default_value=200.0)
    tint = node(unreal.MaterialExpressionVectorParameter, -500, 100, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    uv = node(unreal.MaterialExpressionTextureCoordinate, -1100, -150, coordinate_index=0)
    per = node(unreal.MaterialExpressionDivide, -900, 0, const_a=100.0)        # 1 UV unit is 1 m
    MEL.connect_material_expressions(size, '', per, 'B')
    scaled = node(unreal.MaterialExpressionMultiply, -700, -100)
    MEL.connect_material_expressions(uv, '', scaled, 'A')
    MEL.connect_material_expressions(per, '', scaled, 'B')
    sample = node(unreal.MaterialExpressionTextureSample, -500, -200, sampler_source=unreal.SamplerSourceMode.SSM_FROM_TEXTURE_ASSET)
    MEL.connect_material_expressions(tex, '', sample, 'Tex')
    MEL.connect_material_expressions(scaled, '', sample, 'UVs')
    rgb = node(unreal.MaterialExpressionComponentMask, -400, 100, r=True, g=True, b=True)
    MEL.connect_material_expressions(tint, '', rgb, '')
    col = node(unreal.MaterialExpressionMultiply, -300, -100)
    ok = [MEL.connect_material_expressions(sample, 'RGB', col, 'A'), MEL.connect_material_expressions(rgb, '', col, 'B')]
    if not all(ok):
        log.append('M_PxTile: a connection failed %s' % ok)
    # Two pieces of the author's that overlap on one plane would fight for the pixel (the keep's corner pillars inside its
    # walls). Each piece is moved toward the camera by its own fraction of a centimetre, drawn from its position: the
    # faces separate, nothing visible moves.
    opos = node(unreal.MaterialExpressionObjectPositionWS, -1100, 400)
    seed = node(unreal.MaterialExpressionDotProduct, -950, 400)
    seedc = node(unreal.MaterialExpressionConstant3Vector, -1100, 500, constant=unreal.LinearColor(0.1291, 0.7823, 0.3772, 0))
    MEL.connect_material_expressions(opos, '', seed, 'A')
    MEL.connect_material_expressions(seedc, '', seed, 'B')
    sn = node(unreal.MaterialExpressionSine, -800, 400)
    MEL.connect_material_expressions(seed, '', sn, '')
    big = node(unreal.MaterialExpressionMultiply, -700, 400, const_b=43758.5453)
    MEL.connect_material_expressions(sn, '', big, 'A')
    fr = node(unreal.MaterialExpressionFrac, -600, 400)
    MEL.connect_material_expressions(big, '', fr, '')
    amt = node(unreal.MaterialExpressionMultiply, -500, 400, const_b=0.8)
    MEL.connect_material_expressions(fr, '', amt, 'A')
    # toward the camera (the camera vector node is not allowed in the vertex shader): normalize(camera - position)
    cpos = node(unreal.MaterialExpressionCameraPositionWS, -800, 550)
    wpos = node(unreal.MaterialExpressionWorldPosition, -800, 650)
    tocam = node(unreal.MaterialExpressionSubtract, -650, 600)
    MEL.connect_material_expressions(cpos, '', tocam, 'A')
    MEL.connect_material_expressions(wpos, '', tocam, 'B')
    cam = node(unreal.MaterialExpressionNormalize, -500, 550)
    MEL.connect_material_expressions(tocam, '', cam, '')
    wpo = node(unreal.MaterialExpressionMultiply, -350, 450)
    MEL.connect_material_expressions(cam, '', wpo, 'A')
    MEL.connect_material_expressions(amt, '', wpo, 'B')
    MEL.connect_material_property(wpo, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    rough = node(unreal.MaterialExpressionConstant, -300, 100, r=0.92)
    MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


def master_world():
    """The twins' material: the game's own world-aligned pixel surface (M_PixelSurface: the tile laid on the world by
    the face's dominant axis, nearest filter), copied, drawn two-sided and with the per-piece depth nudge. Courses run on
    from one module to the next and round a corner without a step."""
    path = DIR + '/M_PxWorld'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = eal.duplicate_asset('/Game/Glyphbound/Materials/M_PixelSurface', path)
    m.set_editor_property('two_sided', True)

    def node(cls, x, y, **props):
        e = MEL.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e
    opos = node(unreal.MaterialExpressionObjectPositionWS, -1100, 700)
    seedc = node(unreal.MaterialExpressionConstant3Vector, -1100, 800, constant=unreal.LinearColor(0.1291, 0.7823, 0.3772, 0))
    seed = node(unreal.MaterialExpressionDotProduct, -950, 700)
    MEL.connect_material_expressions(opos, '', seed, 'A')
    MEL.connect_material_expressions(seedc, '', seed, 'B')
    sn = node(unreal.MaterialExpressionSine, -800, 700)
    MEL.connect_material_expressions(seed, '', sn, '')
    big = node(unreal.MaterialExpressionMultiply, -700, 700, const_b=43758.5453)
    MEL.connect_material_expressions(sn, '', big, 'A')
    fr = node(unreal.MaterialExpressionFrac, -600, 700)
    MEL.connect_material_expressions(big, '', fr, '')
    amt = node(unreal.MaterialExpressionMultiply, -500, 700, const_b=0.8)
    MEL.connect_material_expressions(fr, '', amt, 'A')
    cpos = node(unreal.MaterialExpressionCameraPositionWS, -800, 850)
    wpos = node(unreal.MaterialExpressionWorldPosition, -800, 950)
    tocam = node(unreal.MaterialExpressionSubtract, -650, 900)
    MEL.connect_material_expressions(cpos, '', tocam, 'A')
    MEL.connect_material_expressions(wpos, '', tocam, 'B')
    cam = node(unreal.MaterialExpressionNormalize, -500, 900)
    MEL.connect_material_expressions(tocam, '', cam, '')
    wpo = node(unreal.MaterialExpressionMultiply, -350, 800)
    MEL.connect_material_expressions(cam, '', wpo, 'A')
    MEL.connect_material_expressions(amt, '', wpo, 'B')
    MEL.connect_material_property(wpo, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    # (the tile follows the nudge by under a centimetre: an eighth of a texel)
    MEL.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


def instance(kind, pic, cm, parent):
    path = DIR + '/MI_Px_' + kind
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset('MI_Px_' + kind, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', parent)
    t = unreal.load_asset(DIR + '/Tiles/T_px_' + pic.replace('castle_', ''))
    if t:
        MEL.set_material_instance_texture_parameter_value(mi, 'Texture', t)
    MEL.set_material_instance_scalar_parameter_value(mi, 'TileSize', cm)
    # the drawn stone is darker than the pack's: in the shade of the walls (lit only by the sky and by bounce) it went
    # nearly black. TINT = {kind: factor} brightens a tile without redrawing it.
    f = TINT.get(kind, 1.0)
    MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(f, f, f, 1))
    if kind == 'roof' and not t:
        # no roof tile yet: the dark stone, reddened
        MEL.set_material_instance_texture_parameter_value(mi, 'Texture', unreal.load_asset(DIR + '/Tiles/T_px_dark'))
        MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(1.25, 0.62, 0.5, 1))
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


def materials():
    if not eal.does_directory_exist(DIR + '/Tiles'):
        eal.make_directory(DIR + '/Tiles')
    for pic, _ in set(KINDS.values()) | set((v[0], v[1]) for v in EXTRA.values()):
        import_tile(pic)
    parent = master()         # (master_world, the world-aligned copy, striped these meshes: their normals do not pick the plane well)
    out = {}
    world = unreal.load_asset('/Game/Glyphbound/Materials/M_PixelSurface')
    for kind, (pic, cm) in list(KINDS.items()) + [(k, (v[0], float(v[1]))) for k, v in EXTRA.items()]:
        out[kind] = instance(kind, pic, cm, parent)
        # the same tile laid on the world, for the builder's plain boxes (no UVs of their own)
        path = DIR + '/MIW_Px_' + kind
        mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset('MIW_Px_' + kind, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', world)
        t = unreal.load_asset(DIR + '/Tiles/T_px_' + pic.replace('castle_', '')) or unreal.load_asset(DIR + '/Tiles/T_px_dark')
        MEL.set_material_instance_texture_parameter_value(mi, 'Texture', t)
        MEL.set_material_instance_scalar_parameter_value(mi, 'TileSize', cm)
        f = TINT.get(kind, 1.0)
        MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(f, f, f, 1))
        eal.save_asset(path, only_if_is_dirty=False)
    return out


def twin_name(pack_name):
    return 'SM_Px_' + (pack_name[3:] if pack_name.startswith('SM_') else pack_name)


def build_mesh(d, mats):
    name = twin_name(d['name'])
    pkg = DIR + '/Meshes'
    path = pkg + '/' + name
    if eal.does_asset_exist(path):
        sm = unreal.load_asset(path)
    else:
        sm = tools.create_asset(name, pkg, unreal.StaticMesh, None)
    desc = sm.create_static_mesh_description()
    kinds = sorted(d['sections'])
    slots = []
    for kind in kinds:
        sec = d['sections'][kind]
        pg = desc.create_polygon_group()
        desc.set_polygon_group_material_slot_name(pg, kind)
        slots.append(unreal.StaticMaterial(material_interface=mats.get(kind), material_slot_name=kind))
        v, uv, t = sec['v'], sec['uv'], sec['t']
        vids = []
        for i in range(len(v) // 3):
            vid = desc.create_vertex()
            desc.set_vertex_position(vid, unreal.Vector(v[i * 3], v[i * 3 + 1], v[i * 3 + 2]))
            vids.append(vid)
        for k in range(len(t) // 3):
            tri = []
            for q in (0, 1, 2):           # castle_twins.js winds them as Unreal's front faces want (checked in the game: the other way showed each wall's far side)
                i = t[k * 3 + q]
                vi = desc.create_vertex_instance(vids[i])
                desc.set_vertex_instance_uv(vi, unreal.Vector2D(uv[i * 2], uv[i * 2 + 1]), 0)
                tri.append(vi)
            desc.create_triangle(pg, tri)
    sm.set_editor_property('static_materials', slots)
    sm.build_from_static_mesh_descriptions([desc])
    # light and plain: no Nanite, no lightmap UV of its own (the game is lit by Lumen), flat normals
    ns = sm.get_editor_property('nanite_settings')
    ns.set_editor_property('enabled', False)
    sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs is None:
        sm.create_body_setup() if hasattr(sm, 'create_body_setup') else None
        bs = sm.get_editor_property('body_setup')
    if d.get('boxes'):
        unreal.EditorStaticMeshLibrary.remove_collisions(sm)
        agg = bs.get_editor_property('agg_geom')
        elems = []
        for b in d['boxes']:
            e = unreal.KBoxElem()
            e.set_editor_property('center', unreal.Vector(*b['c']))
            e.set_editor_property('x', b['s'][0])
            e.set_editor_property('y', b['s'][1])
            e.set_editor_property('z', b['s'][2])
            e.set_editor_property('rotation', unreal.Rotator(roll=0, pitch=0, yaw=b['yaw']))
            elems.append(e)
        agg.set_editor_property('box_elems', elems)
        bs.set_editor_property('agg_geom', agg)
        bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    else:
        # a mesh built from a description has its sections' collision off: the faces must be switched on to block
        for sec in range(sm.get_num_sections(0)):
            unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).enable_section_collision(sm, True, 0, sec)
        bs = sm.get_editor_property('body_setup')
        bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    # complex traces (the builder's checks, shots) see the faces from both sides: the physics cooking reads this mesh's
    # faces the other way round from the renderer
    bs.set_editor_property('double_sided_geometry', True)
    sm.set_editor_property('body_setup', bs)
    eal.save_asset(path, only_if_is_dirty=False)
    return name


mats = materials()
log.append('materials: %d instances of M_PxTile' % len(mats))
done = []
if not CFG.get('skip_meshes'):
    only = CFG.get('only')
    for f in sorted(os.listdir(SRC)):
        if not f.endswith('.json'):
            continue
        if only and f[:-5] not in only:
            continue
        d = json.load(open(os.path.join(SRC, f)))
        try:
            done.append(build_mesh(d, mats))
        except Exception as e:
            log.append('FAILED %s: %s' % (f, e))
log.append('meshes: %d twins built' % len(done))
open(os.path.join(SAVED, 'castle_twins_import.txt'), 'w').write('\n'.join(log))
'\n'.join(log)
