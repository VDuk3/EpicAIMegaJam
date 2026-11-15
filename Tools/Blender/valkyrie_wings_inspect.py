# valkyrie_wings_inspect.py - what the Valkyrie's wings are made of and what they are skinned to (overnight point 1).
#   blender -b --python valkyrie_wings_inspect.py -- <out_dir>
# Writes <out_dir>/wings_report.json (mesh islands with bounds and their top bones) and back/side renders
# (workbench) of the rest pose, wing islands tinted.
import bpy, bmesh, sys, os, json
from collections import defaultdict, Counter
from mathutils import Vector

OUT = sys.argv[sys.argv.index("--") + 1]
os.makedirs(OUT, exist_ok=True)
GLB = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "Glyphbound", "SourceArt", "Enemies", "Valkyrie", "Valkiria.glb"))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
arm = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"][0]
body = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
mw = body.matrix_world
me = body.data
vg = {g.index: g.name for g in body.vertex_groups}

bm = bmesh.new()
bm.from_mesh(me)
bm.verts.ensure_lookup_table()
island = [-1] * len(bm.verts)
islands = []
for v in bm.verts:
    if island[v.index] >= 0:
        continue
    stack = [v]
    island[v.index] = len(islands)
    members = []
    while stack:
        x = stack.pop()
        members.append(x.index)
        for e in x.link_edges:
            o = e.other_vert(x)
            if island[o.index] < 0:
                island[o.index] = len(islands)
                stack.append(o)
    islands.append(members)

rep = {"verts": len(me.vertices), "bones": [b.name for b in arm.data.bones], "islands": []}
for i, members in enumerate(islands):
    pts = [mw @ me.vertices[k].co for k in members]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    bonew = Counter()
    for k in members:
        for g in me.vertices[k].groups:
            bonew[vg[g.group]] += g.weight
    rep["islands"].append({"i": i, "n": len(members), "lo": [round(c, 3) for c in lo], "hi": [round(c, 3) for c in hi],
                           "top_bones": [(b, round(w, 1)) for b, w in bonew.most_common(8)]})
rep["islands"].sort(key=lambda r: -r["n"])

# per-vertex dominant bone for vertices behind the torso and wider than the shoulders (wings if they are one island)
allp = [mw @ v.co for v in me.vertices]
zmin = min(p.z for p in allp); zmax = max(p.z for p in allp)
rep["height"] = round(zmax - zmin, 3)
rep["bbox"] = [[round(min(p.x for p in allp), 3), round(min(p.y for p in allp), 3)], [round(max(p.x for p in allp), 3), round(max(p.y for p in allp), 3)]]
with open(os.path.join(OUT, "wings_report.json"), "w") as f:
    json.dump(rep, f, indent=1)
print("REPORT", os.path.join(OUT, "wings_report.json"), len(islands), "islands")
