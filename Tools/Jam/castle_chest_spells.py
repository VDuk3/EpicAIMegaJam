# castle_chest_spells.py - owner, 2026-11-15: a normal chest in the third cell (the slab SM_Px_FloorCorridor3) that
# gives the fire spellbook: every spell of the UnlockGroup "Fire" (Spells.json). Same pose as the other two.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_chest_spells.py
import unreal

MAP = '/Game/Glyphbound/Maps/PastCastle'
SPOT = unreal.Vector(12345.0, -3000.0, -951.5)
YAW = 90.0
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world or not world.get_path_name().startswith(MAP):
    les.load_level(MAP)
chest = None
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('Chest_FireSpells'):
        chest = a                                   # already placed: keep it where the owner left it
if chest is None:
    chest = eas.spawn_actor_from_class(unreal.GBChest, SPOT, unreal.Rotator(0, 0, YAW))
    chest.set_actor_label('Chest_FireSpells')
    chest.set_folder_path('Chests')
drop = unreal.GBDropDefinition()
drop.set_editor_property('spell_group', 'Fire')
chest.set_editor_property('contents', [drop])
chest.set_editor_property('opened_flag', 'Chest.PastCastle.FireSpells')
saved = les.save_current_level()
'chest at %s yaw %.0f, class %s, saved=%s' % (chest.get_actor_location(), chest.get_actor_rotation().yaw, chest.get_class().get_name(), saved)
