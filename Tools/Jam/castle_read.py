# castle_read.py - reads the castle pack (Medieval Kingdom) in the Editor without changing it:
#   1. every level named in the settings is opened and written to JSON: each static mesh component (instanced ones
#      too) with its world transform and bounds, each light, each level instance with the level it places;
#   2. every static mesh under the named folders is measured: bounds, triangles, materials, simple collision.
# The author's assembled castle is the reference for how the pieces go together (owner, 2026-11-14), as the base
# kits' demo maps were for the future base.
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/castle_read.py
# Settings: Glyphbound/Saved/castle_read.json
#   {"out": "C:/GameJam/Design/Levels/kits/castle/", "levels": ["/Game/CastleTown/Levels/..."],
#    "mesh_folders": ["/Game/CastleTown/Static_Mesh/Castle_Kit"], "pieces_out": "pieces.json"}
import json
import os

import unreal

CFG = json.load(open(os.path.join(unreal.Paths.project_saved_dir(), 'castle_read.json')))
OUT = CFG['out']
os.makedirs(OUT, exist_ok=True)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def xf(t):
    l, r, s = t.translation, t.rotation.rotator(), t.scale3d
    return {'loc': [round(l.x, 1), round(l.y, 1), round(l.z, 1)], 'rot': [round(r.pitch, 2), round(r.yaw, 2), round(r.roll, 2)],
            'scale': [round(s.x, 3), round(s.y, 3), round(s.z, 3)]}


def read_level(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return {'level': path, 'missing': True}
    LES.load_level(path)
    items, classes = [], {}
    lo, hi = [1e9] * 3, [-1e9] * 3
    for a in EAS.get_all_level_actors():
        cn = a.get_class().get_name()
        classes[cn] = classes.get(cn, 0) + 1
        label = a.get_actor_label()
        if isinstance(a, unreal.LevelInstance):
            d = xf(a.get_actor_transform())
            d.update({'kind': 'level_instance', 'label': label, 'class': cn,
                      'level': str(a.get_editor_property('world_asset').get_path_name()) if a.get_editor_property('world_asset') else None})
            items.append(d)
            continue
        if isinstance(a, unreal.Light):
            c = a.light_component
            col = c.get_editor_property('light_color')
            d = xf(a.get_actor_transform())
            d.update({'kind': 'light', 'label': label, 'class': cn, 'intensity': c.get_editor_property('intensity'),
                      'color': [col.r, col.g, col.b]})
            for p in ('attenuation_radius', 'intensity_units', 'source_radius', 'inner_cone_angle', 'outer_cone_angle'):
                try:
                    v = c.get_editor_property(p)
                    d[p] = v if isinstance(v, (int, float, bool)) else str(v)
                except Exception:
                    pass
            items.append(d)
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.static_mesh
            if not m:
                continue
            mats = [x.get_path_name() if x else None for x in c.get_editor_property('override_materials')]
            base = {'kind': 'mesh', 'label': label, 'class': cn, 'mesh': m.get_path_name().split('.')[0], 'mats': mats}
            if isinstance(c, unreal.InstancedStaticMeshComponent):
                for i in range(c.get_instance_count()):
                    d = dict(base)
                    d.update(xf(c.get_instance_transform(i, True)))
                    d['instanced'] = True
                    items.append(d)
            else:
                d = dict(base)
                d.update(xf(c.get_world_transform()))
                o, e, _ = unreal.SystemLibrary.get_component_bounds(c)
                d['bmin'] = [round(o.x - e.x), round(o.y - e.y), round(o.z - e.z)]
                d['bmax'] = [round(o.x + e.x), round(o.y + e.y), round(o.z + e.z)]
                for k in range(3):
                    lo[k] = min(lo[k], d['bmin'][k])
                    hi[k] = max(hi[k], d['bmax'][k])
                items.append(d)
    res = {'level': path, 'classes': classes, 'bmin': lo, 'bmax': hi, 'items': items}
    name = path.split('/')[-1]
    json.dump(res, open(OUT + name + '.json', 'w'), indent=0)
    n = {}
    for it in items:
        n[it['kind']] = n.get(it['kind'], 0) + 1
    return {'level': name, 'counts': n, 'bmin': lo, 'bmax': hi, 'classes': classes}


def read_pieces(folders):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    out = []
    for folder in folders:
        for ad in reg.get_assets_by_path(folder, recursive=True):
            if str(ad.asset_class_path.asset_name) != 'StaticMesh':
                continue
            m = ad.get_asset()
            bb = m.get_bounding_box()
            body = m.get_editor_property('body_setup')
            out.append({'mesh': str(ad.package_name), 'min': [round(bb.min.x, 1), round(bb.min.y, 1), round(bb.min.z, 1)],
                        'max': [round(bb.max.x, 1), round(bb.max.y, 1), round(bb.max.z, 1)],
                        'size': [round(bb.max.x - bb.min.x), round(bb.max.y - bb.min.y), round(bb.max.z - bb.min.z)],
                        'tris': m.get_num_triangles(0), 'lods': m.get_num_lods(),
                        'mats': [str(s.material_interface.get_name()) if s.material_interface else None for s in m.static_materials],
                        'simple': SMS.get_simple_collision_count(m),
                        'trace': str(body.get_editor_property('collision_trace_flag')) if body else None,
                        'nanite': bool(m.get_editor_property('nanite_settings').enabled)})
    json.dump(out, open(OUT + CFG.get('pieces_out', 'pieces.json'), 'w'), indent=0)
    return len(out)


summary = {'levels': [read_level(p) for p in CFG.get('levels', [])]}
if CFG.get('mesh_folders'):
    summary['pieces'] = read_pieces(CFG['mesh_folders'])
if CFG.get('levels'):
    LES.load_level(CFG.get('back_to', '/Engine/Maps/Entry'))
json.dumps(summary)
