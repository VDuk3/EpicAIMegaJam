# place_tentacle_boss.py - owner, 2026-11-15: the tentacle boss in FutureBase's last deep hall, and the seal of its arena.
#   - GBEncounter "Boss_Tentacle" (Encounters.json; the machine Entity_Phase1, prespawned) in the middle of the hall;
#   - at the dark corridor's mouth, an invisible wall and a smoke system, both tagged EncounterBarrier + Boss_Tentacle:
#     GBEncounter.cpp turns them on once the fight is on and the player is inside the hall, off when it is cleared or reset.
# Same encounter entry as Build-FutureBase.py (ENCOUNTERS); the wall and the smoke live only here.
# Run in the Editor with FutureBase open and nobody else editing it:
#   node Tools/Jam/mcp_call.js py Tools/Jam/place_tentacle_boss.py
# It refuses during PIE, on another map, or when another map package is dirty; it backs up the .umap first, then
# saves only FutureBase.
import unreal


def run():
    import os, shutil, datetime
    MAP = '/Game/Glyphbound/Maps/FutureBase'
    TAG = 'GBBase'
    ENC = 'Boss_Tentacle'
    SMOKE = '/Game/Smoke_NiagaraFluids/FX/NS_Smoke_NF_01'
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if les.is_in_play_in_editor() or world is None:
        return 'NOT DONE: PIE is running'
    if not world.get_path_name().startswith(MAP + '.'):
        return 'NOT DONE: open map is %s' % world.get_path_name()
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    others = [d for d in dirty if d != MAP]
    if others:
        return 'NOT DONE: other dirty maps %s' % others

    umap = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), 'Glyphbound', 'Maps', 'FutureBase.umap')
    backup_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'MapBackups')
    os.makedirs(backup_dir, exist_ok=True)
    backup = os.path.join(backup_dir, 'FutureBase_%s.umap' % datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
    shutil.copy2(umap, backup)

    for a in eas.get_all_level_actors():
        if a.actor_has_tag(ENC) or (a.get_class().get_name() == 'GBEncounter' and str(a.get_editor_property('encounter_id')) == ENC):
            eas.destroy_actor(a)

    # the encounter: 15 x 13 m trigger in the hall (inner faces x 14762..18362, y -1400..1400, floor z -600)
    centre = unreal.Vector(16762.0, 0.0, -450.0)
    enc = eas.spawn_actor_from_class(unreal.GBEncounter, centre, unreal.Rotator(0, 0, 0))
    enc.set_editor_property('encounter_id', unreal.Name(ENC))
    enc.set_editor_property('presence', unreal.GBEraPresence.FUTURE_ONLY)
    enc.set_editor_property('trigger_extent', unreal.Vector(1500, 1300, 500))
    enc.set_editor_property('spawn_points', [unreal.Vector(16862 - centre.x, -200 - centre.y, -600 + 100.0 - centre.z)])
    enc.set_actor_label(ENC)
    enc.tags = [unreal.Name(TAG), unreal.Name('stage_encounters')]
    enc.set_folder_path(unreal.Name('Base/encounters/Gameplay'))

    # the seal: across the dark corridor's last metre (4 m wide, 3.5 m high), invisible, blocking only while sealed
    wall = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(14710.0, 0.0, -420.0), unreal.Rotator(0, 0, 0))
    smc = wall.static_mesh_component
    smc.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    smc.set_collision_profile_name('InvisibleWall')
    smc.set_editor_property('can_ever_affect_navigation', False)
    smc.set_editor_property('hidden_in_game', True)
    wall.set_actor_scale3d(unreal.Vector(0.4, 4.6, 3.8))
    wall.set_actor_label('Boss_Tentacle_SealWall')
    wall.tags = [unreal.Name('EncounterBarrier'), unreal.Name(ENC)]
    wall.set_folder_path(unreal.Name('Base/encounters/Gameplay'))

    smoke = eas.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(14740.0, 0.0, -600.0), unreal.Rotator(0, 0, 0))
    nc = smoke.niagara_component
    nc.set_asset(unreal.load_asset(SMOKE))
    nc.set_editor_property('auto_activate', False)
    smoke.set_actor_label('Boss_Tentacle_SealSmoke')
    smoke.tags = [unreal.Name('EncounterBarrier'), unreal.Name(ENC)]
    smoke.set_folder_path(unreal.Name('Base/encounters/Gameplay'))

    enc = wall = smoke = smc = nc = None
    saved = unreal.EditorLoadingAndSavingUtils.save_packages([unreal.load_package(MAP)], True)
    return 'placed Boss_Tentacle, seal wall and smoke; backup %s; saved=%s' % (backup, saved)


run()
