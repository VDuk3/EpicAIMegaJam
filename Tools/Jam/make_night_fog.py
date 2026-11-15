# make_night_fog.py - the material of the castle's fog bank (owner, 2026-11-15: "cubre de niebla impenetrable los
# alrededores"): /Game/Glyphbound/Castle/M_NightFog, unlit and translucent, for the curtains Build-PastCastle.py hangs
# around the site, plus one instance per curtain layer (MI_NightFog_0..3, denser outward).
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/make_night_fog.py
# Why curtains and not Local Fog Volumes: the engine's local fog volumes drew nothing in this project (not in the game,
# not in Editor captures, not even one bright red volume in front of the camera), and the curtains are under our control.
# The look: two layers of the engine's tiling noise, in world space so every curtain matches its neighbour, drifting
# slowly; a vertical fade (thick at the ground, gone by FOG_TOP); a soft edge where a curtain meets the ground (depth fade).
import unreal

DIR = '/Game/Glyphbound/Castle'
NAME = 'M_NightFog'
NOISE = '/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq'
LAYERS = [0.32, 0.45, 0.6, 1.0]        # opacity of each curtain, inner to outer: the outer one hides everything

eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

path = DIR + '/' + NAME
if eal.does_asset_exist(path):
    mat = unreal.load_asset(path)
    MEL.delete_all_material_expressions(mat)
else:
    mat = tools.create_asset(NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)


def node(cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, inp, out=''):
    MEL.connect_material_expressions(a, out, b, inp)


def scalar(name, value, x, y):
    return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def const(v, x, y):
    return node(unreal.MaterialExpressionConstant, x, y, r=v)


def op(cls, a, b, x, y):
    e = node(cls, x, y)
    link(a, e, 'A')
    link(b, e, 'B')
    return e


noise_tex = unreal.load_asset(NOISE)
wp = node(unreal.MaterialExpressionWorldPosition, -1600, 0)
mx = node(unreal.MaterialExpressionComponentMask, -1400, -100, r=True, g=False, b=False, a=False)
my = node(unreal.MaterialExpressionComponentMask, -1400, 0, r=False, g=True, b=False, a=False)
mz = node(unreal.MaterialExpressionComponentMask, -1400, 100, r=False, g=False, b=True, a=False)
for m in (mx, my, mz):
    link(wp, m, '')
along = op(unreal.MaterialExpressionAdd, mx, my, -1250, -50)         # a curtain runs along x or along y
uv = node(unreal.MaterialExpressionAppendVector, -1100, 0)
link(along, uv, 'A')
link(mz, uv, 'B')


def noise_layer(scale_cm, sx, sy, y):
    d = op(unreal.MaterialExpressionDivide, uv, const(scale_cm, -1000, y + 60), -950, y)
    p = node(unreal.MaterialExpressionPanner, -800, y, speed_x=sx, speed_y=sy)
    link(d, p, 'Coordinate')
    t = node(unreal.MaterialExpressionTextureSample, -600, y, texture=noise_tex)
    link(p, t, 'UVs')
    return t


n1 = noise_layer(16000.0, 0.004, 0.0015, -300)     # banks of ~5 m (the noise has 64 texels)
n2 = noise_layer(6000.0, -0.007, 0.003, 0)        # wisps of ~1 m drifting the other way
nm = op(unreal.MaterialExpressionMultiply, n1, n2, -400, -150)
nmix = node(unreal.MaterialExpressionLinearInterpolate, -250, -150)
link(const(0.45, -400, -60), nmix, 'A')
link(const(1.0, -400, -20), nmix, 'B')
link(nm, nmix, 'Alpha')
# vertical fade: 1 under FogFadeStart, 0 at FogTop
fstart = scalar('FogFadeStart', 2500.0, -1250, 250)
ftop = scalar('FogTop', 5500.0, -1250, 330)
num = op(unreal.MaterialExpressionSubtract, mz, fstart, -1050, 220)
den = op(unreal.MaterialExpressionSubtract, ftop, fstart, -1050, 320)
frac = op(unreal.MaterialExpressionDivide, num, den, -900, 260)
sat = node(unreal.MaterialExpressionSaturate, -780, 260)
link(frac, sat, '')
fade = node(unreal.MaterialExpressionOneMinus, -660, 260)
link(sat, fade, '')
opac = scalar('Opacity', 1.0, -400, 120)
# NoiseOpacity 1: the noise thins the curtain in patches; 0: the curtain keeps its whole opacity (the outer one, which
# must hide everything below FogFadeStart; review by Sol 6.1: with the noise it let up to 41% through)
nop = scalar('NoiseOpacity', 1.0, -560, 40)
nsel = node(unreal.MaterialExpressionLinearInterpolate, -360, 40)
link(const(1.0, -560, 100), nsel, 'A')
link(nmix, nsel, 'B')
link(nop, nsel, 'Alpha')
o1 = op(unreal.MaterialExpressionMultiply, opac, nsel, -200, 60)
o2 = op(unreal.MaterialExpressionMultiply, o1, fade, -60, 120)
df = node(unreal.MaterialExpressionDepthFade, 100, 120, fade_distance_default=400.0)
link(o2, df, 'Opacity')
MEL.connect_material_property(df, '', unreal.MaterialProperty.MP_OPACITY)
col = node(unreal.MaterialExpressionVectorParameter, -400, -380, parameter_name='FogColor', default_value=unreal.LinearColor(0.42, 0.48, 0.62, 1.0))
bright = scalar('Brightness', 0.08, -400, -280)
c1 = op(unreal.MaterialExpressionMultiply, col, bright, -200, -330)
c2 = op(unreal.MaterialExpressionMultiply, c1, nmix, -60, -260)
MEL.connect_material_property(c2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(mat)
eal.save_asset(path, only_if_is_dirty=False)

made = []
for i, o in enumerate(LAYERS):
    ipath = '%s/MI_NightFog_%d' % (DIR, i)
    mi = unreal.load_asset(ipath) if eal.does_asset_exist(ipath) else tools.create_asset('MI_NightFog_%d' % i, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', mat)
    MEL.set_material_instance_scalar_parameter_value(mi, 'Opacity', o)
    MEL.set_material_instance_scalar_parameter_value(mi, 'NoiseOpacity', 0.0 if o >= 1.0 else 1.0)
    eal.save_asset(ipath, only_if_is_dirty=False)
    made.append(ipath)
'material %s, instances %s' % (path, ', '.join(made))
