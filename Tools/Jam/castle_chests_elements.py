# castle_chests_elements.py - owner, 2026-11-15: the two missing elemental spellbooks (Lightning, Ice) and the two missing
# staffs (Staff_Storm, Staff_Frost), each in a chest (AGBChest, same pixel look as the cell chests), spread along the
# castle route after its fights:
#   Front Hall back   Staff_Storm       (early: a second element for the first halls)
#   King's Hall       Lightning book    (after the fire zone's fight)
#   Side Hall         Staff_Frost and the Ice book (after the ice zone's fight, before the Top Level: the past Valkyrie
#                     takes more ice damage, IceDamageTaken 1.25)
# Each spot is traced to the floor; a spot whose floor is not near the expected height is skipped and reported.
# Run in the Editor with PastCastle open: node Tools/Jam/mcp_call.js py Tools/Jam/castle_chests_elements.py
import os
import shutil
import datetime

import unreal

MAP = '/Game/Glyphbound/Maps/PastCastle'
CHESTS = [
    # label, x, y, expected floor z, yaw (the chest opens towards its local +Y: turned to face the room), weapon id, spell group
    ('Chest_StormStaff', 7500.0, -600.0, 250.0, 90.0, 'Staff_Storm', ''),
    ('Chest_LightningSpells', 8400.0, -4600.0, 250.0, 0.0, '', 'Lightning'),
    ('Chest_FrostStaff', 8200.0, 4500.0, 250.0, 180.0, 'Staff_Frost', ''),
    ('Chest_IceSpells', 10000.0, 4450.0, 250.0, 180.0, '', 'Ice'),
]


def floor_at(world, x, y, z):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 300.0), unreal.Vector(x, y, z - 300.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or not world.get_path_name().startswith(MAP):
        return 'PastCastle is not the open map; nothing done'
    src = os.path.join(unreal.Paths.project_content_dir(), 'Glyphbound', 'Maps', 'PastCastle.umap')
    bdir = os.path.join(unreal.Paths.project_saved_dir(), 'MapBackups')
    os.makedirs(bdir, exist_ok=True)
    shutil.copy2(os.path.abspath(src), os.path.join(bdir, 'PastCastle_%s_chests.umap' % datetime.datetime.now().strftime('%Y%m%d-%H%M%S')))

    labels = [c[0] for c in CHESTS]
    for a in eas.get_all_level_actors():
        if a.get_actor_label() in labels:
            eas.destroy_actor(a)
    report = []
    for label, x, y, z, yaw, weapon, group in CHESTS:
        f = floor_at(world, x, y, z)
        if f is None or abs(f - z) > 80.0:
            report.append('%s SKIPPED (floor %s)' % (label, f))
            continue
        chest = eas.spawn_actor_from_class(unreal.GBChest, unreal.Vector(x, y, f), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        chest.set_actor_label(label)
        chest.set_folder_path('Chests')
        drop = unreal.GBDropDefinition()
        if weapon:
            drop.set_editor_property('weapon_id', weapon)
            drop.set_editor_property('count', 1)
        else:
            drop.set_editor_property('spell_group', group)
        chest.set_editor_property('contents', [drop])
        chest.set_editor_property('opened_flag', 'Chest.PastCastle.' + label[len('Chest_'):])
        report.append('%s at (%.0f,%.0f,%.0f)' % (label, x, y, f))
    saved = les.save_current_level()
    return '%s, saved=%s' % (report, saved)


main()
