# castle_glass_stairs.py - owner's castle playtest, 2026-11-15, notes 6 and 11.
#  - Windows: two pixel panes (leaded glass, stained glass) and their material, for Build-PastCastle's stage "windows".
#  - Stairs: the pixel twins' stairs showed one stone tile laid over several steps ("the floor's texture on a stair's
#    relief"). A stair material lays the dressed stone on the treads and a dark course on the risers (told apart by the
#    face's normal), so every step reads on its own. Applied to the twins' stair meshes and to any actor that overrides them.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_glass_stairs.py
import unreal

DIR = '/Game/Glyphbound/Castle/Pixel'
SRC = 'C:/GameJam/Glyphbound/SourceArt/PixelArt/Windows/'
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = []


def import_tex(name):
    path = DIR + '/Tiles/' + name
    task = unreal.AssetImportTask()
    task.filename = SRC + name + '.png'
    task.destination_path = DIR + '/Tiles'
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    t = unreal.load_asset(path)
    t.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
    t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    t.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
    t.set_editor_property('srgb', True)
    t.set_editor_property('never_stream', True)
    eal.save_asset(path, only_if_is_dirty=False)
    return t


def new_material(name):
    path = DIR + '/' + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        MEL.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    def node(cls, x, y, **props):
        e = MEL.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e
    return m, path, node


def glass_master():
    """The pane: the picture once over the face (a cube's faces carry 0..1 UVs), lit, and glowing by Glow (light from
    the halls behind it)."""
    m, path, node = new_material('M_PxGlass')
    tex = node(unreal.MaterialExpressionTextureSampleParameter2D, -600, 0, parameter_name='Texture',
               texture=unreal.load_asset('/Engine/EngineMaterials/DefaultDiffuse'))
    glow = node(unreal.MaterialExpressionScalarParameter, -600, 250, parameter_name='Glow', default_value=0.5)
    em = node(unreal.MaterialExpressionMultiply, -300, 150)
    MEL.connect_material_expressions(tex, 'RGB', em, 'A')
    MEL.connect_material_expressions(glow, '', em, 'B')
    rough = node(unreal.MaterialExpressionConstant, -300, 300, r=0.35)
    MEL.connect_material_property(tex, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


def stairs_master():
    m, path, node = new_material('M_PxStairs')
    m.set_editor_property('two_sided', True)          # as M_PxTile: the twins' winding is reversed, two-sided hides it
    size = node(unreal.MaterialExpressionScalarParameter, -1300, 0, parameter_name='TileSize', default_value=200.0)
    uv = node(unreal.MaterialExpressionTextureCoordinate, -1300, -150, coordinate_index=0)
    per = node(unreal.MaterialExpressionDivide, -1100, 0, const_a=100.0)
    MEL.connect_material_expressions(size, '', per, 'B')
    scaled = node(unreal.MaterialExpressionMultiply, -950, -100)
    MEL.connect_material_expressions(uv, '', scaled, 'A')
    MEL.connect_material_expressions(per, '', scaled, 'B')
    tread = node(unreal.MaterialExpressionTextureSampleParameter2D, -700, -300, parameter_name='Tread',
                 texture=unreal.load_asset('/Engine/EngineMaterials/DefaultDiffuse'))
    riser = node(unreal.MaterialExpressionTextureSampleParameter2D, -700, 0, parameter_name='Riser',
                 texture=unreal.load_asset('/Engine/EngineMaterials/DefaultDiffuse'))
    MEL.connect_material_expressions(scaled, '', tread, 'UVs')
    MEL.connect_material_expressions(scaled, '', riser, 'UVs')
    ttint = node(unreal.MaterialExpressionScalarParameter, -700, 250, parameter_name='TreadTint', default_value=1.35)
    rtint = node(unreal.MaterialExpressionScalarParameter, -700, 350, parameter_name='RiserTint', default_value=0.55)
    tcol = node(unreal.MaterialExpressionMultiply, -450, -250)
    MEL.connect_material_expressions(tread, 'RGB', tcol, 'A')
    MEL.connect_material_expressions(ttint, '', tcol, 'B')
    rcol = node(unreal.MaterialExpressionMultiply, -450, 50)
    MEL.connect_material_expressions(riser, 'RGB', rcol, 'A')
    MEL.connect_material_expressions(rtint, '', rcol, 'B')
    # tread where the face looks up: saturate((n.z - 0.6) * 5); risers (n.z ~ 0) and the undersides get the dark course
    nrm = node(unreal.MaterialExpressionVertexNormalWS, -1100, 400)
    nz = node(unreal.MaterialExpressionComponentMask, -950, 400, r=False, g=False, b=True)
    MEL.connect_material_expressions(nrm, '', nz, '')
    sub = node(unreal.MaterialExpressionSubtract, -800, 400, const_b=0.6)
    MEL.connect_material_expressions(nz, '', sub, 'A')
    mul = node(unreal.MaterialExpressionMultiply, -650, 400, const_b=5.0)
    MEL.connect_material_expressions(sub, '', mul, 'A')
    sat = node(unreal.MaterialExpressionSaturate, -500, 400)
    MEL.connect_material_expressions(mul, '', sat, '')
    lerp = node(unreal.MaterialExpressionLinearInterpolate, -250, 0)
    ok = [MEL.connect_material_expressions(rcol, '', lerp, 'A'), MEL.connect_material_expressions(tcol, '', lerp, 'B'),
          MEL.connect_material_expressions(sat, '', lerp, 'Alpha')]
    if not all(ok):
        log.append('M_PxStairs: a connection failed %s' % ok)
    rough = node(unreal.MaterialExpressionConstant, -250, 200, r=0.92)
    MEL.connect_material_property(lerp, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    eal.save_asset(path, only_if_is_dirty=False)
    return m


def instance(name, parent, tex=None, scalars=None):
    path = DIR + '/' + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', parent)
    for k, t in (tex or {}).items():
        MEL.set_material_instance_texture_parameter_value(mi, k, t)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


glass = glass_master()
leaded = import_tex('T_px_window_leaded')
stained = import_tex('T_px_window_stained')
instance('MI_Px_glass_leaded', glass, {'Texture': leaded}, {'Glow': 0.35})
instance('MI_Px_glass_stained', glass, {'Texture': stained}, {'Glow': 1.4})

stairs = stairs_master()
dressed = unreal.load_asset(DIR + '/Tiles/T_px_dressed')
dark = unreal.load_asset(DIR + '/Tiles/T_px_dark') or dressed
mi_st = instance('MI_Px_stairs', stairs, {'Tread': dressed, 'Riser': dark}, {'TileSize': 100.0, 'TreadTint': 1.35, 'RiserTint': 0.6})

# The twins' stair meshes: every slot that holds the dressed stone (or any pixel tile) takes the stair material.
STAIRS = ('SM_Px_Main_Stairs', 'SM_Px_Spiral_Stairs', 'SM_Px_Stairs_4M', 'SM_Px_Stairs_5M', 'SM_Px_FloorCorridorStair_01')
done = []
for p in eal.list_assets(DIR + '/Twins', recursive=True, include_folder=False):
    nm = p.split('/')[-1].split('.')[0]
    if nm not in STAIRS:          # by name only, a vaulted ceiling and a corridor wall would count: their faces are walls
        continue
    sm = unreal.load_asset(p)
    if not isinstance(sm, unreal.StaticMesh):
        continue
    for i, sl in enumerate(sm.static_materials):
        mi = sl.material_interface
        if mi and mi.get_name().startswith('MI_Px_') and mi.get_name() not in ('MI_Px_wood', 'MI_Px_iron', 'MI_Px_cloth'):
            sm.set_material(i, mi_st)
    eal.save_asset(p, only_if_is_dirty=False)
    done.append(nm)
# Actors in the open map that override those meshes' materials with the dressed stone
n_act = 0
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    c = a.static_mesh_component
    if not c.static_mesh or c.static_mesh.get_name() not in STAIRS:
        continue
    for i, mi in enumerate(c.override_materials):
        if mi and mi.get_name().startswith('MI_Px_') and mi.get_name() not in ('MI_Px_wood', 'MI_Px_iron', 'MI_Px_cloth'):
            c.set_material(i, mi_st)
            n_act += 1
log.append('stairs: %d meshes (%s), %d actor overrides' % (len(done), ', '.join(done), n_act))
print('\n'.join(log))
