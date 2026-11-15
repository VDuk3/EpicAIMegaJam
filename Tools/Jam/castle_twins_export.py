# castle_twins_export.py - writes the geometry of every castle-pack mesh the castle map uses, for the pixel twins.
# Run in the Editor on the map PastCastle: node Tools/Jam/mcp_call.js py Tools/Jam/castle_twins_export.py
# Output: <Saved>/CastleTwins/src/<mesh name>.json  {"path", "bounds": [min, max], "mats": [slot material names],
#         "v": [x, y, z, ...] (cm, the mesh's own frame), "t": [i0, i1, i2, ...], "m": [slot of each triangle]}
#         and <Saved>/CastleTwins/src/_used.json: every pack mesh in the map with its count of placed pieces.
# The twins (Tools/Jam/castle_twins.js) are made from these files outside the Editor.
import json
import os

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'CastleTwins', 'src')
os.makedirs(OUT, exist_ok=True)
CFG_PATH = os.path.join(unreal.Paths.project_saved_dir(), 'castle_twins.json')
CFG = json.load(open(CFG_PATH)) if os.path.exists(CFG_PATH) else {}
ONLY = CFG.get('export_only')            # optional list of mesh names
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
MAP = '/Game/Glyphbound/Maps/PastCastle'
if ues.get_editor_world().get_path_name().split('.')[0] != MAP:
    les.load_level(MAP)

used = {}
for a in eas.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    sm = a.static_mesh_component.static_mesh
    if not sm:
        continue
    p = sm.get_path_name().split('.')[0]
    if not p.startswith('/Game/CastleTown/'):
        continue
    used[p] = used.get(p, 0) + 1

done = []
for p in sorted(used):
    name = p.split('/')[-1]
    if ONLY and name not in ONLY:
        continue
    f = os.path.join(OUT, name + '.json')
    if os.path.exists(f) and not CFG.get('export_again'):
        continue
    sm = unreal.load_asset(p)
    sm.set_editor_property('allow_cpu_access', True)
    v, t, m = [], [], []
    for sec in range(sm.get_num_sections(0)):
        r = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, sec)
        base = len(v) // 3
        for q in r[0]:
            v += [round(q.x, 2), round(q.y, 2), round(q.z, 2)]
        t += [base + i for i in r[1]]
        m += [sec] * (len(r[1]) // 3)
    # A section's material slot is not always its index: ask the mesh.
    slots = [sl.material_interface.get_name() if sl.material_interface else '' for sl in sm.static_materials]
    sec_mat = []
    for sec in range(sm.get_num_sections(0)):
        idx = sm.get_material_index_from_section(0, sec) if hasattr(sm, 'get_material_index_from_section') else sec
        sec_mat.append(slots[idx] if 0 <= idx < len(slots) else '')
    b = sm.get_bounding_box()
    json.dump({'path': p, 'bounds': [[b.min.x, b.min.y, b.min.z], [b.max.x, b.max.y, b.max.z]], 'mats': sec_mat,
               'v': v, 't': t, 'm': m}, open(f, 'w'))
    done.append(name)
json.dump(used, open(os.path.join(OUT, '_used.json'), 'w'), indent=1)
'%d pack meshes in the map, %d exported now' % (len(used), len(done))
