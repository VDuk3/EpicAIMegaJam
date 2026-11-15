# castle_mimic.py - owner, 2026-11-15: a mimic in the cell next to the chest of the fire staff. It is AGBMimic, the
# same chest (same boxes and pixel materials) until the player tries to open it or hits it.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_mimic.py
import unreal

MAP = '/Game/Glyphbound/Maps/PastCastle'
SPOT = unreal.Vector(12345.0, -3800.0, -951.5)      # the cell north of the chest's (cells at y -4600, -3800, -3000), same pose
YAW = 90.0
REWARD = ('HealthPotion', 2)                        # provisional: what it drops when killed
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world or not world.get_path_name().startswith(MAP):
    les.load_level(MAP)
mimic = None
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('Mimic_Cell'):
        mimic = a                                   # already placed: the owner may have moved it, keep it there
if mimic is None:
    mimic = eas.spawn_actor_from_class(unreal.GBMimic, SPOT, unreal.Rotator(0, 0, YAW))
    mimic.set_actor_label('Mimic_Cell')
    mimic.set_folder_path('Chests')
drop = unreal.GBDropDefinition()
drop.set_editor_property('item_id', REWARD[0])
drop.set_editor_property('count', REWARD[1])
mimic.set_editor_property('contents', [drop])
mimic.set_editor_property('opened_flag', 'Mimic.PastCastle.Cell')
saved = les.save_current_level()
'mimic at %s yaw %.0f, saved=%s' % (mimic.get_actor_location(), mimic.get_actor_rotation().yaw, saved)
