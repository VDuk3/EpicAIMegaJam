# reaper_blade_sockets.py - finds the final boss's scythe blade in the delivered GLB (the scythe is part of the body mesh and
# rides hand_r) and writes the blade's points in hand_r space, in the same 3.5 m / cm frame as enemies_export_fbx.py do_reaper:
# sockets BladeBase, BladeMid, BladeTip (the Nature slash trail runs between them) and Grip (the green thorns round the shaft).
#   blender -b --python reaper_blade_sockets.py -- <FinalBoss.glb> <out.json>
import bpy, sys, json, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
GLB, OUT = args[0], args[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
body = next(o for o in bpy.context.scene.objects if o.type == "MESH" and len(o.data.polygons) > 1000)
bpy.context.view_layer.update()
mw = body.matrix_world
vs = [mw @ v.co for v in body.data.vertices]
zmin = min(v.z for v in vs)
zmax = max(v.z for v in vs)
S = 3.5 / (zmax - zmin)          # the export's scale (do_reaper: 3.5 m tall)

gi = body.vertex_groups["hand_r"].index if "hand_r" in body.vertex_groups else None
hand_verts = []
for v in body.data.vertices:
    w = sum(g.weight for g in v.groups if g.group == gi)
    if w > 0.6:
        hand_verts.append((mw @ v.co))
print("RB hand_r verts", len(hand_verts))
hb = arm.data.bones["hand_r"]
hand_head = arm.matrix_world @ hb.head_local
# the scythe: hand_r vertices far from the hand bone (fingers are within ~15 cm of it at real scale)
far = [p for p in hand_verts if (p - hand_head).length * S > 0.35]
print("RB scythe verts", len(far))
top = max(p.z for p in far)
bot = min(p.z for p in far)
print("RB scythe z range (m)", round((bot - zmin) * S, 2), round((top - zmin) * S, 2))
# the shaft is a near-vertical line (rest pose): its axis is the median xy of the scythe's points; the crescent blade is the
# part far from that axis (the green thorns stay within ~30 cm of the shaft, the blade reaches much further out)
xs = sorted(p.x for p in far)
ys = sorted(p.y for p in far)
shaft_axis_xy = Vector((xs[len(xs) // 2], ys[len(ys) // 2]))
def off_axis(p):
    return (Vector((p.x, p.y)) - shaft_axis_xy).length * S
blade = [p for p in far if off_axis(p) > 0.30]
print('RB blade verts', len(blade), 'z', round((min(p.z for p in blade) - zmin) * S, 2), round((max(p.z for p in blade) - zmin) * S, 2))
base = max(blade, key=lambda p: p.z - 0.5 * off_axis(p) / S)
tip = min(blade, key=lambda p: p.z)
mid = max(blade, key=lambda p: off_axis(p) - abs((p - base).length - (p - tip).length) * S)
grip_band = [p for p in far if bot + (top - bot) * 0.35 <= p.z <= bot + (top - bot) * 0.6]
grip = sum(grip_band, Vector()) / max(len(grip_band), 1)

# hand_r space in the export: the export applies scale S and drops by zmin; bone frames keep their orientation (transform_apply)
hm = arm.matrix_world @ hb.matrix_local
def local_cm(p):
    q = hm.inverted() @ p          # metres in the GLB, hand_r frame
    return [round(c * S * 100.0, 2) for c in q]
res = {"scale": S, "BladeBase": local_cm(base), "BladeMid": local_cm(mid), "BladeTip": local_cm(tip), "Grip": local_cm(grip),
       "world_cm": {k: [round(c * S * 100, 1) for c in (p - Vector((0, 0, zmin)))] for k, p in (("base", base), ("mid", mid), ("tip", tip), ("grip", grip), ("hand", hand_head))}}
json.dump(res, open(OUT, "w"), indent=1)
print("RB", json.dumps(res))
