# Review renders of the shotgun in Cycles, adapted from rifle_render_review.py. The owner judges art on these before
# any Unreal import. Works on the Tripo FBX as it comes or on a .blend.
# Survey views are framed from the model's bounds, so they need no knowledge of the gun: top, bottom, left, right,
# front, rear (orthographic, scale printed) and four three-quarter views. First-person views are added by name once
# the gun's frame is known (see FP below).
# With "flat" as the last argument the gun is drawn unlit, its colour texture as it is: what the texture holds, with no
# help from the lights (used to judge the cables' texels).
# On Shotgun.blend the ammo hologram is drawn the way the game draws it (the rifle's M_RifleHolo: additive, scanlines,
# edge fade; frame, count and one pip per shell from AGBGun::DrawViewModelDisplay), so the render shows what the game
# will show. Poses, as the last argument: "pump" (the pump at the back of its travel), "shell" (the shell half-way
# out of the loading port), "pump+shell".
# Usage: blender -b --factory-startup --python shotgun_render_review.py -- <model.fbx|model.glb|Shotgun.blend> <out_dir>
#        [prefix] [view,view,...] [samples] [flat|lit] [pose]
import bpy, sys, os, math, glob
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
path, out_dir = argv[0], argv[1]
prefix = argv[2] if len(argv) > 2 else "v"
views = argv[3].split(",") if len(argv) > 3 and argv[3] else None
samples = int(argv[4]) if len(argv) > 4 else 64
flat = len(argv) > 5 and argv[5] == "flat"
pose = argv[6] if len(argv) > 6 else ""
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

if "pump" in pose and "Shotgun_Pump" in bpy.data.objects:
    ob = bpy.data.objects["Shotgun_Pump"]
    ob.location = Vector(ob.location) + Vector(ob["slide"])
if "shell" in pose and "Shotgun_Magazine" in bpy.data.objects:
    ob = bpy.data.objects["Shotgun_Magazine"]
    ob.location = Vector(ob.location) + Vector(ob["out"]) * 1.4

# Review only: what the game draws on the hologram pane (AGBGun::DrawViewModelDisplay) as the rifle's M_RifleHolo shows
# it: additive and see-through, scanlines, a faint tint stronger at the foot, soft edges. The count, the frame lines,
# the two side bars and one pip per shell, in shares of the display from its top left.
pane = bpy.data.objects.get("Shotgun_AmmoDisplay")
if pane is not None:
    from mathutils import Matrix
    import bmesh
    SHOWN, CAPACITY = 5, 6
    FRAME = [(0.10, 0.095, 0.78, 0.125), (0.08, 0.885, 0.60, 0.915), (0.075, 0.26, 0.10, 0.60), (0.90, 0.26, 0.925, 0.60)]
    PIP_ROW, PIP_FILL = (0.075, 0.70, 0.925, 0.80), 0.68
    GLOW, TINT, SCANLINES, SCAN_DEPTH, EDGE = 5.0, 0.05, 72.0, 0.45, (14.0, 10.0)
    HOLO_COLOUR = (0.10, 0.85, 1.00)
    pts = [pane.matrix_world @ v.co for v in pane.data.vertices]
    centre = sum(pts, Vector()) / len(pts)
    out_n = (pane.matrix_world.to_3x3() @ pane.data.polygons[0].normal).normalized()
    right = Vector((-1.0, 0.0, 0.0))
    up = out_n.cross(right).normalized()
    width = max((p - q).dot(right) for p in pts for q in pts)
    height = max((p - q).dot(up) for p in pts for q in pts)
    m = Matrix.Identity(4)
    for i, axis in enumerate((right, up, out_n)):
        for k in range(3):
            m[k][i] = axis[k]
    for k in range(3):
        m[k][3] = centre[k]

    def holo_material(name, ink):
        """Emission added over what is behind. Object coordinates: X right, Y up, origin at the pane's centre."""
        hm = bpy.data.materials.new(name)
        hm.use_nodes = True
        nt = hm.node_tree
        nt.nodes.clear()

        def math(op, x, y=None):
            n = nt.nodes.new("ShaderNodeMath")
            n.operation = op
            for i, val in enumerate((x, y)):
                if val is None:
                    continue
                if isinstance(val, (int, float)):
                    n.inputs[i].default_value = val
                else:
                    nt.links.new(val, n.inputs[i])
            return n.outputs[0]

        coord = nt.nodes.new("ShaderNodeTexCoord")
        coord.object = pane_frame
        xyz = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(coord.outputs["Object"], xyz.inputs["Vector"])
        u = math("ADD", math("DIVIDE", xyz.outputs["X"], width), 0.5)          # 0 left .. 1 right
        v = math("SUBTRACT", 0.5, math("DIVIDE", xyz.outputs["Y"], height))    # 0 top .. 1 foot
        scan = math("ADD", math("MULTIPLY", math("SINE", math("MULTIPLY", v, SCANLINES * 6.2831853)), SCAN_DEPTH / 2.0),
                    1.0 - SCAN_DEPTH / 2.0)

        def inset(c, gain):
            near = math("MINIMUM", c, math("SUBTRACT", 1.0, c))
            n = nt.nodes.new("ShaderNodeClamp")
            nt.links.new(math("MULTIPLY", near, gain), n.inputs["Value"])
            return n.outputs[0]

        edge = math("MULTIPLY", inset(u, EDGE[0]), inset(v, EDGE[1]))
        level = ink if ink is not None else math("MULTIPLY", math("ADD", math("MULTIPLY", v, 1.2), 0.4), TINT)
        strength = math("MULTIPLY", math("MULTIPLY", math("MULTIPLY", level, scan), edge), GLOW)
        emit = nt.nodes.new("ShaderNodeEmission")
        emit.inputs["Color"].default_value = HOLO_COLOUR + (1.0,)
        nt.links.new(strength, emit.inputs["Strength"])
        clear = nt.nodes.new("ShaderNodeBsdfTransparent")
        add = nt.nodes.new("ShaderNodeAddShader")
        nt.links.new(clear.outputs["BSDF"], add.inputs[0])
        nt.links.new(emit.outputs["Emission"], add.inputs[1])
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(add.outputs["Shader"], out.inputs["Surface"])
        return hm

    pane_frame = bpy.data.objects.new("ReviewHoloFrame", None)     # an empty: the pane's own axes
    pane_frame.matrix_world = m
    sc.collection.objects.link(pane_frame)
    lit, spent, glass = holo_material("ReviewHoloLit", 1.0), holo_material("ReviewHoloSpent", 0.16), holo_material("ReviewHoloPane", None)
    pane.data.materials.clear()
    pane.data.materials.append(glass)

    lift = Matrix.Translation((0.0, 0.0, 0.0002))
    curve = bpy.data.curves.new("ReviewAmmo", "FONT")
    curve.body = str(SHOWN)
    curve.align_x = "CENTER"
    curve.align_y = "CENTER"
    curve.size = 0.42 * height * 0.95
    curve.materials.append(lit)
    text = bpy.data.objects.new("ReviewAmmo", curve)
    text.matrix_world = m @ lift @ Matrix.Translation((0.0, (0.5 - 0.39) * height, 0.0))
    sc.collection.objects.link(text)
    fb = bmesh.new()

    def share_box(x0, y0, x1, y1, mat_index):
        f = fb.faces.new([fb.verts.new(((x - 0.5) * width, (0.5 - y) * height, 0.0))
                          for x, y in ((x0, y1), (x1, y1), (x1, y0), (x0, y0))])
        f.material_index = mat_index

    for box_shares in FRAME:
        share_box(*box_shares, 0)
    slot = (PIP_ROW[2] - PIP_ROW[0]) / CAPACITY
    for i in range(CAPACITY):
        x0 = PIP_ROW[0] + i * slot
        share_box(x0, PIP_ROW[1], x0 + slot * PIP_FILL, PIP_ROW[3], 0 if i < SHOWN else 1)
    frame_me = bpy.data.meshes.new("ReviewHoloLines")
    fb.to_mesh(frame_me)
    fb.free()
    frame_me.materials.append(lit)
    frame_me.materials.append(spent)
    lines = bpy.data.objects.new("ReviewHoloLines", frame_me)
    lines.matrix_world = m @ lift
    sc.collection.objects.link(lines)

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
    "fp_sight": ((0.13, 0.42, 0.42), (0.0085, -0.02, 0.352), 75),
    # the functional parts: the energy bar from above the stock, the loading port from below, the pump from the left
    "top_bar": ((0.10, 0.30, 0.47), (0.0, 0.10, 0.305), 70),
    "under_port": ((0.20, 0.10, -0.10), (0.0, -0.02, 0.105), 60),
    "under_left": ((0.42, -0.02, -0.06), (0.0, -0.16, 0.12), 45),
    "pump_left": ((0.42, -0.42, 0.06), (0.0, -0.33, 0.10), 60),
    "left_34": ((0.62, -0.62, 0.30), (0.0, -0.05, 0.19), 50),
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
