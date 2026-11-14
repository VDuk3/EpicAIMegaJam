# import_bulkhead.py - the glass bulkhead of the transfer bay (Tools/Blender/bulkhead_glass_export.py) into the project.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/import_bulkhead.py
# Imports SM_GB_Bulkhead_Frame and SM_GB_Bulkhead_Glazing and makes M_GB_DeepGlass: the thick, dirty glass of the deep
# wing's windows (owner, 2026-11-14: "there are windows of thick dirty glass in the map already, take them as the
# model"). Opaque like them: nearly black teal glass with the deep water's glow behind it, grime gathered along each
# pane's edges and bottom (the panes are unwrapped 0..1), streaks running down, and a gloss that the grime kills.
# The steel is the worn corridor's own (MI_ShipCorridor_HullWorn: laid over the world, it needs no unwrap).
# Idempotent. Without the FBX files it only rebuilds the material and reassigns.
import os

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Engine/Maps/Entry')   # never rebuild a material under a level that shows it
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
SRC = 'C:/GameJam/Glyphbound/Saved/bulkhead_export/'
DST = '/Game/Glyphbound/Environment/Bulkhead'
OUR = '/Game/Glyphbound/Materials'
STEEL = '/Game/Glyphbound/Environment/ShipCorridor/MI_ShipCorridor_HullWorn'
NOISE_A = '/Game/HologramShader/Textures/Greyscale/T_Noise_02'
NOISE_B = '/Game/HologramShader/Textures/Greyscale/T_Noise_03'
out = []


def node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, getattr(unreal, 'MaterialExpression' + cls), x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin='', out_pin=''):
    mel.connect_material_expressions(a, out_pin, b, pin)


def scalar(m, name, value, x, y):
    return node(m, 'ScalarParameter', x, y, parameter_name=name, default_value=value)


def vector(m, name, value, x, y):
    return node(m, 'VectorParameter', x, y, parameter_name=name, default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))


# ---- M_GB_DeepGlass
path = OUR + '/M_GB_DeepGlass'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path)
    mel.delete_all_material_expressions(m)
else:
    m = tools.create_asset('M_GB_DeepGlass', OUR, unreal.Material, unreal.MaterialFactoryNew())


def op(cls, a, b, x, y, **props):
    e = node(m, cls, x, y, **props)
    if a is not None:
        link(a, e, 'A')
    if b is not None:
        link(b, e, 'B')
    return e


def one(cls, a, x, y):
    e = node(m, cls, x, y)
    link(a, e)
    return e


uv = node(m, 'TextureCoordinate', -2600, -200)
u = node(m, 'ComponentMask', -2400, -260, r=True, g=False, b=False, a=False)
v = node(m, 'ComponentMask', -2400, -140, r=False, g=True, b=False, a=False)
link(uv, u)
link(uv, v)
# distance to the pane's nearest edge, 0 at the edge, 0.5 in the middle
du = op('Min', u, one('OneMinus', u, -2250, -320), -2100, -280)
dv = op('Min', v, one('OneMinus', v, -2250, -80), -2100, -120)
d = op('Min', du, dv, -1950, -200)
edge = one('Saturate', one('OneMinus', op('Divide', d, None, -1800, -200, const_b=0.2), -1650, -200), -1500, -200)       # 1 at the edge, 0 from 20 % in
edge2 = op('Multiply', edge, edge, -1350, -200)
bottom = one('Saturate', one('OneMinus', op('Divide', v, None, -1800, 0, const_b=0.30), -1650, 0), -1500, 0)               # 1 at a pane's foot, 0 above 30 %

wp = node(m, 'WorldPosition', -2600, 400)
wy = node(m, 'ComponentMask', -2400, 340, r=False, g=True, b=False, a=False)
wz = node(m, 'ComponentMask', -2400, 460, r=False, g=False, b=True, a=False)
wyz = node(m, 'ComponentMask', -2400, 580, r=False, g=True, b=True, a=False)
for p in (wy, wz, wyz):
    link(wp, p)


def tex(coord, tex_path, x, y):
    ts = node(m, 'TextureSample', x, y, texture=unreal.load_asset(tex_path))
    link(coord, ts, 'UVs')
    red = node(m, 'ComponentMask', x + 180, y, r=True, g=False, b=False, a=False)      # one channel: everything below is a scalar
    link(ts, red)
    return red


patches = tex(op('Divide', wyz, None, -2200, 580, const_b=310.0), NOISE_A, -2000, 580)                  # large: where the glass is cloudier
fine = tex(op('Divide', wyz, None, -2200, 800, const_b=47.0), NOISE_B, -2000, 800)                      # small: scuffs
# streaks that ran down the glass: a noise stretched 14 times along Z
drip_uv = node(m, 'AppendVector', -2050, 1020)
link(op('Divide', wy, None, -2200, 1000, const_b=38.0), drip_uv, 'A')
link(op('Divide', wz, None, -2200, 1100, const_b=540.0), drip_uv, 'B')
drips = tex(drip_uv, NOISE_B, -1850, 1020)
drip_mask = one('Saturate', op('Multiply', op('Subtract', drips, None, -1650, 1020, const_b=0.56), None, -1500, 1020, const_b=4.0), -1350, 1020)

# grime: thick along the edges and at the foot, broken up by the patches, plus the streaks, plus a haze everywhere
border = op('Add', edge2, op('Multiply', bottom, None, -1350, 0, const_b=0.55), -1150, -100)
broken = op('Multiply', border, op('Add', patches, None, -1350, 580, const_b=0.45), -950, 100)
streaked = op('Add', broken, op('Multiply', drip_mask, None, -1150, 1020, const_b=0.85), -750, 300)
hazed = op('Add', streaked, op('Multiply', fine, None, -1350, 800, const_b=0.14), -550, 300)
grime = one('Saturate', op('Multiply', hazed, scalar(m, 'Grime', 1.0, -750, 480), -400, 300), -250, 300)

colour = node(m, 'LinearInterpolate', 0, -300)
link(vector(m, 'Glass', (0.003, 0.016, 0.02), -300, -400), colour, 'A')
link(vector(m, 'Dirt', (0.014, 0.016, 0.011), -300, -220), colour, 'B')
link(grime, colour, 'Alpha')
mel.connect_material_property(colour, '', MP.MP_BASE_COLOR)

clean_rough = node(m, 'LinearInterpolate', -300, 0, const_a=0.03, const_b=0.2)
link(fine, clean_rough, 'Alpha')
rough = node(m, 'LinearInterpolate', 0, 0, const_b=0.82)
link(clean_rough, rough, 'A')
link(grime, rough, 'Alpha')
mel.connect_material_property(rough, '', MP.MP_ROUGHNESS)

# the glow of what is behind: the deep water's colour, uneven, gone where the grime is thick, a little stronger up the pane
depth = node(m, 'LinearInterpolate', -550, 700, const_a=0.8, const_b=1.15)
link(patches, depth, 'Alpha')
rise = node(m, 'LinearInterpolate', -550, 880, const_a=0.75, const_b=1.15)
link(v, rise, 'Alpha')
through = one('OneMinus', op('Multiply', grime, None, -250, 620, const_b=0.92), -100, 620)
glow = op('Multiply', vector(m, 'Glow', (0.02, 0.14, 0.2), -550, 1060), scalar(m, 'GlowStrength', 0.03, -550, 1240), -300, 1100)
g1 = op('Multiply', glow, depth, -100, 900)
g2 = op('Multiply', g1, rise, 50, 900)
vignette = one('Saturate', op('Multiply', d, None, -1800, -420, const_b=2.4), -1650, -420)      # 0 at a pane's edge, 1 from 42 % in: thick glass passes less light near its frame
vig = node(m, 'LinearInterpolate', -1450, -420, const_a=0.12, const_b=1.0)
link(vignette, vig, 'Alpha')
g2b = op('Multiply', g2, vig, 200, 950)
g3 = op('Multiply', g2b, through, 350, 800)
mel.connect_material_property(g3, '', MP.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
eal.save_asset(path, only_if_is_dirty=False)
out.append('M_GB_DeepGlass')

# ---- the meshes
for name in ('SM_GB_Bulkhead_Frame', 'SM_GB_Bulkhead_Glazing'):
    fbx = SRC + name + '.fbx'
    if os.path.exists(fbx):
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', fbx)
        task.set_editor_property('destination_path', DST)
        task.set_editor_property('destination_name', name)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', True)
        tools.import_asset_tasks([task])
    sm = unreal.load_asset(DST + '/' + name)
    if not sm:
        out.append(name + ': NOT IMPORTED')
        continue
    mats = sm.get_editor_property('static_materials')
    slots = []
    for i, slot in enumerate(mats):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        sm.set_material(i, unreal.load_asset(path if 'glass' in slot_name.lower() else STEEL))
        slots.append(slot_name)
    eal.save_asset(DST + '/' + name, only_if_is_dirty=False)
    b = sm.get_bounding_box()
    out.append('%s slots %s box (%.0f, %.0f, %.0f)..(%.0f, %.0f, %.0f)' % (name, slots, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
'made: ' + ' | '.join(out)
