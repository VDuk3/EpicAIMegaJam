# Lists the objects of the pistol .blend: type, parent, transform, size, materials, UV maps and animation.
# Usage: blender -b <Pistol.blend> --python pistol_list_objects.py
import bpy

scene = bpy.context.scene
print("FRAMES", scene.frame_start, scene.frame_end, "unit scale", scene.unit_settings.scale_length)
for ob in bpy.data.objects:
    act = ob.animation_data.action.name if ob.animation_data and ob.animation_data.action else None
    mats = [s.material.name for s in ob.material_slots if s.material] if ob.type == "MESH" else []
    uvs = [l.name for l in ob.data.uv_layers] if ob.type == "MESH" else []
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons) if ob.type == "MESH" else 0
    cons = [c.type for c in ob.constraints]
    print("OBJ", ob.name, ob.type, "parent=", ob.parent.name if ob.parent else None,
          "ptype=", ob.parent_type, "loc=", tuple(round(v, 4) for v in ob.location),
          "rot=", tuple(round(v, 4) for v in ob.rotation_euler), "scale=", tuple(round(v, 4) for v in ob.scale),
          "dims=", tuple(round(v, 4) for v in ob.dimensions), "tris=", tris, "mats=", mats, "uvs=", uvs,
          "action=", act, "cons=", cons, "mods=", [m.type for m in ob.modifiers], "hidden=", ob.hide_render)
for a in bpy.data.actions:
    print("ACTION", a.name, "range", tuple(a.frame_range))
    try:
        curves = a.fcurves
    except AttributeError:
        curves = [fc for layer in a.layers for strip in layer.strips for bag in strip.channelbags for fc in bag.fcurves]
    for fc in curves:
        kps = [(round(k.co[0], 1), round(k.co[1], 4)) for k in fc.keyframe_points]
        print("  FC", fc.data_path, fc.array_index, kps[:12])
for m in bpy.data.materials:
    print("MAT", m.name, "users", m.users)
