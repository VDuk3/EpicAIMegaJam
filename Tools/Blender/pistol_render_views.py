# Renders fixed review views of the pistol (both sides, top, front 3/4, approximate first-person) with EEVEE.
# Usage: blender -b <file.blend> --python pistol_render_views.py -- <out_dir> [frame] [res]
import bpy, sys, os, math
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
frame = int(argv[1]) if len(argv) > 1 else 1
res = int(argv[2]) if len(argv) > 2 else 1400
os.makedirs(out_dir, exist_ok=True)

sc = bpy.context.scene
sc.frame_set(frame)
sc.render.engine = "BLENDER_EEVEE"
sc.render.resolution_x = res
sc.render.resolution_y = int(res * 0.75)
sc.render.film_transparent = False
sc.view_settings.view_transform = "AgX" if "AgX" in [i.identifier for i in sc.view_settings.bl_rna.properties["view_transform"].enum_items] else "Standard"

# neutral studio light, independent of whatever lights the file has
world = bpy.data.worlds.new("ReviewWorld")
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
bg.inputs[0].default_value = (0.32, 0.33, 0.35, 1)
bg.inputs[1].default_value = 0.9
sc.world = world
for ob in list(bpy.data.objects):
    if ob.type == "LIGHT":
        ob.hide_render = True
def add_light(name, rot, energy):
    ld = bpy.data.lights.new(name, "SUN")
    ld.energy = energy
    lo = bpy.data.objects.new(name, ld)
    lo.rotation_euler = [math.radians(a) for a in rot]
    sc.collection.objects.link(lo)
add_light("Key", (50, 10, 35), 3.0)
add_light("Fill", (60, -10, 215), 1.2)

# bounds of all visible meshes
pts = []
for ob in sc.objects:
    if ob.type == "MESH" and not ob.hide_render:
        pts += [ob.matrix_world @ Vector(c) for c in ob.bound_box]
mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center = (mn + mx) / 2
size = (mx - mn).length

cam_data = bpy.data.cameras.new("ReviewCam")
cam = bpy.data.objects.new("ReviewCam", cam_data)
sc.collection.objects.link(cam)
sc.camera = cam

def shoot(name, direction, dist_mul=1.25, lens=50, target=None):
    t = target if target is not None else center
    d = Vector(direction).normalized()
    cam.location = t + d * size * dist_mul
    cam.rotation_euler = (t - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam_data.lens = lens
    sc.render.filepath = os.path.join(out_dir, f"{name}_f{frame:02d}.png")
    bpy.ops.render.render(write_still=True)
    print("RENDER", sc.render.filepath)

# The muzzle points to -X (the imbue bar's lit end is at low X). The energy bar is on the -Y side.
shoot("side_minusY", (0, -1, 0.05))
shoot("side_plusY", (0, 1, 0.05))
shoot("top", (0.0, -0.05, 1))
shoot("front34", (-1, -0.7, 0.35))
# approximate first-person: behind the grip (+X), slightly above and to the right
shoot("fps_approx", (1, -0.35, 0.45), dist_mul=0.75, lens=35)
