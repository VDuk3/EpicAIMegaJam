# portals_castle.py - 2026-11-15: the portals of PastCastle (route: Design/2026-11-15_PortalRoute.md).
#   Event_CastleDoor  owner: the FIRST time the player enters the castle itself (the keep's door, not the outer
#                     gate), the caption CastleDoor plays and FutureBase opens (arrival at its player start, by the
#                     seal). Walk-in, once (flag CastleDoorEvent); also sets SealUsed so the crypt's seal goes dark.
#   Portal_Dungeon    the logic of the pixel portal Portal_1 (castle_portal.py) in the dungeon by the seal's hall:
#                     opens FutureBase, arrival in front of its Portal_ToPast (GBArrival=Base). Usable once SealUsed.
#   Arrival_Dungeon   where the future's Portal_ToPast arrives (GBArrival=Dungeon), in front of the pixel portal.
# Logic is AGBPortal in map-portal mode, configured by tags (see GBPortal.cpp). The pixel look stays Portal_1.
# Run in the Editor with PastCastle loaded: node Tools/Jam/mcp_call.js py Tools/Jam/portals_castle.py
import os
import shutil
import datetime

import unreal

MAP = '/Game/Glyphbound/Maps/PastCastle'
TAG = 'GBPortalsScript'
DOOR = (5450.0, 0.0, 250.0)          # the keep's door: the arched passage (Back_Arch3 at 5350, Back_Arch2 at 5690, 3 m wide) into the keep
DUNGEON = (11290.0, -7000.0, -951.0)  # floor under the pixel portal Portal_1
ARRIVE_DUNGEON = (11290.0, -6650.0, -951.0)


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or not world.get_path_name().startswith(MAP):
        return 'PastCastle is not the open map; nothing done'
    src = os.path.join(unreal.Paths.project_content_dir(), 'Glyphbound', 'Maps', 'PastCastle.umap')
    bdir = os.path.join(unreal.Paths.project_saved_dir(), 'MapBackups')
    os.makedirs(bdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    shutil.copy2(os.path.abspath(src), os.path.join(bdir, 'PastCastle_%s_portals.umap' % stamp))

    for a in eas.get_all_level_actors():
        if a.actor_has_tag(TAG):
            eas.destroy_actor(a)
    pixel = [a for a in eas.get_all_level_actors() if a.get_actor_label() == 'Portal_1']
    dx, dy, dz = DUNGEON
    if pixel:
        l = pixel[0].get_actor_location()
        dx, dy = l.x, l.y
    probes = []
    for name, (x, y, z) in (('door', DOOR), ('dungeon', (dx, dy, dz)), ('arrival', (dx, ARRIVE_DUNGEON[1], ARRIVE_DUNGEON[2]))):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 150), unreal.Vector(x, y, z - 200),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        floor = hit.to_tuple()[4].z if hit else None
        side = []
        for sy in (1, -1):
            h2 = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 120), unreal.Vector(x, y + sy * 800, z + 120),
                                                        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
            side.append(round(h2.to_tuple()[4].y - y) if h2 else None)
        probes.append('%s floor=%s sides=%s' % (name, floor, side))
        if floor is None or abs(floor - z) > 60:
            return 'ABORT, nothing saved: %s' % probes
    made = []

    def tagged(actor, label, extra=()):
        actor.set_actor_label(label)
        actor.set_folder_path('Portals')
        actor.tags = [unreal.Name(TAG)] + [unreal.Name(t) for t in extra]
        made.append(label)
        return actor

    door = eas.spawn_actor_from_class(unreal.GBPortal, unreal.Vector(*DOOR), unreal.Rotator(0, 0, 0))
    tagged(door, 'Event_CastleDoor', ['GBMap=/Game/Glyphbound/Maps/FutureBase', 'GBOnEnter', 'GBRadius=200',
                                      'GBOnceFlag=CastleDoorEvent', 'GBSetFlag=SealUsed', 'GBCutscene=CastleDoor', 'GBNoFrame'])
    dun = eas.spawn_actor_from_class(unreal.GBPortal, unreal.Vector(dx, dy, dz), unreal.Rotator(0, 0, 0))
    tagged(dun, 'Portal_Dungeon', ['GBMap=/Game/Glyphbound/Maps/FutureBase', 'GBArrive=Base', 'GBRequires=SealUsed', 'GBNoFrame'])
    ax, ay, az = ARRIVE_DUNGEON
    arr = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(dx, ay, az), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
    tagged(arr, 'Arrival_Dungeon', ['GBArrival=Dungeon'])

    saved = les.save_current_level()
    return 'made %s (pixel portal found: %s at %.0f,%.0f), saved=%s, probes %s' % (made, bool(pixel), dx, dy, saved, probes)


main()
