# reaper_blade_island.py - the final boss's crescent blade is a separate island of the body mesh skinned to the back, not to
# the staff in hand_r. This finds that island, reports its bone weights, and (with --move) makes a preview where the blade is
# moved onto the staff's head and skinned rigidly to hand_r, rendered front / right / 3-4.
#   blender -b --python reaper_blade_island.py -- <FinalBoss.glb> <out_dir> [--move]
import bpy, bmesh, sys, os, json
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
GLB, DIR = args[0], args[1]
MOVE = "--move" in args
os.makedirs(DIR, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
for o in list(bpy.context.scene.objects):
    if o.name.startswith("Icosphere"):
        bpy.data.objects.remove(o)
arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
body = next(o for o in bpy.context.scene.objects if o.type == "MESH")
mw = body.matrix_world
me = body.data
co = [mw @ v.co for v in me.vertices]
zmin = min(c.z for c in co); zmax = max(c.z for c in co); H = zmax - zmin

bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table()
seen = [False] * len(bm.verts); islands = []
for v in bm.verts:
    if seen[v.index]:
        continue
    st = [v]; seen[v.index] = True; isl = []
    while st:
        a = st.pop(); isl.append(a.index)
        for e in a.link_edges:
            b = e.other_vert(a)
            if not seen[b.index]:
                seen[b.index] = True; st.append(b)
    islands.append(isl)
bm.free()
names = {g.index: g.name for g in body.vertex_groups}


def bones_of(isl):
    acc = {}
    for i in isl:
        for g in me.vertices[i].groups:
            acc[names[g.group]] = acc.get(names[g.group], 0.0) + g.weight
    return sorted(acc.items(), key=lambda kv: -kv[1])[:4]


def box(isl):
    ps = [co[i] for i in isl]
    lo = Vector((min(p.x for p in ps), min(p.y for p in ps), min(p.z for p in ps)))
    hi = Vector((max(p.x for p in ps), max(p.y for p in ps), max(p.z for p in ps)))
    return lo, hi


# the crescent: several thin islands on the model's right side (x < 0), skinned to clavicle_r / upperarm_r (the auto rig took
# it for part of the shoulder), between 18% and 100% of the height
blade = []
for isl in islands:
    lo, hi = box(isl)
    b = bones_of(isl)
    if b and b[0][0] in ('clavicle_r', 'upperarm_r') and hi.x < -0.01 * H and len(isl) < 200 and (hi.z - lo.z) > H * 0.1:
        blade += isl
        print('BI blade island', len(isl), 'z', round((lo.z - zmin) / H, 2), round((hi.z - zmin) / H, 2))
print('BI islands', len(islands), 'blade verts', len(blade))
res = {"blade_verts": len(blade) if blade else 0}
if blade and MOVE:
    blo, bhi = box(blade)
    gi = body.vertex_groups["hand_r"].index
    staff = [co[v.index] for v in me.vertices if any(g.group == gi and g.weight > 0.6 for g in v.groups)]
    stop = max(staff, key=lambda p: p.z)
    # blade top point nearest the staff in x: its attachment end
    btop = max((co[i] for i in blade), key=lambda p: p.z)
    # the blade's top meets the staff's head in x and y; it keeps rising a little above the head (scythe)
    delta = Vector((stop.x - btop.x, stop.y - btop.y, (stop.z + H * 0.04) - btop.z))
    print("BI move blade by (m, model units)", tuple(round(c, 3) for c in delta))
    inv = mw.inverted()
    for i in blade:
        v = me.vertices[i]
        v.co = inv @ (co[i] + delta)
        for g in list(v.groups):
            body.vertex_groups[names[g.group]].remove([i])
        body.vertex_groups["hand_r"].add([i], 1.0, "REPLACE")
    res["delta"] = list(delta)
    res["blade_indices"] = blade
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"; sc.display.shading.color_type = "TEXTURE"
    w = bpy.data.worlds.new("w"); sc.world = w; w.color = (0.42, 0.43, 0.46)
    sc.render.resolution_x, sc.render.resolution_y = 700, 900
    cd = bpy.data.cameras.new("c"); cd.type = "ORTHO"; cd.ortho_scale = H * 1.15
    cam = bpy.data.objects.new("c", cd); sc.collection.objects.link(cam); sc.camera = cam
    c = Vector((0, 0, (zmin + zmax) / 2))
    for name, d in (("front", Vector((0, -1, 0))), ("right", Vector((1, 0, 0))), ("q34", Vector((-0.7, -0.7, 0.15)).normalized())):
        cam.location = c + d * 10
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = os.path.join(DIR, "moved_%s.png" % name)
        bpy.ops.render.render(write_still=True)
    # pose test: the right arm raised, to show the blade follows the staff
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    import math
    pb = arm.pose.bones["upperarm_r"]; pb.rotation_mode = "XYZ"; pb.rotation_euler = (0, 0, math.radians(-60))
    pl = arm.pose.bones["lowerarm_r"]; pl.rotation_mode = "XYZ"; pl.rotation_euler = (0, 0, math.radians(-40))
    bpy.ops.object.mode_set(mode="OBJECT")
    for name, d in (("pose_front", Vector((0, -1, 0))), ("pose_right", Vector((1, 0, 0)))):
        cam.location = c + d * 10
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        sc.render.filepath = os.path.join(DIR, "moved_%s.png" % name)
        bpy.ops.render.render(write_still=True)
json.dump(res, open(os.path.join(DIR, "blade_island.json"), "w"))
