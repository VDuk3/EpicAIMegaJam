# Review renders of the delivered final boss GLB (the Entity), before any Unreal import.
#   blender -b --python finalboss_review_render.py -- <glb> <out_dir>
# Full body from 4 sides + 3/4, close-ups (head, chest, hands, staff, back, feet) and a pose test that bends
# the rig's arms, spine and legs to show the skin weights. Workbench with textures, plain grey background.
import sys, os, math, bpy
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index("--") + 1:]
GLB, OUT = args[0], args[1]
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
sc = bpy.context.scene
all_meshes = [o for o in sc.objects if o.type == "MESH"]
# the GLB carries a stray 80-face "Icosphere" (no material); bounds and renders use only the skinned body
meshes = [o for o in all_meshes if len(o.data.polygons) > 1000]
for o in all_meshes:
    if o not in meshes:
        print("FB stray mesh", o.name, "parent", o.parent.name if o.parent else None,
              "loc", tuple(round(v, 3) for v in o.matrix_world.translation), "dims", tuple(round(v, 3) for v in o.dimensions))
        o.hide_render = True
arm = next((o for o in sc.objects if o.type == "ARMATURE"), None)
bpy.context.view_layer.update()

# bounds of all meshes in world space
pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
size = hi - lo
H = size.z
c = (lo + hi) / 2
print("FB bounds lo", tuple(round(v, 3) for v in lo), "hi", tuple(round(v, 3) for v in hi), "size", tuple(round(v, 3) for v in size))
print("FB meshes", [(o.name, len(o.data.polygons), [m.name for m in o.data.materials]) for o in meshes])
if arm:
    print("FB bones", len(arm.data.bones), [b.name for b in arm.data.bones])

sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.light = "STUDIO"
sc.display.shading.color_type = "TEXTURE"
sc.display.shading.show_shadows = False
sc.display.shading.show_specular_highlight = True
sc.render.resolution_x = 800
sc.render.resolution_y = 1000
sc.render.image_settings.file_format = "PNG"
w = bpy.data.worlds.new("w")
sc.world = w
w.color = (0.42, 0.43, 0.46)
cam_d = bpy.data.cameras.new("c")
cam_d.type = "ORTHO"
cam_d.clip_end = 1000
cam = bpy.data.objects.new("c", cam_d)
sc.collection.objects.link(cam)
sc.camera = cam

# glTF import: -Y is the model's front after conversion (glTF +Z forward -> Blender -Y)
FRONT, BACK, LEFT, RIGHT = (0, -1, 0), (0, 1, 0), (-1, 0, 0), (1, 0, 0)


def shot(name, centre, dirv, scale, aspect=(800, 1000)):
    sc.render.resolution_x, sc.render.resolution_y = aspect
    d = Vector(dirv).normalized()
    cam.location = Vector(centre) + d * (H * 5)
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam_d.ortho_scale = scale
    sc.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)


def at(fz, fx=0.0, fy=0.0):
    return Vector((c.x + fx * size.x, c.y + fy * size.y, lo.z + fz * H))


full = max(H, size.x) * 1.08
shot("01_front", at(0.5), FRONT, full)
shot("02_back", at(0.5), BACK, full)
shot("03_left", at(0.5), LEFT, full)
shot("04_right", at(0.5), RIGHT, full)
shot("05_three_quarter", at(0.5), (0.7, -0.7, 0.15), full)
shot("06_three_quarter_back", at(0.5), (-0.7, 0.7, 0.15), full)
shot("07_head_front", at(0.88), FRONT, H * 0.25, (800, 800))
shot("08_head_3q", at(0.88), (0.6, -0.8, 0.1), H * 0.25, (800, 800))
shot("09_chest", at(0.68), FRONT, H * 0.32, (800, 800))
shot("10_back_close", at(0.68), BACK, H * 0.35, (800, 800))
shot("11_waist_hands", at(0.48), FRONT, H * 0.4, (800, 800))
shot("12_feet", at(0.08), (0.5, -0.85, 0.2), H * 0.3, (800, 800))
shot("13_top", at(0.6), (0.0, -0.3, 1.0), full)

# pose test: bend a few bones by name pattern so the skin weights show
if arm:
    def find(*keys):
        for b in arm.pose.bones:
            n = b.name.lower()
            if all(k in n for k in keys):
                return b
        return None

    def bend(pb, axis, deg):
        if not pb:
            return
        head = arm.matrix_world @ pb.head
        m = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-head)
        pb.matrix = arm.matrix_world.inverted() @ m @ arm.matrix_world @ pb.matrix
        bpy.context.view_layer.update()

    # the rig uses the UE mannequin bone names; side is read from the bone's world x, not its name
    P = arm.pose.bones
    for sd in ("l", "r"):
        ua = P.get("upperarm_" + sd)
        sgn = 1 if ua and (arm.matrix_world @ ua.head).x > c.x else -1
        bend(ua, (0, 1, 0), sgn * 45)
        bend(P.get("lowerarm_" + sd), (1, 0, 0), -70)
        bend(P.get("thigh_" + sd), (1, 0, 0), -40 if sd == "l" else 25)
        bend(P.get("calf_" + sd), (1, 0, 0), 50 if sd == "l" else 0)
    bend(P.get("spine_03"), (1, 0, 0), -15)
    bend(P.get("head"), (0, 0, 1), 30)
    shot("14_pose_front", at(0.5), FRONT, full * 1.1)
    shot("15_pose_3q", at(0.5), (0.7, -0.7, 0.15), full * 1.1)
    shot("16_pose_back", at(0.5), BACK, full * 1.1)
print("FB done", OUT)
