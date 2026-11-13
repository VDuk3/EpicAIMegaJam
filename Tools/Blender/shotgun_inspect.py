# Headless inventory of the shotgun's Tripo FBX as it comes: objects, size, axes, triangles, loose pieces before and
# after welding vertices, open edges, materials, images and UV maps. Read-only: nothing is saved.
# Usage: blender -b --factory-startup --python shotgun_inspect.py -- <model.fbx> [weld_mm]
import bpy, bmesh, sys, os, glob
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
fbx = argv[0]
weld = (float(argv[1]) if len(argv) > 1 else 0.01) / 1000.0
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
print("BLENDER", bpy.app.version_string)

for im in bpy.data.images:
    if im.size[0] == 0:
        hits = glob.glob(os.path.join(os.path.dirname(fbx), "*.fbm", "*"))
        print("IMAGE_UNRESOLVED", im.name, im.filepath, "candidates", [os.path.basename(h) for h in hits])
        if len(hits) == 1:
            im.filepath = hits[0]
            im.reload()
    print("IMAGE", im.name, tuple(im.size), im.colorspace_settings.name, im.filepath)

for ob in bpy.data.objects:
    print("OBJECT", ob.name, ob.type, "parent", ob.parent.name if ob.parent else "-",
          "loc", tuple(round(v, 4) for v in ob.location), "rot", tuple(round(v, 4) for v in ob.rotation_euler),
          "scale", tuple(round(v, 4) for v in ob.scale), "dims", tuple(round(v, 4) for v in ob.dimensions))

for m in bpy.data.materials:
    print("MATERIAL", m.name)
    if m.use_nodes:
        for n in m.node_tree.nodes:
            print("   node", n.bl_idname, n.name, getattr(getattr(n, "image", None), "name", ""))
        for l in m.node_tree.links:
            print("   link", l.from_node.name, l.from_socket.identifier, "->", l.to_node.name, l.to_socket.identifier)
        b = m.node_tree.nodes.get("Principled BSDF")
        if b:
            for k in ("Metallic", "Roughness"):
                print("   ", k, b.inputs[k].default_value, "linked" if b.inputs[k].is_linked else "")


def islands(bm):
    seen, out = set(), []
    for v in bm.verts:
        if v.index in seen:
            continue
        stack, verts = [v], []
        seen.add(v.index)
        while stack:
            a = stack.pop()
            verts.append(a)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in seen:
                    seen.add(b.index)
                    stack.append(b)
        out.append(verts)
    return out


for ob in [o for o in bpy.data.objects if o.type == "MESH"]:
    me = ob.data
    print("MESH", ob.name, "verts", len(me.vertices), "polys", len(me.polygons),
          "tris", sum(len(p.vertices) - 2 for p in me.polygons),
          "uv", [u.name for u in me.uv_layers], "colors", [c.name for c in me.color_attributes],
          "custom_normals", me.has_custom_normals, "slots", [s.material.name if s.material else "-" for s in ob.material_slots])
    mw = ob.matrix_world
    pts = [mw @ v.co for v in me.vertices]
    lo = Vector([min(p[i] for p in pts) for i in range(3)])
    hi = Vector([max(p[i] for p in pts) for i in range(3)])
    print("  WORLD_BOUNDS min", tuple(round(v, 4) for v in lo), "max", tuple(round(v, 4) for v in hi),
          "size", tuple(round(v, 4) for v in hi - lo))
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.transform(mw)
    bm.verts.index_update()
    print("  LOOSE_PIECES_AS_IS", len(islands(bm)), "open_edges", sum(1 for e in bm.edges if e.is_boundary))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=weld)
    bm.verts.index_update()
    isl = islands(bm)
    print("  AFTER_WELD_%gmm" % (weld * 1000), "verts", len(bm.verts), "pieces", len(isl),
          "open_edges", sum(1 for e in bm.edges if e.is_boundary),
          "non_manifold", sum(1 for e in bm.edges if not e.is_manifold and not e.is_boundary))
    isl.sort(key=lambda vs: -len(vs))
    for i, vs in enumerate(isl[:40]):
        a = Vector([min(v.co[k] for v in vs) for k in range(3)])
        b = Vector([max(v.co[k] for v in vs) for k in range(3)])
        faces = {f.index for v in vs for f in v.link_faces}
        print("   PIECE %d verts %d faces %d min %s max %s" % (i, len(vs), len(faces),
              tuple(round(v, 3) for v in a), tuple(round(v, 3) for v in b)))
    bm.free()
