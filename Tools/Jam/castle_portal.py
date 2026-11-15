# castle_portal.py - owner, 2026-11-15: a pixel-art portal in the castle. The clip "Portal" is baked by GBPixelVfx from
# the Portal FX pack's NS_Portal_9 (orange ring of sparks); its facing is Fixed, so it stands as placed and does not
# turn to the player. Placed as an AGBTorch with no holder mesh (it plays a clip and carries the clip's light).
# Where it leads is not wired yet. The spot is provisional: the owner moves the actor.
# Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/castle_portal.py
import unreal

MAP = '/Game/Glyphbound/Maps/PastCastle'
SPOT = unreal.Vector(11290.0, -7000.0, -951.0 + 150.0)   # middle of the slab SM_Px_FloorCorridor13 (owner), ring centre 1.5 m up
YAW = 0.0
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not world or not world.get_path_name().startswith(MAP):
    les.load_level(MAP)
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('Portal_'):
        eas.destroy_actor(a)
# The clip's quad lies flat in the component's frame: a roll of 90 degrees stands it up (Rotator is roll, pitch, yaw).
p = eas.spawn_actor_from_class(unreal.GBTorch, SPOT, unreal.Rotator(90.0, 0.0, YAW))
p.set_actor_label('Portal_1')
p.set_folder_path('Portal')
p.set_editor_property('clip_id', 'Portal')
p.set_editor_property('flame_size', 1.0)
p.set_editor_property('light_offset', unreal.Vector(0, 0, 0))
saved = les.save_current_level()
'portal at %s, saved=%s' % (SPOT, saved)
