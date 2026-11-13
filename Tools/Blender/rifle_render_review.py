# Review renders of the rifle in its source frame (muzzle -Y, up +Z, metres), in Cycles: the player's first-person view,
# close-ups and scaled orthographic views. The owner judges reworked art on these before any Unreal import, so they
# must show what the game will show: the ammo hologram is drawn here the way M_RifleHolo draws it (additive, scanlines,
# edge fade), with the frame, count and pips of AGBGun::DrawViewModelDisplay.
# Views: fp (the game's framing), fp_close, left_side, left_receiver, left_front, right_side, rail_top_front,
# rail_top_rear, rail_34, tip_fp, glow_fp, sight_rear, repair_side, repair_fp; orthographic with a known scale:
# ortho_left_front, ortho_left_mid, ortho_top_front, ortho_rear. Camera places are the rifle's: copy and adapt them
# for another gun.
# Usage: blender -b --factory-startup --python rifle_render_review.py -- <Rifle.blend|model.glb> <out_dir> [prefix]
#        [dump_textures 0|1] [view,view,...]
import bpy, sys, os, math
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
path, out_dir = argv[0], argv[1]
prefix = argv[2] if len(argv) > 2 else "v"
dump = len(argv) > 3 and argv[3] == "1"
os.makedirs(out_dir, exist_ok=True)
if path.lower().endswith(".blend"):
    bpy.ops.wm.open_mainfile(filepath=path)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)

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
sc.cycles.samples = 96
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


def add_light(name, kind, rot, energy, size=0.0, loc=(0, 0, 0)):
    ld = bpy.data.lights.new(name, kind)
    ld.energy = energy
    if kind == "SUN":
        ld.angle = math.radians(3)
    else:
        ld.size = size
    lo = bpy.data.objects.new(name, ld)
    lo.rotation_euler = [math.radians(a) for a in rot]
    lo.location = loc
    sc.collection.objects.link(lo)


add_light("Key", "SUN", (48, 0, 150), 3.5)          # from above, behind-left of the shooter
add_light("Fill", "SUN", (65, 0, -60), 0.8)

# Review only: what the game draws on the hologram pane (AGBGun::DrawViewModelDisplay) as M_RifleHolo shows it
# (Tools/Jam/import_rifle.py): additive and see-through, scanlines, a faint tint stronger at the foot, soft edges.
# The count, the frame lines, the two side bars and one pip per round, in shares of the display from its top left.
pane = bpy.data.objects.get("Rifle_AmmoDisplay")
if pane is not None:
    from mathutils import Matrix
    import bmesh
    SHOWN, CAPACITY = 24, 30
    FRAME = [(0.10, 0.095, 0.78, 0.125), (0.08, 0.885, 0.60, 0.915), (0.075, 0.26, 0.10, 0.60), (0.90, 0.26, 0.925, 0.60)]
    PIP_ROW, PIP_FILL = (0.075, 0.70, 0.925, 0.80), 0.68
    GLOW, TINT, SCANLINES, SCAN_DEPTH, EDGE = 5.0, 0.05, 72.0, 0.45, (14.0, 10.0)
    HOLO_COLOUR = (0.05, 0.9, 0.6)
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


def shoot(name, loc, target, lens):
    cam.location = Vector(loc)
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam_data.lens = lens
    sc.render.filepath = os.path.join(out_dir, "%s_%s.png" % (prefix, name))
    bpy.ops.render.render(write_still=True)
    print("RENDER", sc.render.filepath)


# Shooter looks to -Y; the shooter's right is -X. Hand point (grip top) is about (-0.0055, 0.19, 0).
# First person: eye 9 cm left of the gun, 13 cm above the grip top, 20 cm behind it; 90 deg horizontal FOV (18 mm).
views = argv[4].split(",") if len(argv) > 4 else ["fp", "fp_close", "left_side", "left_receiver", "left_front", "right_side"]
SHOTS = {
    "fp": ((0.085, 0.42, 0.14), (0.085, -1.0, 0.11), 18),
    "fp_close": ((0.10, 0.33, 0.16), (-0.01, -0.05, 0.05), 35),
    "left_side": ((0.75, 0.0, 0.12), (0.0, 0.0, 0.0), 50),
    "left_receiver": ((0.32, 0.22, 0.10), (0.0, 0.08, 0.03), 50),
    "left_front": ((0.30, -0.10, 0.12), (0.0, -0.25, 0.03), 50),
    "right_side": ((-0.75, 0.0, 0.12), (0.0, 0.0, 0.0), 50),
    # the top rail: straight down (two halves), and a low three-quarter view along it
    "rail_top_front": ((-0.005, -0.27, 0.45), (-0.0051, -0.27, 0.09), 60),
    "rail_top_rear": ((-0.005, -0.03, 0.45), (-0.0051, -0.03, 0.09), 60),
    "rail_34": ((0.16, 0.16, 0.22), (-0.005, -0.15, 0.09), 50),
}
# Orthographic views with a known scale: (camera location, target, metres across the image width)
ORTHO = {
    "ortho_left_front": ((1.0, -0.30, 0.07), (0.0, -0.30, 0.07), 0.40),
    "ortho_left_mid": ((1.0, 0.05, 0.07), (0.0, 0.05, 0.07), 0.40),
    "ortho_top_front": ((-0.0055, -0.36, 1.0), (-0.0055, -0.36, 0.0), 0.30),
    "ortho_rear": ((-0.0055, 1.0, 0.11), (-0.0055, 0.0, 0.11), 0.16),
}
# player-like close-ups of the muzzle end and of the side glow
SHOTS["tip_fp"] = ((0.10, -0.10, 0.20), (0.0, -0.36, 0.09), 60)
SHOTS["glow_fp"] = ((0.13, -0.02, 0.17), (0.03, -0.26, 0.06), 60)
SHOTS["sight_rear"] = ((0.06, 0.40, 0.17), (-0.005, 0.20, 0.125), 70)
SHOTS["repair_side"] = ((0.50, -0.17, 0.16), (0.0, -0.17, 0.075), 85)
SHOTS["repair_fp"] = ((0.15, 0.10, 0.19), (0.03, -0.17, 0.07), 60)
for v in views:
    if v in ORTHO:
        cam_data.type = "ORTHO"
        cam_data.ortho_scale = ORTHO[v][2]
        shoot(v, ORTHO[v][0], ORTHO[v][1], 50)
        cam_data.type = "PERSP"
    else:
        shoot(v, *SHOTS[v])

if dump:
    for im in bpy.data.images:
        if im.size[0] > 0 and im.name not in ("Render Result", "Viewer Node"):
            c = im.copy()
            c.scale(2048, 2048)
            name = "tex_" + ("color" if im.name.startswith("Color_") else im.name)
            c.filepath_raw = os.path.join(out_dir, name + ".png")
            c.file_format = "PNG"
            c.save()
            print("TEX", c.filepath_raw)
