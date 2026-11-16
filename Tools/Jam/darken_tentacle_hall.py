# darken_tentacle_hall.py - owner, 2026-11-15: "elimina toda la luz de esa sala" (the tentacle boss's hall, not the corridor).
# Deletes every light actor inside the hall (inner faces x 14762..18362, y -1400..1400, floor z -600, 12 m high) in
# FutureBase; Build-FutureBase.py no longer places them. Only the boss's own glow and its eye laser are left.
# Run with FutureBase open and nobody else editing it: node Tools/Jam/mcp_call.js py Tools/Jam/darken_tentacle_hall.py
# Refuses during PIE, on another map or with another map dirty; backs up the .umap first; saves only FutureBase.
import unreal, os, shutil, datetime


def run():
    MAP = '/Game/Glyphbound/Maps/FutureBase'
    X0, X1, Y0, Y1, Z0, Z1 = 14762.0, 18362.0, -1400.0, 1400.0, -700.0, 700.0
    EXPOSURE = 1.0                  # fixed adaptation (min = max brightness): the base's lit rooms adapt around this
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if les.is_in_play_in_editor() or world is None:
        return 'NOT DONE: PIE is running'
    if not world.get_path_name().startswith(MAP + '.'):
        return 'NOT DONE: open map is %s' % world.get_path_name()
    others = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() if p.get_name() != MAP]
    if others:
        return 'NOT DONE: other dirty maps %s' % others
    umap = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), 'Glyphbound', 'Maps', 'FutureBase.umap')
    backup_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'MapBackups')
    os.makedirs(backup_dir, exist_ok=True)
    backup = os.path.join(backup_dir, 'FutureBase_%s_before-dark-hall.umap' % datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
    shutil.copy2(umap, backup)
    gone = []
    for a in eas.get_all_level_actors():
        if not isinstance(a, unreal.Light):
            continue
        l = a.get_actor_location()
        if X0 <= l.x <= X1 and Y0 <= l.y <= Y1 and Z0 <= l.z <= Z1:
            gone.append('%s (%.0f, %.0f, %.0f)' % (a.get_actor_label(), l.x, l.y, l.z))
            eas.destroy_actor(a)
    # the wing's own exposure (histogram, down to 0.03) brightens a dark hall back up: the boss's hall gets a fixed
    # exposure, as if lit like the base, so what has no light stays dark
    for old in eas.get_all_level_actors():
        if old.get_actor_label() == 'Boss_Tentacle_Exposure':
            eas.destroy_actor(old)
    ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector((X0 + X1) / 2.0, 0.0, 0.0), unreal.Rotator(0, 0, 0))
    ppv.set_actor_label('Boss_Tentacle_Exposure')
    ppv.set_folder_path(unreal.Name('Base/encounters/Gameplay'))
    ppv.set_actor_scale3d(unreal.Vector((X1 - X0) / 200.0, (Y1 - Y0) / 200.0, (Z1 - Z0) / 200.0))
    ppv.set_editor_property('unbound', False)
    ppv.set_editor_property('priority', 10.0)
    ppv.set_editor_property('blend_radius', 150.0)
    st = ppv.get_editor_property('settings')
    for name, value in (('auto_exposure_method', unreal.AutoExposureMethod.AEM_HISTOGRAM), ('auto_exposure_bias', 0.0),
                        ('auto_exposure_min_brightness', EXPOSURE), ('auto_exposure_max_brightness', EXPOSURE)):
        st.set_editor_property('override_' + name, True)
        st.set_editor_property(name, value)
    ppv.set_editor_property('settings', st)
    ppv = st = None
    a = l = None
    saved = unreal.EditorLoadingAndSavingUtils.save_packages([unreal.load_package(MAP)], True)
    return 'exposure fixed at %s; removed %d lights: %s; backup %s; saved=%s' % (EXPOSURE, len(gone), ', '.join(gone), backup, saved)


run()
