# Inspects the sprite-bake sources: armature, bones, mesh bounds, materials, clip frame ranges.
#   blender -b --python Tools/Blender/sprite_inspect.py -- <fbx> [<fbx> ...]
import bpy, sys, os
from mathutils import Vector

ARGS = sys.argv[sys.argv.index("--") + 1:]


def bounds(o):
    cs = [o.matrix_world @ Vector(c) for c in o.bound_box]
    mn = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs)))
    mx = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
    return mn, mx


for path in ARGS:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    print("=====", os.path.basename(path), "unit", bpy.context.scene.unit_settings.scale_length,
          "fps", bpy.context.scene.render.fps)
    for o in bpy.context.scene.objects:
        mn, mx = bounds(o)
        print(" OBJ", o.type, o.name, "parent", o.parent.name if o.parent else None, "scale", tuple(round(s, 4) for s in o.scale),
              "rot", tuple(round(r, 3) for r in o.rotation_euler), "min", tuple(round(v, 3) for v in mn),
              "max", tuple(round(v, 3) for v in mx))
        if o.type == "MESH":
            print("   verts", len(o.data.vertices), "mats", [m.name if m else None for m in o.data.materials],
                  "vgroups", len(o.vertex_groups))
        if o.type == "ARMATURE":
            bones = o.data.bones
            print("   bones", len(bones))
            print("   ", [b.name for b in bones][:200])
            if o.animation_data and o.animation_data.action:
                a = o.animation_data.action
                print("   ACTION", a.name, "range", tuple(a.frame_range))
    for a in bpy.data.actions:
        print(" action", a.name, tuple(a.frame_range))
