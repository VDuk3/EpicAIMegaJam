# reaper_blade_check.py - debug render of reaper_blade_sockets.py's result: the scythe's hand_r vertices in yellow, the ones
# taken as blade in red, the sockets as spheres (base green, mid blue, tip red, grip white). Front and right ortho views.
#   blender -b --python reaper_blade_check.py -- <FinalBoss.glb> <out_dir>
import bpy, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
GLB, DIR = args[0], args[1]
sys.argv = [sys.argv[0], "--", GLB, os.path.join(DIR, "blade_sockets.json")]
exec(open(os.path.join(os.path.dirname(__file__), "reaper_blade_sockets.py")).read())
sc = bpy.context.scene


def ball(p, rgba, r=0.012):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=p)
    o = bpy.context.active_object
    m = bpy.data.materials.new("m")
    m.diffuse_color = rgba
    o.data.materials.append(m)


for p in far[::6]:
    ball(p, (1, 1, 0, 1), 0.004)
for p in blade:
    ball(p, (1, 0, 0, 1), 0.007)
ball(base, (0, 1, 0, 1)); ball(mid, (0, 0.3, 1, 1)); ball(tip, (1, 0, 0, 1)); ball(grip, (1, 1, 1, 1))
sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.color_type = "MATERIAL"
sc.render.resolution_x, sc.render.resolution_y = 700, 900
cd = bpy.data.cameras.new("c"); cd.type = "ORTHO"; cd.ortho_scale = (zmax - zmin) * 1.1
cam = bpy.data.objects.new("c", cd); sc.collection.objects.link(cam); sc.camera = cam
c = Vector((0, 0, (zmin + zmax) / 2))
for name, d in (("front", Vector((0, -1, 0))), ("right", Vector((1, 0, 0))), ("back", Vector((0, 1, 0)))):
    cam.location = c + d * 10
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = os.path.join(DIR, "blade_%s.png" % name)
    bpy.ops.render.render(write_still=True)
