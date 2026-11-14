# Measures the orc body's proportions and how much of the body each armor set covers.
#   blender -b --python orc_measure.py -- <stage_dir>
import sys, os, json, bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

STAGE = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.open_mainfile(filepath=os.path.join(STAGE, "OrcArmor", "armor_fit_debug.blend"))
arm = bpy.data.objects["Armature"]
body = [o for o in bpy.data.objects if o.name.startswith("SK_Orc") and not o.name.startswith("SK_OrcArmor")][0]
armor = [o for o in bpy.data.objects if o.name.startswith("SK_OrcArmor_")]
wm = body.matrix_world
vs = [wm @ v.co for v in body.data.vertices]
out = {}

# ---- proportions (cm)
zs = [v.z for v in vs]
H = max(zs) - min(zs)
def bone(n):
    return arm.matrix_world @ arm.data.bones[n].head_local
def width_at(z0, z1, lim=None):
    p = [v for v in vs if z0 <= v.z <= z1 and (lim is None or abs(v.x) < lim)]
    return (max(v.x for v in p) - min(v.x for v in p)) if p else 0
head_z = bone("head").z
neck_z = bone("neck_01").z
pelvis_z = bone("pelvis").z
names = {g.index: g.name for g in body.vertex_groups}
dom = {}
for v in body.data.vertices:
    if v.groups:
        dom[v.index] = names[max(v.groups, key=lambda x: x.weight).group]
headv = [vs[i] for i, b in dom.items() if b == "head"]
out["height_cm"] = round(H, 1)
out["head_height_cm"] = round(max(v.z for v in headv) - min(v.z for v in headv), 1)
out["head_width_cm"] = round(max(v.x for v in headv) - min(v.x for v in headv), 1)
out["heads_tall"] = round(H / (max(v.z for v in headv) - min(v.z for v in headv)), 2)
out["shoulder_width_cm"] = round(bone("upperarm_l").x - bone("upperarm_r").x, 1)
out["shoulder_joint_z"] = round(bone("upperarm_l").z, 1)
out["chest_width_cm"] = round(width_at(bone("spine_04").z - 5, bone("spine_04").z + 5, lim=abs(bone("upperarm_l").x)), 1)
out["waist_width_cm"] = round(width_at(bone("spine_01").z - 3, bone("spine_01").z + 3, lim=abs(bone("upperarm_l").x)), 1)
out["hip_joint_z_cm"] = round(bone("thigh_l").z, 1)
out["leg_fraction"] = round(bone("thigh_l").z / H, 3)
out["knee_z"] = round(bone("calf_l").z, 1)
out["arm_len_cm"] = round((bone("upperarm_l") - bone("lowerarm_l")).length + (bone("lowerarm_l") - bone("hand_l")).length, 1)
out["hand_z"] = round(bone("hand_l").z, 1)
out["tris_body"] = sum(len(p.vertices) - 2 for p in body.data.polygons)
out["neck_z"] = round(neck_z, 1); out["head_z"] = round(head_z, 1); out["pelvis_z"] = round(pelvis_z, 1)

# ---- coverage: share of the body's surface with armor within 12 cm along the outward normal
REG = {
    "head": ["head"], "neck": ["neck"], "chest": ["spine_03", "spine_04", "spine_05", "clavicle"],
    "belly": ["spine_01", "spine_02"], "pelvis": ["pelvis"], "upper_arm": ["upperarm"], "forearm": ["lowerarm"],
    "hand": ["hand", "index", "middle", "ring", "pinky", "thumb"], "thigh": ["thigh"], "shin": ["calf"],
    "foot": ["foot", "ball"],
}
def region(b):
    for r, pats in REG.items():
        if any(b.startswith(p) for p in pats):
            return r
    return "other"
dg = bpy.context.evaluated_depsgraph_get()
polys = []
for p in body.data.polygons:
    c = wm @ p.center
    n = (wm.to_3x3() @ p.normal).normalized()
    cnt = {}
    for vi in p.vertices:
        r = region(dom.get(vi, ""))
        cnt[r] = cnt.get(r, 0) + 1
    polys.append((c, n, p.area, max(cnt, key=cnt.get), c.y > 0))
cov = {}
tri = {}
for st in "ABCD":
    trees = []
    t = 0
    for o in armor:
        if o.name.startswith("SK_OrcArmor_%s_" % st):
            m = o.matrix_world
            verts = [m @ v.co for v in o.data.vertices]
            faces = [tuple(p.vertices) for p in o.data.polygons]
            trees.append(BVHTree.FromPolygons(verts, faces))
            t += sum(len(f) - 2 for f in faces)
    tri[st] = t
    acc = {}
    for c, n, a, r, back in polys:
        hit = any(tr.ray_cast(c + n * 0.05, n, 12.0)[0] is not None for tr in trees)
        for key in (r, "ALL", "ALL_back" if back else "ALL_front"):
            x = acc.setdefault(key, [0.0, 0.0])
            x[1] += a
            if hit:
                x[0] += a
    cov[st] = {k: round(100 * v[0] / v[1]) for k, v in sorted(acc.items())}
out["coverage_pct"] = cov
out["tris_armor_set"] = tri
print("MEASURE " + json.dumps(out))
json.dump(out, open(os.path.join(STAGE, "measure.json"), "w"), indent=1)
