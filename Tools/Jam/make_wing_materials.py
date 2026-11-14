# make_wing_materials.py - materials our own level pieces need (owner's notes of 2026-11-14). Run in the Editor:
#   node Tools/Jam/mcp_call.js py Tools/Jam/make_wing_materials.py
# Makes, or rebuilds:
#   M_GB_Cracks             overlay on a thing that breaks: cracks that grow with its scalar "Stage" (0..1)
#   M_GB_FrostedGlass       an opaque glass wall: pale, glossy, nothing seen through it
#   M_GB_EnergyGate         the sheet of an energy door: see-through, with slow bright bands; scalar "Fade" (1 on, 0 out)
#   M_GB_LightFlicker       light function: a failing lamp (mostly off, with bursts)
#   M_ShipCorridor_Flicker  the same flicker on a glowing surface, in step with the lamp
#   M_ShipCorridor_Worn     the Spaceship Corridor's painted metal, worn: dirt and rust from two noise textures laid
#                           over the world (the model has no unwrapped UVs, and its author's wear is procedural in
#                           Blender), with instances for hull, floor grid and pipes; and a dead light strip.
# The crack mask is drawn by Tools/Jam/Make-CrackTexture.ps1.
import os

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Engine/Maps/Entry')   # never rebuild a material under a level that shows it
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
OUR = '/Game/Glyphbound/Materials'
SHIP = '/Game/Glyphbound/Environment/ShipCorridor'
NOISE_A = '/Game/HologramShader/Textures/Greyscale/T_Noise_02'
NOISE_B = '/Game/HologramShader/Textures/Greyscale/T_Noise_03'
out = []


def master(folder, name):
    path = folder + '/' + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    return m


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


def finish(m, folder, name):
    mel.recompile_material(m)
    eal.save_asset(folder + '/' + name, only_if_is_dirty=False)
    out.append(name)


def flicker(m, x, y):
    """0.04 most of the time, 1 in bursts: three sines of unrelated speeds added, and only their peaks let through."""
    t = node(m, 'Time', x, y)
    total = None
    for i, speed in enumerate((0.37, 1.9, 6.1)):
        mul = node(m, 'Multiply', x + 150, y + i * 90, const_b=speed)
        link(t, mul, 'A')
        sn = node(m, 'Sine', x + 300, y + i * 90)
        link(mul, sn)
        if total is None:
            total = sn
        else:
            add = node(m, 'Add', x + 450, y + i * 90)
            link(total, add, 'A')
            link(sn, add, 'B')
            total = add
    sub = node(m, 'Subtract', x + 600, y, const_b=0.55)
    link(total, sub, 'A')
    gain = node(m, 'Multiply', x + 750, y, const_b=9.0)
    link(sub, gain, 'A')
    sat = node(m, 'Saturate', x + 900, y)
    link(gain, sat)
    low = node(m, 'Add', x + 1050, y, const_b=0.04)
    link(sat, low, 'A')
    return low


# ---- the crack mask
tex_src = 'C:/GameJam/Glyphbound/SourceArt/Environment/Breakable/T_GB_Cracks.png'
if os.path.exists(tex_src):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', tex_src)
    task.set_editor_property('destination_path', OUR)
    task.set_editor_property('destination_name', 'T_GB_Cracks')
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    tools.import_asset_tasks([task])
cracks_tex = unreal.load_asset(OUR + '/T_GB_Cracks')

# ---- M_GB_Cracks: translucent, unlit, laid over the world's Y and Z (a wall across the X axis; 6 m per repeat)
m = master(OUR, 'M_GB_Cracks')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
wp = node(m, 'WorldPosition', -1100, 0)
yz = node(m, 'ComponentMask', -900, 0, r=False, g=True, b=True, a=False)
link(wp, yz)
uv = node(m, 'Divide', -750, 0, const_b=600.0)
link(yz, uv, 'A')
ts = node(m, 'TextureSample', -580, 0, texture=cracks_tex)
link(uv, ts, 'UVs')
stage = scalar(m, 'Stage', 0.0, -580, 250)
hidden = node(m, 'OneMinus', -400, 250)
link(stage, hidden)
over = node(m, 'Subtract', -250, 100)
link(ts, over, 'A', 'R')
link(hidden, over, 'B')
gain = node(m, 'Multiply', -100, 100, const_b=10.0)
link(over, gain, 'A')
sat = node(m, 'Saturate', 50, 100)
link(gain, sat)
opacity = node(m, 'Multiply', 200, 100, const_b=0.95)
link(sat, opacity, 'A')
mel.connect_material_property(opacity, '', MP.MP_OPACITY)
# Pale: the wall that breaks is frosted glass, and glass cracks white.
mel.connect_material_property(vector(m, 'Color', (0.55, 0.6, 0.64), 50, -150), '', MP.MP_EMISSIVE_COLOR)
finish(m, OUR, 'M_GB_Cracks')

# ---- M_GB_FrostedGlass (owner, 2026-11-14: "change it for a wall of opaque glass", then "more of a glass look, not
# so uniform"). Opaque. Panes 2 m wide in dark seams, with one seam across at half height; each pane clouded by two
# noise textures laid over the wall (large patches of lighter and darker glass, fine streaks in how glossy it is);
# a faint uneven glow, as of light behind it. Laid over the world's Y and Z: a wall across the X axis.
m = master(OUR, 'M_GB_FrostedGlass')
wp = node(m, 'WorldPosition', -2300, 0)
gy = node(m, 'ComponentMask', -2100, -100, r=False, g=True, b=False, a=False)
gz = node(m, 'ComponentMask', -2100, 100, r=False, g=False, b=True, a=False)
yz = node(m, 'ComponentMask', -2100, 300, r=False, g=True, b=True, a=False)
for p in (gy, gz, yz):
    link(wp, p)


def seam(coord, size, x, y):
    """1 on a thin line every `size` cm of `coord`, 0 elsewhere."""
    dv = node(m, 'Divide', x, y, const_b=size)
    link(coord, dv, 'A')
    fr = node(m, 'Frac', x + 130, y)
    link(dv, fr)
    off = node(m, 'Subtract', x + 260, y, const_b=0.5)
    link(fr, off, 'A')
    ab = node(m, 'Abs', x + 390, y)
    link(off, ab)
    edge = node(m, 'Subtract', x + 520, y, const_b=0.4875)
    link(ab, edge, 'A')
    gain = node(m, 'Multiply', x + 650, y, const_b=240.0)
    link(edge, gain, 'A')
    sat = node(m, 'Saturate', x + 780, y)
    link(gain, sat)
    return sat


frame = node(m, 'Max', -950, 0)
link(seam(gy, 200.0, -1900, -100), frame, 'A')
link(seam(gz, 281.0, -1900, 100), frame, 'B')


def cloud(size, tex, x, y):
    dv = node(m, 'Divide', x, y, const_b=size)
    link(yz, dv, 'A')
    ts = node(m, 'TextureSample', x + 150, y, texture=unreal.load_asset(tex))
    link(dv, ts, 'UVs')
    return ts


patches = cloud(430.0, NOISE_A, -1900, 350)
streaks = cloud(55.0, NOISE_B, -1900, 600)
pane = node(m, 'LinearInterpolate', -1300, 350)
link(vector(m, 'Dark', (0.04, 0.062, 0.072), -1600, 250), pane, 'A')
link(vector(m, 'Light', (0.095, 0.14, 0.16), -1600, 420), pane, 'B')
link(patches, pane, 'Alpha', 'R')
grain = node(m, 'LinearInterpolate', -1300, 600, const_a=0.75, const_b=1.25)
link(streaks, grain, 'Alpha', 'R')
clouded = node(m, 'Multiply', -1100, 450)
link(pane, clouded, 'A')
link(grain, clouded, 'B')
colour = node(m, 'LinearInterpolate', -700, 300)
link(clouded, colour, 'A')
link(node(m, 'Constant3Vector', -950, 200, constant=unreal.LinearColor(0.008, 0.009, 0.01, 1.0)), colour, 'B')
link(frame, colour, 'Alpha')
mel.connect_material_property(colour, '', MP.MP_BASE_COLOR)
gloss = node(m, 'LinearInterpolate', -1100, 750, const_a=0.1, const_b=0.5)
link(streaks, gloss, 'Alpha', 'R')
rough = node(m, 'LinearInterpolate', -700, 650, const_b=0.75)
link(gloss, rough, 'A')
link(frame, rough, 'Alpha')
mel.connect_material_property(rough, '', MP.MP_ROUGHNESS)
behind = node(m, 'Multiply', -1100, 950)
link(patches, behind, 'A', 'R')
link(patches, behind, 'B', 'R')
lit = node(m, 'Multiply', -900, 950)
link(behind, lit, 'A')
link(scalar(m, 'GlowStrength', 0.02, -1100, 1100), lit, 'B')
glow = node(m, 'Multiply', -700, 950)
link(vector(m, 'Glow', (0.35, 0.6, 0.65), -900, 1100), glow, 'A')
link(lit, glow, 'B')
clear = node(m, 'OneMinus', -700, 1150)
link(frame, clear)
shown = node(m, 'Multiply', -500, 1000)
link(glow, shown, 'A')
link(clear, shown, 'B')
mel.connect_material_property(shown, '', MP.MP_EMISSIVE_COLOR)
finish(m, OUR, 'M_GB_FrostedGlass')

# ---- M_GB_EnergyGate: what is behind it must stay in sight (owner: the Valkyrie has to be seen through the shut door)
m = master(OUR, 'M_GB_EnergyGate')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
wp = node(m, 'WorldPosition', -1500, 0)
height = node(m, 'ComponentMask', -1320, 0, r=False, g=False, b=True, a=False)
link(wp, height)
lines = node(m, 'Multiply', -1150, 0, const_b=0.025)
link(height, lines, 'A')
drift = node(m, 'Multiply', -1150, 150, const_b=0.35)
link(node(m, 'Time', -1320, 150), drift, 'A')
phase = node(m, 'Add', -1000, 50)
link(lines, phase, 'A')
link(drift, phase, 'B')
wave = node(m, 'Sine', -850, 50)
link(phase, wave)
half = node(m, 'Multiply', -700, 50, const_b=0.5)
link(wave, half, 'A')
band = node(m, 'Add', -550, 50, const_b=0.5)
link(half, band, 'A')
sharp = node(m, 'Multiply', -400, 50)
link(band, sharp, 'A')
link(band, sharp, 'B')
fade = scalar(m, 'Fade', 1.0, -400, 400)
bands = node(m, 'Multiply', -330, -50, const_b=0.5)
link(sharp, bands, 'A')
glow = node(m, 'Add', -250, -50, const_b=0.12)
link(bands, glow, 'A')
tinted = node(m, 'Multiply', -100, -100)
link(vector(m, 'Color', (0.03, 0.2, 0.3), -400, -250), tinted, 'A')
link(glow, tinted, 'B')
shown = node(m, 'Multiply', 50, -100)
link(tinted, shown, 'A')
link(fade, shown, 'B')
mel.connect_material_property(shown, '', MP.MP_EMISSIVE_COLOR)
thin = node(m, 'Multiply', -250, 200, const_b=0.14)
link(sharp, thin, 'A')
base = node(m, 'Add', -100, 200, const_b=0.05)
link(thin, base, 'A')
opacity = node(m, 'Multiply', 50, 200)
link(base, opacity, 'A')
link(fade, opacity, 'B')
mel.connect_material_property(opacity, '', MP.MP_OPACITY)
finish(m, OUR, 'M_GB_EnergyGate')

# ---- M_GB_LightFlicker: a light function
m = master(OUR, 'M_GB_LightFlicker')
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_LIGHT_FUNCTION)
mel.connect_material_property(flicker(m, -1300, 0), '', MP.MP_EMISSIVE_COLOR)
finish(m, OUR, 'M_GB_LightFlicker')

# ---- M_ShipCorridor_Flicker: a lamp's glowing face, flickering with the same rhythm
m = master(SHIP, 'M_ShipCorridor_Flicker')
glow = node(m, 'Multiply', -150, 0)
link(vector(m, 'Emissive', (1.0, 0.75, 0.45), -450, -150), glow, 'A')
level = node(m, 'Multiply', -300, 100)
link(scalar(m, 'EmissiveStrength', 14.0, -450, 50), level, 'A')
link(flicker(m, -1700, 300), level, 'B')
link(level, glow, 'B')
mel.connect_material_property(glow, '', MP.MP_EMISSIVE_COLOR)
mel.connect_material_property(node(m, 'Constant3Vector', -300, -350, constant=unreal.LinearColor(0.01, 0.01, 0.01, 1.0)), '', MP.MP_BASE_COLOR)
finish(m, SHIP, 'M_ShipCorridor_Flicker')

# ---- M_ShipCorridor_Worn
m = master(SHIP, 'M_ShipCorridor_Worn')
wp = node(m, 'WorldPosition', -2100, 0)
px = node(m, 'ComponentMask', -1900, -150, r=True, g=False, b=False, a=False)
py = node(m, 'ComponentMask', -1900, 0, r=False, g=True, b=False, a=False)
pz = node(m, 'ComponentMask', -1900, 150, r=False, g=False, b=True, a=False)
for p in (px, py, pz):
    link(wp, p)


def mix(a, b, k, x, y):
    """a + b * k"""
    mul = node(m, 'Multiply', x, y, const_b=k)
    link(b, mul, 'A')
    add = node(m, 'Add', x + 150, y)
    link(a, add, 'A')
    link(mul, add, 'B')
    return add


def sample(u, v, size, tex, x, y):
    ap = node(m, 'AppendVector', x, y)
    link(u, ap, 'A')
    link(v, ap, 'B')
    dv = node(m, 'Divide', x + 150, y, const_b=size)
    link(ap, dv, 'A')
    ts = node(m, 'TextureSample', x + 300, y, texture=unreal.load_asset(tex))
    link(dv, ts, 'UVs')
    return ts


# Two slanted projections, so no wall of the corridor shows streaks: (x + .31z, y + .47z) and (y - .29x, z + .41x).
big = sample(mix(px, pz, 0.31, -1700, -300), mix(py, pz, 0.47, -1700, -200), 260.0, NOISE_A, -1350, -250)
fine = sample(mix(py, px, -0.29, -1700, 100), mix(pz, px, 0.41, -1700, 200), 95.0, NOISE_B, -1350, 150)
both = node(m, 'Multiply', -850, -50)
link(big, both, 'A', 'R')
link(fine, both, 'B', 'R')
twice = node(m, 'Multiply', -700, -50, const_b=2.0)
link(both, twice, 'A')
edge = node(m, 'Subtract', -550, -50)
link(twice, edge, 'A')
link(scalar(m, 'Rust', 0.34, -700, 100), edge, 'B')     # lower = more rust
gain = node(m, 'Multiply', -400, -50, const_b=7.0)
link(edge, gain, 'A')
rust = node(m, 'Saturate', -250, -50)
link(gain, rust)
dirt = node(m, 'LinearInterpolate', -850, -400, const_a=0.45, const_b=1.0)
link(fine, dirt, 'Alpha', 'R')
paint = node(m, 'Multiply', -550, -450)
link(vector(m, 'Color', (0.08, 0.085, 0.09), -850, -600), paint, 'A')
link(dirt, paint, 'B')
colour = node(m, 'LinearInterpolate', -100, -350)
link(paint, colour, 'A')
link(vector(m, 'RustColor', (0.16, 0.065, 0.03), -550, -250), colour, 'B')
link(rust, colour, 'Alpha')
mel.connect_material_property(colour, '', MP.MP_BASE_COLOR)
metal = node(m, 'LinearInterpolate', -100, 100, const_b=0.05)
link(scalar(m, 'Metallic', 0.5, -400, 150), metal, 'A')
link(rust, metal, 'Alpha')
mel.connect_material_property(metal, '', MP.MP_METALLIC)
rough = node(m, 'LinearInterpolate', -100, 300, const_b=0.93)
link(scalar(m, 'Roughness', 0.6, -400, 350), rough, 'A')
link(rust, rough, 'Alpha')
mel.connect_material_property(rough, '', MP.MP_ROUGHNESS)
finish(m, SHIP, 'M_ShipCorridor_Worn')
worn = m

# ---- instances
def instance(name, parent, scalars=None, vectors=None):
    path = SHIP + '/' + name
    if not eal.does_asset_exist(path):
        tools.create_asset(name, SHIP, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    mel.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    out.append(name)


instance('MI_ShipCorridor_HullWorn', worn, {'Metallic': 0.5, 'Roughness': 0.62, 'Rust': 0.30}, {'Color': (0.075, 0.08, 0.088), 'RustColor': (0.15, 0.06, 0.028)})
instance('MI_ShipCorridor_GridWorn', worn, {'Metallic': 0.85, 'Roughness': 0.55, 'Rust': 0.24}, {'Color': (0.04, 0.04, 0.043), 'RustColor': (0.11, 0.045, 0.02)})
instance('MI_ShipCorridor_PipesWorn', worn, {'Metallic': 0.85, 'Roughness': 0.5, 'Rust': 0.28}, {'Color': (0.13, 0.125, 0.115), 'RustColor': (0.17, 0.07, 0.03)})
plain = unreal.load_asset(SHIP + '/M_ShipCorridor')
if plain:
    instance('MI_ShipCorridor_LightDead', plain, {'Metallic': 0.0, 'Roughness': 0.5, 'EmissiveStrength': 0.006}, {'Color': (0.03, 0.03, 0.03), 'Emissive': (1.0, 0.9, 0.8)})
    # The worn corridor is under water: its windows give a faint deep-sea glow instead of the black of the first corridor's.
    instance('MI_ShipCorridor_Water', plain, {'Metallic': 0.0, 'Roughness': 0.08, 'EmissiveStrength': 0.1}, {'Color': (0.0, 0.01, 0.015), 'Emissive': (0.02, 0.14, 0.2)})
instance('MI_ShipCorridor_LightFlicker', unreal.load_asset(SHIP + '/M_ShipCorridor_Flicker'), {'EmissiveStrength': 18.0}, {'Emissive': (1.0, 0.8, 0.55)})
'made: ' + ', '.join(out)
