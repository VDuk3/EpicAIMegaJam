# castle_chest.py - owner, 2026-11-15: a simple pixel-art chest on the floor of the cell, that opens and gives one item.
# The chest is the game's own AGBChest (opens once, gives its Contents); this script makes its two pixel materials
# (pictures drawn by Tools/Jam/make_chest_art.js, shown once per face by M_PxGlass with no glow) and places it.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_chest.py
import unreal

DIR = '/Game/Glyphbound/Castle/Pixel'
SRC = 'C:/GameJam/Glyphbound/SourceArt/PixelArt/Chest/'
MAP = '/Game/Glyphbound/Maps/PastCastle'
SLAB = 'StaticMeshActor_21258'            # SM_Px_FloorCorridor7, the slab the owner selected
WEAPON = 'Staff_Ember'                    # the fire staff (owner, 2026-11-15)
eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world or not world.get_path_name().startswith(MAP):
    les.load_level(MAP)


def import_tex(name):
    task = unreal.AssetImportTask()
    task.filename = SRC + name + '.png'
    task.destination_path = DIR + '/Tiles'
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    t = unreal.load_asset(DIR + '/Tiles/' + name)
    t.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
    t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    t.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
    t.set_editor_property('srgb', True)
    t.set_editor_property('never_stream', True)
    eal.save_asset(DIR + '/Tiles/' + name, only_if_is_dirty=False)
    return t


def instance(name, tex):
    path = DIR + '/' + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', unreal.load_asset(DIR + '/M_PxGlass'))
    MEL.set_material_instance_texture_parameter_value(mi, 'Texture', tex)
    MEL.set_material_instance_scalar_parameter_value(mi, 'Glow', 0.0)
    eal.save_asset(path, only_if_is_dirty=False)
    return mi


instance('MI_Px_chest_body', import_tex('T_px_chest_body'))
instance('MI_Px_chest_lid', import_tex('T_px_chest_lid'))
instance('MI_Px_chest_lock', import_tex('T_px_chest_lock'))

# The owner moves and turns the chest by hand: an existing one is kept where it is.
slab = None
chest = None
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('Chest_Cell'):
        chest = a
    elif a.get_name() == SLAB:
        slab = a
o, e = slab.get_actor_bounds(False)
if chest is None:
    chest = eas.spawn_actor_from_class(unreal.GBChest, unreal.Vector(o.x, o.y, o.z + e.z), unreal.Rotator(0, 0, 0))
    chest.set_actor_label('Chest_Cell')
    chest.set_folder_path('Chests')
drop = unreal.GBDropDefinition()
drop.set_editor_property('weapon_id', WEAPON)
drop.set_editor_property('count', 1)
chest.set_editor_property('contents', [drop])
chest.set_editor_property('opened_flag', 'Chest.PastCastle.Cell')
saved = les.save_current_level()
'chest at %s yaw %.0f with %s, saved=%s' % (chest.get_actor_location(), chest.get_actor_rotation().yaw, WEAPON, saved)
