# valkyrie_wings_check.py - renders the wing classification and the fix of valkyrie_wings_lib.py, in the rest pose and
# with the arms brought down and forward (as the game's clips move them), so a wrong wing piece shows at once.
#   blender -b --python valkyrie_wings_check.py -- <out_dir>
import bpy, sys, os, math
sys.path.insert(0, os.path.dirname(__file__))
import valkyrie_wings_lib as W

OUT = sys.argv[sys.argv.index("--") + 1]
GLB = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "Glyphbound", "SourceArt", "Enemies", "Valkyrie", "Valkiria.glb"))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
arm = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"][0]
body = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
me = body.data
wing, arm_y = W.wing_vertices(body, arm)
print("WING_VERTS", len(wing), "arm_y", round(arm_y, 3))
col = me.color_attributes.new("wingmask", "BYTE_COLOR", "POINT")
for v in me.vertices:
    col.data[v.index].color = (1, 0.1, 0.1, 1) if v.index in wing else (0.7, 0.7, 0.7, 1)
W.rigidify(body, arm, wing)

sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.color_type = "VERTEX"
sc.display.shading.light = "STUDIO"
sc.render.resolution_x = 700; sc.render.resolution_y = 700
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = "ORTHO"; cam.data.ortho_scale = 1.2
VIEWS = (("back", (0, 3, 0.45), (math.radians(90), 0, math.radians(180))),
         ("side", (3, 0, 0.45), (math.radians(90), 0, math.radians(90))),
         ("front34", (2.2, -2.2, 0.45), (math.radians(90), 0, math.radians(45))))


def shoot(tag):
    for name, loc, rot in VIEWS:
        cam.location = loc; cam.rotation_euler = rot
        sc.render.filepath = os.path.join(OUT, "wings_%s_%s.png" % (tag, name))
        bpy.ops.render.render(write_still=True)


shoot("rest")
# arms down and a little forward, as in the idle and attack clips
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="POSE")
for side in ("l", "r"):
    pb = arm.pose.bones.get("upperarm_" + side)
    if pb:
        pb.rotation_mode = "XYZ"
        pb.rotation_euler = (math.radians(30), 0, math.radians(65 if side == "l" else -65))
    pb = arm.pose.bones.get("lowerarm_" + side)
    if pb:
        pb.rotation_mode = "XYZ"
        pb.rotation_euler = (0, 0, math.radians(-40 if side == "l" else 40))
bpy.ops.object.mode_set(mode="OBJECT")
shoot("posed")
