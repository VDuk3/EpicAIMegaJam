# valkyrie_wings_render.py - back and side renders of the Valkyrie's rest pose with the wing candidates in red
# (thin islands behind the body, out from the spine). Overnight point 1, diagnosis only.
#   blender -b --python valkyrie_wings_render.py -- <out_dir> [thin_m] [min_abs_x]
import bpy, bmesh, sys, os, math
from mathutils import Vector

A = sys.argv[sys.argv.index("--") + 1:]
OUT = A[0]
THIN = float(A[1]) if len(A) > 1 else 0.03
MINX = float(A[2]) if len(A) > 2 else 0.06
GLB = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "Glyphbound", "SourceArt", "Enemies", "Valkyrie", "Valkiria.glb"))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
body = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
me = body.data
mw = body.matrix_world
bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table()
isl = [-1] * len(bm.verts); groups = []
for v in bm.verts:
    if isl[v.index] >= 0: continue
    st = [v]; isl[v.index] = len(groups); mem = []
    while st:
        x = st.pop(); mem.append(x.index)
        for e in x.link_edges:
            o = e.other_vert(x)
            if isl[o.index] < 0: isl[o.index] = len(groups); st.append(o)
    groups.append(mem)
wing = set()
for gi, mem in enumerate(groups):
    pts = [mw @ me.vertices[k].co for k in mem]
    ys = [p.y for p in pts]; xs = [abs(p.x) for p in pts]
    if max(ys) - min(ys) < THIN and min(ys) > -0.01 and max(xs) > MINX:
        wing.add(gi)
print("WING_ISLANDS", len(wing), sum(len(groups[g]) for g in wing))
# colour attribute: red for wing islands, grey otherwise
col = me.color_attributes.new("wingmask", "BYTE_COLOR", "POINT")
for v in me.vertices:
    col.data[v.index].color = (1, 0.1, 0.1, 1) if isl[v.index] in wing else (0.7, 0.7, 0.7, 1)
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.color_type = "VERTEX"
sc.display.shading.light = "STUDIO"
sc.render.resolution_x = 900; sc.render.resolution_y = 900
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = "ORTHO"; cam.data.ortho_scale = 1.15
for name, loc, rot in (("back", (0, 3, 0.45), (math.radians(90), 0, math.radians(180))),
                       ("side", (3, 0, 0.45), (math.radians(90), 0, math.radians(90))),
                       ("front", (0, -3, 0.45), (math.radians(90), 0, 0))):
    cam.location = loc; cam.rotation_euler = rot
    sc.render.filepath = os.path.join(OUT, "valk_%s.png" % name)
    bpy.ops.render.render(write_still=True)
