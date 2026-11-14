# Review renders of the fitted orc armor (reads the debug blend written by enemies_export_fbx.py ... orc armor --blend).
#   blender -b --python armor_review_render.py -- <stage_dir> [sets=ABCD] [scale=1.0] [only=front,side,...]
# Per set (the other sets are hidden): front, side, back, boots (close-ups), posed_walk, posed_aim, plus a sheet.
# The body is shown with its own texture, the armor with its own textures; Workbench, plain studio light, grey background.
import sys, os, math, bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
STAGE = args[0]
opts = dict(a.split("=", 1) for a in args[1:])
SETS = opts.get("sets", "ABCD")
only = opts.get("only")
OUT = os.path.join(STAGE, "review")
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=os.path.join(STAGE, "OrcArmor", "armor_fit_debug.blend"))
sc = bpy.context.scene
arm = bpy.data.objects["Armature"]
bodies = [o for o in bpy.data.objects if o.name.startswith("SK_Orc") and not o.name.startswith("SK_OrcArmor")]
# under gauntlets the game wears the body without hands; hands=1 shows the whole body instead
body = next((o for o in bodies if o.name.endswith("_NoHands") and not opts.get("hands")), bodies[0])
for o in bpy.data.objects:
    if o.type == "MESH":
        o.hide_set(False)
for o in bodies:
    o.hide_viewport = o is not body
    o.hide_render = o is not body or bool(opts.get("nobody"))
armor = [o for o in bpy.data.objects if o.name.startswith("SK_OrcArmor_")]

sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.light = "STUDIO"
sc.display.shading.color_type = "TEXTURE"
sc.display.shading.show_shadows = False
sc.display.shading.show_specular_highlight = False
sc.render.resolution_x = int(700 * float(opts.get("scale", 1)))
sc.render.resolution_y = int(900 * float(opts.get("scale", 1)))
sc.render.image_settings.file_format = "PNG"
w = bpy.data.worlds.new("w")
sc.world = w
w.color = (0.42, 0.43, 0.46)
cam_d = bpy.data.cameras.new("c")
cam_d.type = "ORTHO"
cam_d.clip_end = 5000
cam = bpy.data.objects.new("c", cam_d)
sc.collection.objects.link(cam)
sc.camera = cam
bpy.context.view_layer.update()


def show_set(st):
    for o in armor:
        o.hide_render = not o.name.startswith("SK_OrcArmor_%s_" % st)
        o.hide_viewport = o.hide_render


def shot(name, centre, dirv, scale, up=(0, 0, 1)):
    d = Vector(dirv).normalized()
    cam.location = Vector(centre) + d * 1500
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam_d.ortho_scale = scale
    sc.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)


def reset_pose():
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
        pb.scale = (1, 1, 1)
    bpy.context.view_layer.update()


def rot(bone, axis, deg):
    """Rotates a pose bone about a world axis through its head (armature object is at the origin, no rotation)."""
    pb = arm.pose.bones[bone]
    head = pb.head.copy()
    m = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-head)
    pb.matrix = m @ pb.matrix
    bpy.context.view_layer.update()


def pose_walk():
    reset_pose()
    rot("thigh_l", "X", -28)
    rot("calf_l", "X", 25)
    rot("thigh_r", "X", 18)
    rot("calf_r", "X", 8)
    rot("upperarm_l", "X", 20)
    rot("lowerarm_l", "X", -25)
    rot("upperarm_r", "X", -20)
    rot("lowerarm_r", "X", -15)


def pose_aim():
    reset_pose()
    rot("upperarm_r", "X", -70)
    rot("lowerarm_r", "X", -35)
    rot("upperarm_l", "X", -55)
    rot("lowerarm_l", "X", -60)
    rot("thigh_l", "X", -12)
    rot("calf_l", "X", 12)
    rot("thigh_r", "X", 8)
    rot("calf_r", "X", 20)


def pose_grip():
    reset_pose()
    rot("upperarm_r", "X", -40)
    rot("lowerarm_r", "X", -70)
    for side in ("_r", "_l"):
        for f in ("index", "middle", "ring", "pinky"):
            for j in (1, 2, 3):
                pb = arm.pose.bones["%s_0%d%s" % (f, j, side)]
                pb.rotation_mode = "XYZ"
                pb.rotation_euler = (0, 0, math.radians(-50 if side == "_r" else 50))
    bpy.context.view_layer.update()


def want(n):
    return not only or n in only.split(",")


FULL = (0, 0, 105)
for st in SETS:
    show_set(st)
    reset_pose()
    if want("front"):
        shot("%s_front" % st, FULL, (0, -1, 0), 235)
    if want("side"):
        shot("%s_side" % st, FULL, (1, 0, 0), 235)
    if want("back"):
        shot("%s_back" % st, FULL, (0, 1, 0), 235)
    if want("boots"):
        shot("%s_boots_front" % st, (0, 0, 36), (0, -1, 0), 120)
        shot("%s_boots_side_r" % st, (-28, 0, 22), (-1, 0, 0), 70)
        shot("%s_boots_q34" % st, (0, 0, 28), (0.7, -0.7, 0.35), 90)
    if want("close"):
        shot("%s_close_hips_front" % st, (0, 0, 92), (0, -1, 0), 100)
        shot("%s_close_hips_side" % st, (0, 0, 92), (1, 0, 0), 100)
        shot("%s_close_torso_front" % st, (0, 0, 150), (0, -1, 0), 110)
        shot("%s_close_torso_side" % st, (0, 0, 150), (1, 0, 0), 110)
        shot("%s_close_head_front" % st, (0, 0, 185), (0, -1, 0), 55)
        shot("%s_close_head_side" % st, (0, 0, 185), (1, 0, 0), 55)
        shot("%s_close_arm_r" % st, (-55, 0, 135), (-0.3, -1, 0.5), 100)
        shot("%s_close_leg_r" % st, (-22, 0, 50), (-1, -0.3, 0.1), 110)
    if want("hands"):
        shot("%s_hand_r_out" % st, (-70, -5, 115), (-1, -0.2, 0.1), 75)
        shot("%s_hand_r_front" % st, (-70, -5, 115), (0, -1, 0.1), 75)
        shot("%s_hand_r_in" % st, (-70, -5, 115), (1, -0.3, 0.1), 75)
        shot("%s_hand_l_front" % st, (70, -5, 115), (0, -1, 0.1), 75)
        pose_grip()
        shot("%s_hand_grip" % st, (-55, -40, 130), (-0.6, -0.8, 0.2), 110)
        reset_pose()
    if want("posed_walk"):
        pose_walk()
        shot("%s_posed_walk_side" % st, FULL, (1, 0, 0), 235)
        shot("%s_posed_walk_front" % st, FULL, (0, -1, 0), 235)
    if want("posed_aim"):
        pose_aim()
        shot("%s_posed_aim_q34" % st, FULL, (0.8, -0.8, 0.2), 235)
        shot("%s_posed_aim_side" % st, FULL, (-1, 0, 0), 235)
    reset_pose()
print("REVIEW DONE", OUT)
