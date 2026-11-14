# Review renders of a GLB (textured, Workbench): front, back, left, top, bottom, 3/4.
# Optional island highlight: islands whose index is listed get a flat red colour (vertex colour).
# Usage: blender -b --python enemies_render.py -- <file.glb> <out_prefix> [highlight=0,1,2] [zoom=x,y,z,dist]
import sys, math, bpy, bmesh
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
glb, out = args[0], args[1]
opts = dict(a.split("=", 1) for a in args[2:])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
for o in list(bpy.context.scene.objects):
    if o.name.startswith("Icosphere"):
        bpy.data.objects.remove(o)
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center = (mn + mx) / 2
radius = (mx - mn).length / 2

scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'TEXTURE'
scene.render.resolution_x = 900
scene.render.resolution_y = 900
world = bpy.data.worlds.new("w"); scene.world = world
world.color = (0.15, 0.15, 0.17)

if "highlight" in opts:
    hl = set(int(x) for x in opts["highlight"].split(","))
    o = meshes[0]
    bm = bmesh.new(); bm.from_mesh(o.data); bm.verts.ensure_lookup_table()
    seen = set(); islands = []
    for v in bm.verts:
        if v.index in seen: continue
        st = [v]; seen.add(v.index); isl = []
        while st:
            a = st.pop(); isl.append(a.index)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in seen: seen.add(b.index); st.append(b)
        islands.append(isl)
    islands.sort(key=len, reverse=True)
    red = set(i for k in hl if k < len(islands) for i in islands[k])
    bm.free()
    attr = o.data.color_attributes.new("hl", 'FLOAT_COLOR', 'POINT')
    for i, d in enumerate(attr.data):
        d.color = (1, 0.05, 0.05, 1) if i in red else (0.6, 0.6, 0.6, 1)
    scene.display.shading.color_type = 'VERTEX'

cam_data = bpy.data.cameras.new("c"); cam = bpy.data.objects.new("c", cam_data)
scene.collection.objects.link(cam); scene.camera = cam
cam_data.lens = 60
if "zoom" in opts:
    zx, zy, zz, zd = (float(x) for x in opts["zoom"].split(","))
    center = Vector((zx, zy, zz)); dist = zd
else:
    dist = radius * 3.2
# Blender glTF import: +Y up in glTF -> +Z up; glTF front (+Z) -> Blender -Y.
views = {
    "front": Vector((0, -1, 0.15)), "back": Vector((0, 1, 0.15)), "left": Vector((-1, 0, 0.15)),
    "right": Vector((1, 0, 0.15)), "top": Vector((0, -0.01, 1)), "bottom": Vector((0, -0.01, -1)),
    "q34": Vector((0.8, -0.8, 0.5)),
}
only = opts.get("views")
for name, d in views.items():
    if only and name not in only.split(","): continue
    d = d.normalized()
    cam.location = center + d * dist
    cam.rotation_euler = (center - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = f"{out}_{name}.png"
    bpy.ops.render.render(write_still=True)
print("bounds", tuple(mn), tuple(mx))
