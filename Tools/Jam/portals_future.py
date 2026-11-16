# portals_future.py - 2026-11-15: the portals of FutureBase (route: Design/2026-11-15_PortalRoute.md).
#   Portal_ToPast   in the control room, behind the seal: opens PastCastle, arrival at the dungeon portal
#                   (GBArrival=Dungeon). Always usable (no gate: a locked portal could soft-lock the route).
#   Arrival_Base    where the castle's dungeon portal arrives (GBArrival=Base), in front of Portal_ToPast.
#   FinalPortal     AGBFinalPortal in the control room's south side; appears with FinalPortalOpen (both quest items
#                   combined) and opens Game.json EntityMapPath (EntityArena).
# Look: the SciFiWorld gate frame (500 x 350) with the Portal FX pack's surface swirl in its opening, and a cold light.
# Logic is AGBPortal in map-portal mode, configured by tags (see GBPortal.cpp).
# Run in the Editor with FutureBase loaded: node Tools/Jam/mcp_call.js py Tools/Jam/portals_future.py
import os
import shutil
import datetime

import unreal

MAP = '/Game/Glyphbound/Maps/FutureBase'
TAG = 'GBPortalsScript'
PORTAL = (-1500.0, 60.0, 0.0)       # floor under the portal, west end of the control room, opening facing +X
FINAL = (-1100.0, -750.0, 0.0)


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or not world.get_path_name().startswith(MAP):
        return 'FutureBase is not the open map; nothing done'
    # dated backup of the map file before any edit
    src = os.path.join(unreal.Paths.project_content_dir(), 'Glyphbound', 'Maps', 'FutureBase.umap')
    bdir = os.path.join(unreal.Paths.project_saved_dir(), 'MapBackups')
    os.makedirs(bdir, exist_ok=True)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    shutil.copy2(os.path.abspath(src), os.path.join(bdir, 'FutureBase_%s_portals.umap' % stamp))

    for a in eas.get_all_level_actors():
        if a.actor_has_tag(TAG):
            eas.destroy_actor(a)

    px, py, pz = PORTAL
    # probes: floor under each spot and a capsule's room at the arrival (the hand-over teleports without a sweep)
    probes = []
    for name, (x, y) in (('portal', (px, py)), ('arrival', (px + 280.0, py)), ('final', FINAL[:2])):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 150), unreal.Vector(x, y, -200),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        floor = hit.to_tuple()[4].z if hit else None
        blocked = unreal.SystemLibrary.capsule_overlap_actors(world, unreal.Vector(x, y, 100), 45.0, 95.0, [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2], unreal.Actor, [])
        blocked = (blocked[-1] if isinstance(blocked, tuple) else blocked) or []
        blocked = [a.get_actor_label() for a in blocked if not a.actor_has_tag(TAG)]
        probes.append('%s floor=%s blocked=%s' % (name, floor, blocked))
        if floor is None or abs(floor - pz) > 30:
            return 'ABORT, nothing saved: %s' % probes
    made = []

    def tagged(actor, label, extra=()):
        actor.set_actor_label(label)
        actor.set_folder_path('Portals')
        actor.tags = [unreal.Name(TAG)] + [unreal.Name(t) for t in extra]
        made.append(label)
        return actor

    # logic
    portal = eas.spawn_actor_from_class(unreal.GBPortal, unreal.Vector(px + 40.0, py, pz), unreal.Rotator(0, 0, 0))
    tagged(portal, 'Portal_ToPast', ['GBMap=/Game/Glyphbound/Maps/PastCastle', 'GBArrive=Dungeon', 'GBNoFrame'])
    # look: frame
    frame = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(px, py, pz), unreal.Rotator(0, 0, 0))
    frame.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/SciFiWorld/Modules/Meshes/SM_MGate01-500x350-1_GateFrame'))
    tagged(frame, 'Portal_ToPast_Frame')
    # look: the swirl in the opening (the surface mesh is 200 x 200 in Y-Z, centred)
    surf = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(px, py, pz + 170.0), unreal.Rotator(0, 0, 0))
    smc = surf.static_mesh_component
    smc.set_static_mesh(unreal.load_asset('/Game/PortalFX/Meshes/SM_PortalSurface'))
    smc.set_material(0, unreal.load_asset('/Game/PortalFX/Materials/MI_PortalSurface_4'))
    smc.set_collision_profile_name('NoCollision')
    smc.set_editor_property('cast_shadow', False)
    surf.set_actor_scale3d(unreal.Vector(1.0, 2.15, 1.65))
    tagged(surf, 'Portal_ToPast_Surface')
    # its background disc just behind, so the refractive swirl reads against something
    bg = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(px - 4.0, py, pz + 170.0), unreal.Rotator(0, 0, 0))
    bsm = bg.static_mesh_component
    bsm.set_static_mesh(unreal.load_asset('/Game/PortalFX/Meshes/SM_PortalSurface_BG'))
    bsm.set_material(0, unreal.load_asset('/Game/PortalFX/Materials/MI_PortalSurface_BG_2'))
    bsm.set_collision_profile_name('NoCollision')
    bsm.set_editor_property('cast_shadow', False)
    bg.set_actor_scale3d(unreal.Vector(1.0, 2.15, 1.65))
    tagged(bg, 'Portal_ToPast_SurfaceBG')
    light = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(px + 120.0, py, pz + 180.0), unreal.Rotator(0, 0, 0))
    lc = light.point_light_component
    lc.set_intensity(6.0)
    lc.set_light_color(unreal.LinearColor(0.35, 0.85, 1.0, 1.0))
    lc.set_attenuation_radius(700.0)
    lc.set_cast_shadows(False)
    tagged(light, 'Portal_ToPast_Light')
    # arrival from the castle
    arr = eas.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(px + 280.0, py, pz), unreal.Rotator(0, 0, 0))
    tagged(arr, 'Arrival_Base', ['GBArrival=Base'])
    # the rift to the Entity
    fx, fy, fz = FINAL
    final = eas.spawn_actor_from_class(unreal.GBFinalPortal, unreal.Vector(fx, fy, fz), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
    tagged(final, 'FinalPortal')

    saved = les.save_current_level()
    return 'made %s, saved=%s, probes %s' % (made, saved, probes)


main()
