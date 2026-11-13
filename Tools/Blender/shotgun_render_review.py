# Review renders of the shotgun in Cycles, adapted from rifle_render_review.py. The owner judges art on these before
# any Unreal import. Works on the Tripo FBX as it comes or on a .blend.
# Survey views are framed from the model's bounds, so they need no knowledge of the gun: top, bottom, left, right,
# front, rear (orthographic, scale printed) and four three-quarter views. First-person views are added by name once
# the gun's frame is known (see FP below).
# With "flat" as the last argument the gun is drawn unlit, its colour texture as it is: what the texture holds, with no
# help from the lights (used to judge the cables' texels).
# Usage: blender -b --factory-startup --python shotgun_render_review.py -- <model.fbx|model.glb|Shotgun.blend> <out_dir>
#        [prefix] [view,view,...] [samples] [flat]
import bpy, sys, os, math, glob
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
path, out_dir = argv[0], argv[1]
prefix = argv[2] if len(argv) > 2 else "v"
views = argv[3].split(",") if len(argv) > 3 and argv[3] else None
samples = int(argv[4]) if len(argv) > 4 else 64
flat = len(argv) > 5 and argv[5] == "flat"
os.makedirs(out_dir, exist_ok=True)
if path.lower().endswith(".blend"):
    bpy.ops.wm.open_mainfile(filepath=path)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if path.lower().endswith(".fbx"):
        bpy.ops.import_scene.fbx(filepath=path)
    else:
        bpy.ops.import_scene.gltf(filepath=path)
    for im in bpy.data.images:
        if im.size[0] == 0:
            hits = glob.glob(os.path.join(os.path.dirname(path), "*.fbm", "*"))
            if len(hits) == 1:
                im.filepath = hits[0]
                im.reload()

    # Tripo exported the gun turned about the vertical axis. Turn it back: the angle that makes it narrowest.
    import numpy as np
    ob = [o for o in bpy.data.objects if o.type == "MESH"][0]
    xy = np.array([(ob.matrix_world @ v.co)[:2] for v in ob.data.vertices])
    best = min((np.ptp(xy[:, 0] * math.cos(a) - xy[:, 1] * math.sin(a)), a)
               for a in np.radians(np.arange(-25, 25, 0.05)))
    ob.rotation_mode = "XYZ"                            # the glTF importer leaves quaternions
    ob.rotation_euler = (0.0, 0.0, best[1])
    bpy.context.view_layer.update()
    xs = [(ob.matrix_world @ v.co).x for v in ob.data.vertices]
    ob.location.x -= (min(xs) + max(xs)) / 2
    bpy.context.view_layer.update()
    print("STRAIGHTEN yaw_deg", round(math.degrees(best[1]), 2), "width_m", round(float(best[0]), 4))

if flat:
    for m in bpy.data.materials:
        nt = m.node_tree
        tex = next((n for n in nt.nodes if n.bl_idname == "ShaderNodeTexImage"), None)
        out = next((n for n in nt.nodes if n.bl_idname == "ShaderNodeOutputMaterial"), None)
        if tex and out:
            tex.interpolation = "Closest"
            em = nt.nodes.new("ShaderNodeEmission")
            nt.links.new(tex.outputs["Color"], em.inputs["Color"])
            nt.links.new(em.outputs[0], out.inputs["Surface"])

sc = bpy.context.scene
sc.render.engine = "CYCLES"
prefs = bpy.context.preferences.addons["cycles"].preferences
for kind in ("OPTIX", "CUDA"):
    try:
        prefs.compute_device_type = kind
        prefs.get_devices()
        for d in prefs.devices:
            d.use = True
        sc.cycles.device = "GPU"
        break
    except Exception as e:
        print("no", kind, e)
sc.cycles.samples = samples
sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 1920, 1080
sc.view_settings.view_transform = "AgX"

world = bpy.data.worlds.new("ReviewWorld")
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
bg.inputs[0].default_value = (0.20, 0.22, 0.26, 1)
bg.inputs[1].default_value = 1.0
sc.world = world
for ob in list(bpy.data.objects):
    if ob.type in ("LIGHT", "CAMERA"):
        ob.hide_render = True


def add_light(name, rot, energy):
    ld = bpy.data.lights.new(name, "SUN")
    ld.energy = energy
    ld.angle = math.radians(3)
    lo = bpy.data.objects.new(name, ld)
    lo.rotation_euler = [math.radians(a) for a in rot]
    sc.collection.objects.link(lo)


add_light("Key", (48, 0, 150), 3.5)
add_light("Fill", (65, 0, -60), 0.8)

pts = [ob.matrix_world @ v.co for ob in bpy.data.objects if ob.type == "MESH" for v in ob.data.vertices]
lo = Vector([min(p[i] for p in pts) for i in range(3)])
hi = Vector([max(p[i] for p in pts) for i in range(3)])
mid, size = (lo + hi) / 2, hi - lo
print("BOUNDS", tuple(round(v, 4) for v in lo), tuple(round(v, 4) for v in hi))

cam_data = bpy.data.cameras.new("ReviewCam")
cam_data.clip_start = 0.01
cam = bpy.data.objects.new("ReviewCam", cam_data)
sc.collection.objects.link(cam)
sc.camera = cam


def shoot(name, loc, target, lens, ortho=None, up="Y"):
    cam.location = Vector(loc)
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat("-Z", up).to_euler()
    cam_data.type = "ORTHO" if ortho else "PERSP"
    if ortho:
        cam_data.ortho_scale = ortho
    cam_data.lens = lens
    sc.render.filepath = os.path.join(out_dir, "%s_%s.png" % (prefix, name))
    bpy.ops.render.render(write_still=True)
    print("RENDER", sc.render.filepath, "ortho_width_m", ortho)


R = size.length
SURVEY = {
    # name: (direction the camera sits in, metres across the image width)
    "top": (Vector((0, 0, 1)), max(size.y, size.x * 16 / 9) * 1.08),
    "bottom": (Vector((0, 0, -1)), max(size.y, size.x * 16 / 9) * 1.08),
    "side_px": (Vector((1, 0, 0)), max(size.y, size.z * 16 / 9) * 1.08),
    "side_nx": (Vector((-1, 0, 0)), max(size.y, size.z * 16 / 9) * 1.08),
    "end_py": (Vector((0, 1, 0)), max(size.x, size.z * 16 / 9) * 1.15),
    "end_ny": (Vector((0, -1, 0)), max(size.x, size.z * 16 / 9) * 1.15),
}
QUARTER = {
    "q_px_py": Vector((1, 1, 0.6)), "q_px_ny": Vector((1, -1, 0.6)),
    "q_nx_py": Vector((-1, 1, 0.6)), "q_nx_ny": Vector((-1, -1, 0.6)),
}
# First person, in the source frame. Filled in after the survey: (camera, target, lens mm).
# Shooter looks to -Y; the shooter's right is -X. Hand point (grip top) is about (0, 0.12, 0.165).
# fp: eye 13 cm left of the gun, above the stock's comb (the thumbhole stock rises 15 cm over the grip) and 30 cm
# behind the grip; 90 deg horizontal FOV (18 mm). The game's own framing is set later in AGBGun's art table.
FP = {
    "fp": ((0.13, 0.42, 0.42), (0.13, -1.0, 0.33), 18),
    "fp_low": ((0.12, 0.30, 0.37), (0.12, -1.0, 0.31), 18),
    "fp_close": ((0.105, 0.26, 0.32), (0.0, -0.12, 0.21), 35),
    "fp_sight": ((0.07, 0.30, 0.34), (0.0, 0.08, 0.31), 70),
    "fp_screen": ((0.16, 0.16, 0.30), (0.04, -0.10, 0.20), 60),
    "left_receiver": ((0.34, 0.06, 0.24), (0.0, -0.06, 0.20), 50),
    "left_front": ((0.34, -0.22, 0.24), (0.0, -0.32, 0.17), 50),
    "left_stock": ((0.40, 0.36, 0.22), (0.0, 0.30, 0.17), 50),
    # the cables: the bundle on the left of the receiver, and the two at the front (left and right of the handguard)
    "cables_receiver": ((0.30, 0.13, 0.30), (0.035, 0.125, 0.245), 85),
    "cables_receiver_fp": ((0.13, 0.42, 0.42), (0.035, 0.12, 0.245), 85),
    "cables_receiver_top": ((0.10, 0.125, 0.50), (0.035, 0.125, 0.245), 85),
    "cables_front_left": ((0.32, -0.36, 0.22), (0.04, -0.36, 0.155), 85),
    "cables_front_right": ((-0.32, -0.36, 0.22), (-0.04, -0.36, 0.155), 85),
    "cables_front_fp": ((0.13, 0.42, 0.42), (0.04, -0.36, 0.16), 130),
}

for v in (views or list(SURVEY) + list(QUARTER)):
    if v in SURVEY:
        d, width = SURVEY[v]
        # top and bottom: the gun's length (Y) runs across the image
        shoot(v, mid + d * (R * 2), mid, 50, ortho=width, up="Y")
        if abs(d.z) > 0.5:
            cam.rotation_euler = (mid - cam.location).to_track_quat("-Z", "X").to_euler()
            bpy.ops.render.render(write_still=True)
    elif v in QUARTER:
        shoot(v, mid + QUARTER[v].normalized() * (R * 1.25), mid, 50)
    else:
        shoot(v, *FP[v])
