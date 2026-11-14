# Inspect the GLB deliveries from ASSETS (2026-11-13): objects, size, armature bones, loose parts.
# Usage: blender -b --python enemies_inspect.py -- <file.glb> [more.glb ...]
import sys, bpy, bmesh
from mathutils import Vector

files = sys.argv[sys.argv.index("--") + 1:]
for f in files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=f)
    print("=" * 30, f)
    for o in bpy.context.scene.objects:
        line = f"OBJ {o.name} type={o.type} parent={o.parent.name if o.parent else None}"
        if o.type == 'MESH':
            ws = [o.matrix_world @ Vector(c) for c in o.bound_box]
            mn = Vector((min(v.x for v in ws), min(v.y for v in ws), min(v.z for v in ws)))
            mx = Vector((max(v.x for v in ws), max(v.y for v in ws), max(v.z for v in ws)))
            bm = bmesh.new(); bm.from_mesh(o.data)
            bm.verts.ensure_lookup_table()
            # loose parts (islands)
            seen = set(); islands = []
            for v in bm.verts:
                if v.index in seen: continue
                stack = [v]; seen.add(v.index); isl = []
                while stack:
                    a = stack.pop(); isl.append(a)
                    for e in a.link_edges:
                        b = e.other_vert(a)
                        if b.index not in seen:
                            seen.add(b.index); stack.append(b)
                islands.append(isl)
            islands.sort(key=len, reverse=True)
            line += f" verts={len(bm.verts)} tris={sum(len(p.verts)-2 for p in bm.faces)} size(m)={tuple(round(x,3) for x in (mx-mn))} min={tuple(round(x,3) for x in mn)} max={tuple(round(x,3) for x in mx)} islands={len(islands)}"
            print(line)
            for i, isl in enumerate(islands[:40]):
                cs = [o.matrix_world @ a.co for a in isl]
                imn = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs)))
                imx = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
                cen = (imn + imx) / 2
                print(f"   isl{i} n={len(isl)} center={tuple(round(x,3) for x in cen)} size={tuple(round(x,3) for x in (imx-imn))}")
            print("   materials:", [m.name for m in o.data.materials])
            print("   vgroups:", len(o.vertex_groups))
            bm.free()
        elif o.type == 'ARMATURE':
            print(line)
            bones = o.data.bones
            print("   bones", len(bones), ":", ", ".join(b.name for b in bones))
            for b in bones:
                if b.parent is None:
                    print("   root:", b.name)
        else:
            print(line)
    print("anims:", [a.name for a in bpy.data.actions])
