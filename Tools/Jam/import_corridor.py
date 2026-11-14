# import_corridor.py - brings the Spaceship Corridor (Tools/Jam/corridor_blender_export.py) into the project.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/import_corridor.py
# Imports the two FBX files and the picture for the screens, makes one plain master material with instances for the looks
# the owner approved (2026-11-13: painted metal without the model's procedural wear, a picture on the screens, dark
# glass), and assigns them by the model's material names. Idempotent: re-import replaces the meshes. Without the FBX
# files (they are deleted once imported) it only retunes the looks on the meshes already in the project.
SRC = 'C:/GameJam/Glyphbound/Saved/corridor_export/'
# Our own picture (Tools/Jam/Make-ScreenTexture.ps1). The model's own Screen.jpg is a stock infographic with a third
# party's watermark on it: not used.
SCREEN = 'C:/GameJam/Glyphbound/SourceArt/Environment/ShipCorridor/T_ShipCorridor_Screen.png'
DST = '/Game/Glyphbound/Environment/ShipCorridor'
import os
# Materials are never rebuilt under a level that shows them (render-thread assert seen on 2026-11-13).
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Engine/Maps/Entry')
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
out = []

# name starts with (compared without spaces, dots and case) -> (colour, metallic, roughness, emissive colour, strength)
LOOKS = [
    ('proceduralimperfections', 'Hull', (0.105, 0.11, 0.125), 0.55, 0.55, None, 0.0),
    ('roofgridmetal', 'Grid', (0.06, 0.06, 0.065), 0.90, 0.45, None, 0.0),
    ('metalpipes', 'Pipes', (0.20, 0.19, 0.18), 0.90, 0.35, None, 0.0),
    ('rubber', 'Rubber', (0.03, 0.03, 0.035), 0.0, 0.45, None, 0.0),
    ('floorwires', 'FloorWires', (0.45, 0.02, 0.02), 0.0, 0.60, None, 0.0),
    ('emmision', 'Light', (0.0, 0.0, 0.0), 0.0, 0.5, (1.0, 0.93, 0.82), 14.0),
    ('flickeringemmision', 'LightWarm', (0.0, 0.0, 0.0), 0.0, 0.5, (1.0, 0.75, 0.45), 8.0),
    ('glass', 'Glass', (0.02, 0.03, 0.04), 0.0, 0.08, None, 0.0),
    ('fireaxe', 'Axe', (0.30, 0.05, 0.04), 0.3, 0.5, None, 0.0),
    ('femain', 'ExtRed', (0.75, 0.01, 0.01), 0.58, 0.45, None, 0.0),
    ('febottom', 'ExtDark', (0.06, 0.0, 0.0), 0.58, 0.19, None, 0.0),
    ('febolt', 'ExtDark', (0.06, 0.0, 0.0), 0.58, 0.19, None, 0.0),
    ('fehandlestainless01', 'Steel', (0.38, 0.38, 0.38), 1.0, 0.15, None, 0.0),
    ('fehandlestainless02', 'SteelDark', (0.05, 0.05, 0.05), 1.0, 0.15, None, 0.0),
    ('felabel', 'Label', (0.75, 0.74, 0.70), 0.0, 0.4, None, 0.0),
    ('fepressuregauge', 'Label', (0.75, 0.74, 0.70), 0.0, 0.4, None, 0.0),
    ('feglass', 'Label', (0.75, 0.74, 0.70), 0.0, 0.4, None, 0.0),
    ('fe', 'Rubber', (0.03, 0.03, 0.035), 0.0, 0.45, None, 0.0),          # tube, rope, belt, range: black
    ('material002', 'CrateDark', (0.05, 0.03, 0.03), 0.0, 0.25, None, 0.0),
    ('material003', 'CrateRed', (0.46, 0.09, 0.07), 0.0, 0.5, None, 0.0),
    ('material004', 'CrateDark', (0.05, 0.03, 0.03), 0.0, 0.25, None, 0.0),
    ('material005', 'CrateBrown', (0.17, 0.10, 0.08), 0.0, 0.5, None, 0.0),
    ('material006', 'CrateBrown', (0.17, 0.10, 0.08), 0.0, 0.5, None, 0.0),
    ('material', 'CrateOrange', (0.71, 0.14, 0.03), 0.0, 0.25, None, 0.0),
]


def norm(name):
    return ''.join(c for c in str(name).lower() if c.isalnum())


def scalar(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', value)
    return e


def vector(m, name, value, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', unreal.LinearColor(value[0], value[1], value[2], 1.0))
    return e


def master(name):
    path = DST + '/' + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, DST, unreal.Material, unreal.MaterialFactoryNew())
    return m


# ---- masters
def plain_master(name, two_sided):
    m = master(name)
    m.set_editor_property('two_sided', two_sided)
    mel.connect_material_property(vector(m, 'Color', (0.2, 0.2, 0.2), -500, -200), '', MP.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, 'Metallic', 0.0, -500, 0), '', MP.MP_METALLIC)
    mel.connect_material_property(scalar(m, 'Roughness', 0.5, -500, 100), '', MP.MP_ROUGHNESS)
    em = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 300)
    mel.connect_material_expressions(vector(m, 'Emissive', (0.0, 0.0, 0.0), -500, 250), '', em, 'A')
    mel.connect_material_expressions(scalar(m, 'EmissiveStrength', 0.0, -500, 450), '', em, 'B')
    mel.connect_material_property(em, '', MP.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_asset(DST + '/' + name, only_if_is_dirty=False)
    return m


base = plain_master('M_ShipCorridor', False)
# The crates' shells are one face thick and some of their faces look inwards: seen from outside, half of each crate was
# missing in the game (owner, 2026-11-14). Blender draws both sides of a face; Unreal does only with a two-sided material.
base_two_sided = plain_master('M_ShipCorridor_TwoSided', True)

task = unreal.AssetImportTask()
task.set_editor_property('filename', SCREEN)
task.set_editor_property('destination_path', DST)
task.set_editor_property('destination_name', 'T_ShipCorridor_Screen')
task.set_editor_property('replace_existing', True)
task.set_editor_property('automated', True)
task.set_editor_property('save', True)
tools.import_asset_tasks([task])
screen_tex = unreal.load_asset(DST + '/T_ShipCorridor_Screen')

m = master('M_ShipCorridor_Screen')
ts = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -600, 0)
ts.set_editor_property('texture', screen_tex)
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 0)
mel.connect_material_expressions(ts, 'RGB', mul, 'A')
mel.connect_material_expressions(scalar(m, 'Strength', 1.0, -600, 250), '', mul, 'B')
mel.connect_material_property(mul, '', MP.MP_EMISSIVE_COLOR)
dark = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, -200)
dark.set_editor_property('constant', unreal.LinearColor(0.01, 0.01, 0.012, 1.0))
mel.connect_material_property(dark, '', MP.MP_BASE_COLOR)
mel.recompile_material(m)
eal.save_asset(DST + '/M_ShipCorridor_Screen', only_if_is_dirty=False)
screen_mat = m

# ---- instances
instances = {}
for prefix, look, color, metal, rough, emissive, strength in LOOKS:
    if look in instances:
        continue
    name = 'MI_ShipCorridor_' + look
    path = DST + '/' + name
    if not eal.does_asset_exist(path):
        tools.create_asset(name, DST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    mel.set_material_instance_parent(mi, base_two_sided if look.startswith('Crate') else base)
    mel.set_material_instance_vector_parameter_value(mi, 'Color', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
    mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
    if emissive:
        mel.set_material_instance_vector_parameter_value(mi, 'Emissive', unreal.LinearColor(emissive[0], emissive[1], emissive[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, 'EmissiveStrength', strength)
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    instances[look] = mi

# ---- meshes (legacy FBX importer: it honours FbxImportUI)
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX false')
for name in ('SM_ShipCorridor_Body', 'SM_ShipCorridor_Details', 'SM_ShipCorridor_DoorA', 'SM_ShipCorridor_DoorB'):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.static_mesh_import_data
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('remove_degenerates', False)
    data.set_editor_property('convert_scene', True)
    data.set_editor_property('import_uniform_scale', 1.0)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', SRC + name + '.fbx')
    task.set_editor_property('destination_path', DST)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('options', ui)
    if os.path.exists(SRC + name + '.fbx'):
        tools.import_asset_tasks([task])
        await asyncio.sleep(0)
    sm = unreal.load_asset(DST + '/' + name)
    if not sm:
        out.append('%s: IMPORT FAILED' % name)
        continue
    unmatched = []
    mats = sm.static_materials
    for i, slot in enumerate(mats):
        key = norm(slot.material_slot_name)
        chosen = None
        if key.startswith('screen'):
            chosen = screen_mat
        else:
            for prefix, look, *_ in LOOKS:
                if key.startswith(prefix):
                    chosen = instances[look]
                    break
        if chosen:
            sm.set_material(i, chosen)
        else:
            unmatched.append(str(slot.material_slot_name))
    if not name.endswith('Details'):          # the body and the door leaves block with their own triangles
        bs = sm.get_editor_property('body_setup')
        bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_asset(DST + '/' + name, only_if_is_dirty=False)
    b = sm.get_bounding_box()
    out.append('%s: %d slots, unmatched %s, bounds (%d, %d, %d) .. (%d, %d, %d)' % (
        name, len(mats), unmatched, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
'\n'.join(out)
