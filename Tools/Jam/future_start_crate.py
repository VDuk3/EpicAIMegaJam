# future_start_crate.py - places the katana crate at the future base's start (owner, 2026-11-15).
# Run inside the Editor with FutureBase open (node Tools/Jam/mcp_call.js py Tools/Jam/future_start_crate.py, or the MCP's
# execute_python). Idempotent: an actor labelled "KatanaCrate" is reused. It does not save the map.
# Where: in front of the white closed panel (SM_Closed_01, wall at y = 1100) on the spawn's right-hand side, which the
# owner marked on a screenshot. The chest's front (+Y local) turns to the room (yaw 180), so leftovers fall into the room.
import unreal

LABEL = 'KatanaCrate'
CRATE = '/Game/High_Tech_Base/Models/SM_Prop_02.SM_Prop_02'   # the bay's cover crate, scaled down to a small box
SCALE = 0.4                                                     # 184 cm -> about 74 cm
LOCATION = unreal.Vector(-940.0, 1100.0 - 0.5 * 200.0 * SCALE - 8.0, 10.0)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'FutureBase', 'open FutureBase first, not ' + world.get_name()

chest = next((a for a in eas.get_all_level_actors() if a.get_actor_label() == LABEL), None)
if chest is None:
    chest = eas.spawn_actor_from_class(unreal.load_class(None, '/Script/Glyphbound.GBChest'), LOCATION, unreal.Rotator(0, 0, 180))
    chest.set_actor_label(LABEL)
chest.set_actor_location_and_rotation(LOCATION, unreal.Rotator(roll=0, pitch=0, yaw=180), False, False)
chest.tags = [unreal.Name('ChestMesh=' + CRATE), unreal.Name('ChestScale=%g' % SCALE)]
drop = unreal.GBDropDefinition()
drop.set_editor_property('weapon_id', 'CrimsonKatana')
drop.set_editor_property('count', 1)
drop.set_editor_property('chance', 1.0)
chest.set_editor_property('contents', [drop])
chest.set_editor_property('opened_flag', 'Chest.FutureStartKatana')
_RESULT = '%s at %s, contents %s' % (chest.get_actor_label(), chest.get_actor_location(), chest.get_editor_property('contents'))
