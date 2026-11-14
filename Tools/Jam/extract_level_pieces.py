# extract_level_pieces.py - writes every mesh and light of the level open in the Editor to a JSON file, so that
# groups of them (one corridor bay, one stair, one room corner) can be placed again in our own maps exactly as the
# kit's author assembled them. The kits' demo maps are the reference for how their pieces go together (owner).
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/extract_level_pieces.py   (set OUT below first)
import json

OUT = r'C:/GameJam/Design/Levels/kits/htb_demo.json'
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
items = []
for a in eas.get_all_level_actors():
    l = a.get_actor_location()
    r = a.get_actor_rotation()
    s = a.get_actor_scale3d()
    base = {'loc': [round(l.x, 1), round(l.y, 1), round(l.z, 1)], 'rot': [round(r.pitch, 2), round(r.yaw, 2), round(r.roll, 2)],
            'scale': [round(s.x, 3), round(s.y, 3), round(s.z, 3)], 'label': a.get_actor_label()}
    if isinstance(a, unreal.StaticMeshActor):
        c = a.static_mesh_component
        if not c.static_mesh:
            continue
        o, e = a.get_actor_bounds(False)
        base.update({'kind': 'mesh', 'mesh': c.static_mesh.get_path_name(),
                     'mats': [m.get_path_name() if m else None for m in c.get_editor_property('override_materials')],
                     'bmin': [round(o.x - e.x), round(o.y - e.y), round(o.z - e.z)], 'bmax': [round(o.x + e.x), round(o.y + e.y), round(o.z + e.z)],
                     'shadow': bool(c.get_editor_property('cast_shadow'))})
        items.append(base)
    elif isinstance(a, unreal.Light):
        c = a.light_component
        col = c.get_editor_property('light_color')
        d = {'kind': 'light', 'class': a.get_class().get_name(), 'intensity': c.get_editor_property('intensity'),
             'color': [col.r, col.g, col.b], 'shadow': bool(c.get_editor_property('cast_shadows'))}
        for p in ('attenuation_radius', 'source_width', 'source_height', 'barn_door_angle', 'barn_door_length', 'source_radius',
                  'inner_cone_angle', 'outer_cone_angle', 'intensity_units', 'use_temperature', 'temperature', 'volumetric_scattering_intensity',
                  'indirect_lighting_intensity', 'specular_scale'):
            try:
                v = c.get_editor_property(p)
                d[p] = v if isinstance(v, (int, float, bool)) else str(v)
            except Exception:
                pass
        base.update(d)
        items.append(base)
import os
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump({'level': unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(), 'items': items}, open(OUT, 'w'), indent=0)
'%d items -> %s' % (len(items), OUT)
