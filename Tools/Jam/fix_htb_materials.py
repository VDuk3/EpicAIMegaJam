# fix_htb_materials.py - gives the High Tech Base kit materials that render in our project (Substrate off).
# Run once in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/fix_htb_materials.py
#
# The pack is authored for Substrate (colour and roughness go into a Substrate Slab node). Our project runs with
# r.Substrate=0, and there every surface of the kit rendered one flat grey: the Base Color buffer showed the same
# value on every material, whatever texture it had. Rewiring the pack's own masters (classic pins connected, slab
# disconnected, shading model Default Lit, parallax off) changed nothing on screen, while the same textures on a
# new plain material rendered correctly. So this script builds two plain masters of our own and re-parents the
# pack's material instances to them. The instances keep their own textures and numbers, because the parameters of
# the new masters have the same names as the pack's:
#   M_GBKit_Surface  for instances of M_Material and M_Base_Prop  (BaseColor, Normal, Roughness, Metallic, AO,
#                    Emissive textures; Tile-x, Tile-y, Emissive_Multiply, Metallic Add, Roughness Add)
#   M_GBKit_Paint    for instances of M_Roughness (flat painted metal: BaseColor texture x "Color Blend" x 2,
#                    which equals the pack's Overlay blend for its dark base textures; Metallic scalar; Roughness
#                    and Normal_Bump textures; Tile-x, Tile-y, Bump_Tiling)
# Sampler types and default textures are read from the pack's masters, so every instance's textures stay valid.
# M_Emissive and M_Decal are left as they are (the emissive one renders).
# Idempotent: the masters are rebuilt on every run and instances already re-parented are skipped.
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
SRC = '/Game/High_Tech_Base/Materials/'
DST = '/Game/Glyphbound/Materials/Kit'
T = 'EditorToolset.MaterialToolset.'
report = []


async def sampler_info(master_name):
    """parameter name -> (default texture, sampler type) of the texture parameters of one of the pack's masters."""
    path = SRC + master_name + '.' + master_name
    info = {}
    for e in await call_tool(T + 'GetExpressions', materialOrFunction={'refPath': path}):
        o = unreal.find_object(None, e['refPath'])
        if o and o.get_class().get_name() in ('MaterialExpressionTextureSampleParameter2D', 'MaterialExpressionTextureObjectParameter'):
            name = str(o.get_editor_property('parameter_name'))
            if name not in info:
                info[name] = (o.get_editor_property('texture'), o.get_editor_property('sampler_type'))
    return info


def new_material(name):
    path = DST + '/' + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DST, unreal.Material, unreal.MaterialFactoryNew())


def scalar(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', value)
    return e


def tex_param(m, info, name, x, y, uv):
    e = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property('parameter_name', name)
    if name in info:
        e.set_editor_property('texture', info[name][0])
        e.set_editor_property('sampler_type', info[name][1])
    mel.connect_material_expressions(uv, '', e, 'UVs')
    return e


def tiled_uv(m, x, y, name_x='Tile-x', name_y='Tile-y'):
    coord = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, x, y)
    tx = scalar(m, name_x, 1.0, x, y + 120)
    ty = scalar(m, name_y, 1.0, x, y + 200)
    app = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, x + 200, y + 150)
    mel.connect_material_expressions(tx, '', app, 'A')
    mel.connect_material_expressions(ty, '', app, 'B')
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, x + 400, y)
    mel.connect_material_expressions(coord, '', mul, 'A')
    mel.connect_material_expressions(app, '', mul, 'B')
    return mul


def op(m, cls, a, a_out, b, b_out, x, y):
    e = mel.create_material_expression(m, cls, x, y)
    mel.connect_material_expressions(a, a_out, e, 'A')
    mel.connect_material_expressions(b, b_out, e, 'B')
    return e


MP = unreal.MaterialProperty

# ---- M_GBKit_Surface
info = await sampler_info('M_Material')
m = new_material('M_GBKit_Surface')
uv = tiled_uv(m, -1400, 0)
bc = tex_param(m, info, 'BaseColor', -700, -400, uv)
mel.connect_material_property(bc, 'RGB', MP.MP_BASE_COLOR)
nr = tex_param(m, info, 'Normal', -700, -100, uv)
mel.connect_material_property(nr, 'RGB', MP.MP_NORMAL)
ro = tex_param(m, info, 'Roughness', -700, 200, uv)
ro_add = op(m, unreal.MaterialExpressionAdd, ro, 'R', scalar(m, 'Roughness Add', 0.0, -700, 420), '', -400, 250)
mel.connect_material_property(ro_add, '', MP.MP_ROUGHNESS)
me = tex_param(m, info, 'Metallic', -700, 520, uv)
me_add = op(m, unreal.MaterialExpressionAdd, me, 'R', scalar(m, 'Metallic Add', 0.0, -700, 740), '', -400, 570)
mel.connect_material_property(me_add, '', MP.MP_METALLIC)
ao = tex_param(m, info, 'AO', -700, 840, uv)
mel.connect_material_property(ao, 'R', MP.MP_AMBIENT_OCCLUSION)
em = tex_param(m, info, 'Emissive', -700, 1100, uv)
em_mul = op(m, unreal.MaterialExpressionMultiply, em, 'RGB', scalar(m, 'Emissive_Multiply', 1.0, -700, 1320), '', -400, 1150)
mel.connect_material_property(em_mul, '', MP.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_asset(DST + '/M_GBKit_Surface', only_if_is_dirty=False)
surface = m
report.append('M_GBKit_Surface built; texture parameters read from the pack: %s' % sorted(info.keys()))
await asyncio.sleep(0)

# ---- M_GBKit_Paint
info = await sampler_info('M_Roughness')
m = new_material('M_GBKit_Paint')
uv = tiled_uv(m, -1400, 0)
bc = tex_param(m, info, 'BaseColor', -900, -400, uv)
tint = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, -150)
tint.set_editor_property('parameter_name', 'Color Blend')
tint.set_editor_property('default_value', unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
two = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -900, 60)
two.set_editor_property('r', 2.0)
tinted = op(m, unreal.MaterialExpressionMultiply, bc, 'RGB', tint, '', -600, -300)
tinted2 = op(m, unreal.MaterialExpressionMultiply, tinted, '', two, '', -400, -300)
sat = mel.create_material_expression(m, unreal.MaterialExpressionSaturate, -200, -300)
mel.connect_material_expressions(tinted2, '', sat, '')
mel.connect_material_property(sat, '', MP.MP_BASE_COLOR)
mel.connect_material_property(scalar(m, 'Metallic', 0.0, -700, 200), '', MP.MP_METALLIC)
ro = tex_param(m, info, 'Roughness', -700, 320, uv)
ro_add = op(m, unreal.MaterialExpressionAdd, ro, 'R', scalar(m, 'Roughness Add', 0.0, -700, 540), '', -400, 370)
mel.connect_material_property(ro_add, '', MP.MP_ROUGHNESS)
bump_uv = op(m, unreal.MaterialExpressionMultiply, uv, '', scalar(m, 'Bump_Tiling', 1.0, -1100, 760), '', -900, 700)
nb = tex_param(m, info, 'Normal_Bump', -700, 660, bump_uv)
mel.connect_material_property(nb, 'RGB', MP.MP_NORMAL)
mel.recompile_material(m)
eal.save_asset(DST + '/M_GBKit_Paint', only_if_is_dirty=False)
paint = m
report.append('M_GBKit_Paint built; texture parameters read from the pack: %s' % sorted(info.keys()))
await asyncio.sleep(0)

# ---- re-parent the instances whose parent is one of the pack's masters (instances of instances follow them)
target = {'M_Material': surface, 'M_Base_Prop': surface, 'M_Roughness': paint}
ar = unreal.AssetRegistryHelpers.get_asset_registry()
# Run this with no level open that shows these materials (the empty start-up map): re-parenting while the pack's
# demo level was on screen crashed the Editor (render-thread assert "UniformExpressionCache should be up to date"
# on MI_R6_Inst, an instance of an instance). So: re-parent everything first, then refresh every instance
# (children included), let a few frames pass, and only then save.
moved = 0
instances = []
for a in ar.get_assets_by_path('/Game/High_Tech_Base/Materials', recursive=True):
    if str(a.asset_class_path.asset_name) != 'MaterialInstanceConstant':
        continue
    mi = a.get_asset()
    instances.append((str(a.package_name), mi))
    parent = mi.parent
    if parent and parent.get_name() in target:
        mel.set_material_instance_parent(mi, target[parent.get_name()])
        moved += 1
for _, mi in instances:
    mel.update_material_instance(mi)
for _ in range(5):
    await asyncio.sleep(0.1)
saved = 0
for pkg, mi in instances:
    if mi.get_base_material().get_name().startswith('M_GBKit'):
        eal.save_asset(pkg, only_if_is_dirty=False)
        saved += 1
        await asyncio.sleep(0)
report.append('instances re-parented now: %d; instances on the new masters, saved: %d' % (moved, saved))
'\n'.join(report)
