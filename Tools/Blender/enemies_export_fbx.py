# Exports the ASSETS deliveries of 2026-11-13 (Tripo GLBs, Glyphbound/SourceArt/...) as FBX files + textures for Unreal.
#   blender -b --python enemies_export_fbx.py -- <stage_dir> <asset> [<asset> ...]
# assets: rifle robot orc drone sentinel spider valkyrie sword armor
# Every asset is written to <stage_dir>/<Asset>/: FBX files, T_*.png textures and report.json (sockets, pivots, sizes).
# Frames: Blender metres, +Z up. Skeletal characters keep the Tripo/UE-mannequin frame (face -Y; the actor turns the
# mesh -90 yaw, like the UE mannequin). Static parts are turned so their front looks to +X (Unreal forward).
# FBX settings: the same as the guns and the Entity (axis_forward -Z, axis_up Y, apply_unit_scale: 1 m = 100 uu).
import bpy, bmesh, sys, os, json, math
from mathutils import Matrix, Vector

ARGS = sys.argv[sys.argv.index("--") + 1:]
STAGE = ARGS[0]
ASSETS = ARGS[1:]
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "Glyphbound", "SourceArt"))
TURN = Matrix.Rotation(math.pi / 2, 4, "Z")   # -Y front -> +X front


def src(*p):
    return os.path.join(ROOT, *p)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load(path):
    reset()
    bpy.ops.import_scene.gltf(filepath=path)
    for o in list(bpy.context.scene.objects):
        if o.name.startswith("Icosphere"):
            bpy.data.objects.remove(o)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    return meshes, arms


def out_dir(asset):
    d = os.path.join(STAGE, asset)
    os.makedirs(d, exist_ok=True)
    return d


def save_textures(mat, prefix, d):
    """Writes the glTF material's images: BaseColor, Normal, MR (glTF metallicRoughness: G roughness, B metal)."""
    found = {}
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")

    def image_upstream(sock):
        stack = [sock]
        while stack:
            s = stack.pop()
            for l in s.links:
                n = l.from_node
                if n.type == "TEX_IMAGE":
                    return n.image
                stack.extend(i for i in n.inputs if i.is_linked)
        return None

    for key, sock in (("BaseColor", "Base Color"), ("Normal", "Normal"), ("MR", "Roughness"), ("MR2", "Metallic")):
        img = image_upstream(bsdf.inputs[sock])
        if img is not None:
            k = "MR" if key == "MR2" else key
            if k not in found:
                found[k] = img
    names = {}
    for k, img in found.items():
        p = os.path.join(d, "T_%s_%s.png" % (prefix, k))
        img.filepath_raw = p
        img.file_format = "PNG"
        img.save()
        names[k] = os.path.basename(p)
    return names


def flatten(objs):
    """Bakes the world transform into each object (mesh data or armature rest), clears parents."""
    for o in objs:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def islands(o):
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    seen = set()
    out = []
    for v in bm.verts:
        if v.index in seen:
            continue
        st = [v]
        seen.add(v.index)
        isl = []
        while st:
            a = st.pop()
            isl.append(a.index)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in seen:
                    seen.add(b.index)
                    st.append(b)
        out.append(isl)
    bm.free()
    out.sort(key=len, reverse=True)
    co = [v.co.copy() for v in o.data.vertices]
    info = []
    for isl in out:
        cs = [co[i] for i in isl]
        mn = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs)))
        mx = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
        info.append({"verts": isl, "min": mn, "max": mx, "c": (mn + mx) / 2, "s": mx - mn})
    return info


def split_by_verts(o, keep_verts, name):
    """Duplicates o keeping only faces whose vertices are all in keep_verts. Returns the new object."""
    keep = set(keep_verts)
    n = o.copy()
    n.data = o.data.copy()
    n.name = name
    bpy.context.scene.collection.objects.link(n)
    bm = bmesh.new()
    bm.from_mesh(n.data)
    bm.verts.ensure_lookup_table()
    dead = [v for v in bm.verts if v.index not in keep]
    bmesh.ops.delete(bm, geom=dead, context="VERTS")
    bm.to_mesh(n.data)
    bm.free()
    return n


def transform_mesh(o, m):
    o.data.transform(m)
    o.data.update()


def set_material_name(o, name):
    for i, m in enumerate(o.data.materials):
        if m is not None:
            m.name = name


def export_static(o, path):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", use_mesh_modifiers=False,
                             add_leaf_bones=False, bake_anim=False, use_custom_props=False, path_mode="STRIP",
                             axis_forward="-Z", axis_up="Y")
    print("WROTE", path)


UNIT = 1.0      # 1.0 while building in metres; 100.0 once to_cm() has run (spider clips scale their offsets by it)


def to_cm(arm, meshes):
    """Scene unit 1 cm, armature and meshes scaled x100 and applied, so the FBX has no unit scale on any node."""
    global UNIT
    bpy.context.scene.unit_settings.scale_length = 0.01
    for ob in [arm] + list(meshes):
        ob.parent = None
    bpy.ops.object.select_all(action="DESELECT")
    for ob in [arm] + list(meshes):
        ob.scale = (100.0, 100.0, 100.0)
        ob.location = ob.location * 100.0
        ob.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for ob in meshes:
        ob.parent = arm
        ob.matrix_parent_inverse = Matrix.Identity(4)
    UNIT = 100.0


def export_skeletal(arm, meshes, path, anim=False):
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = arm
    # The scene must already be in centimetres (to_cm): exported from metres, the root bone came into Unreal with
    # scale 100 (or 1 with the mesh in metres), and every animation then shrank the character to 1/100.
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE",
                             use_mesh_modifiers=False, add_leaf_bones=False, use_armature_deform_only=True,
                             primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
                             bake_anim=anim, bake_anim_use_all_actions=anim, bake_anim_use_nla_strips=False,
                             bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
                             use_custom_props=False, path_mode="STRIP", axis_forward="-Z", axis_up="Y")
    print("WROTE", path)


def write_report(d, report):
    with open(os.path.join(d, "report.json"), "w") as f:
        json.dump(report, f, indent=1)
    print("REPORT", d, json.dumps(report)[:600])


def v3(v, scale=100.0):
    """Blender metres (export frame) -> Unreal cm, Unreal axes (x, -y, z): the FBX -Z/Y export flips Y."""
    return [round(v.x * scale, 2), round(-v.y * scale, 2), round(v.z * scale, 2)]


# ------------------------------------------------------------------------------------------------- rifle (enemy gun)
def do_rifle():
    d = out_dir("EnemyRifle")
    meshes, _ = load(src("Weapons", "EnemyRifle", "Rifle_for_Robots.glb"))
    o = meshes[0]
    flatten([o])
    # The gun lies diagonally in the GLB; its barrel runs 32 deg off +X with the muzzle towards -X after this turn.
    th = -math.atan2(0.474, 0.761) + math.pi
    transform_mesh(o, Matrix.Rotation(th, 4, "Z"))
    LENGTH = 0.80                       # provisional: a compact rifle for 1.7-2.1 m robots and orcs
    vs = [v.co for v in o.data.vertices]
    xmin, xmax = min(v.x for v in vs), max(v.x for v in vs)
    s = LENGTH / (xmax - xmin)
    ys = sorted(v.y for v in vs)
    yc = (ys[0] + ys[-1]) / 2
    grip = Vector((-0.26, yc, 0.26))    # top of the pistol grip, read off an orthographic side render (1 px = 1 mm)
    transform_mesh(o, Matrix.Scale(s, 4) @ Matrix.Translation(-grip))
    vs = [v.co for v in o.data.vertices]
    xmax = max(v.x for v in vs)
    tip = [v for v in vs if v.x > xmax - 0.02]
    muzzle = Vector((xmax, sum(v.y for v in tip) / len(tip), sum(v.z for v in tip) / len(tip)))
    o.name = "SM_EnemyRifle"
    set_material_name(o, "M_EnemyRifle")
    tex = save_textures(o.data.materials[0], "EnemyRifle", d)
    export_static(o, os.path.join(d, "SM_EnemyRifle.fbx"))
    write_report(d, {"textures": tex, "scale": s, "sockets": {"Muzzle": v3(muzzle)},
                     "length_cm": round(LENGTH * 100, 1)})


# ------------------------------------------------------------------------------------------------- skinned humanoids
def humanoid(glb, name, height, d, extra_root=False):
    meshes, arms = load(glb)
    arm = arms[0]
    body = [m for m in meshes if m.find_armature() == arm or m.parent == arm][0]
    flatten([arm, body])
    vs = [body.matrix_world @ v.co for v in body.data.vertices]
    zmin = min(v.z for v in vs)
    zmax = max(v.z for v in vs)
    s = height / (zmax - zmin)
    for ob in (arm, body):              # same scale and offset on both, applied separately (no parenting games)
        ob.scale = (s, s, s)
        ob.location = (0, 0, -zmin * s)
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    body.parent = arm
    body.matrix_parent_inverse = Matrix.Identity(4)
    vs = [body.matrix_world @ v.co for v in body.data.vertices]
    print("HEIGHT", name, round(max(v.z for v in vs) - min(v.z for v in vs), 3), "min z", round(min(v.z for v in vs), 3))
    arm.name = "Armature"
    arm.data.name = "Armature"
    if extra_root and "root" not in arm.data.bones:
        bpy.context.view_layer.objects.active = arm
        bpy.ops.object.mode_set(mode="EDIT")
        eb = arm.data.edit_bones
        old_root = [b for b in eb if b.parent is None][0]
        r = eb.new("root")
        r.head = (0, 0, 0)
        r.tail = (0, 0.2, 0)
        old_root.parent = r
        bpy.ops.object.mode_set(mode="OBJECT")
    body.name = "SK_" + name
    set_material_name(body, "M_" + name)
    tex = save_textures(body.data.materials[0], name, d)
    bones = {b.name: v3(arm.matrix_world @ b.head_local) for b in arm.data.bones}
    return arm, body, tex, s, bones


def do_robot():
    d = out_dir("Robot")
    arm, body, tex, s, bones = humanoid(src("Enemies", "Robot", "Robot.glb"), "Robot", 1.70, d)
    to_cm(arm, [body])
    export_skeletal(arm, [body], os.path.join(d, "SK_Robot.fbx"))
    write_report(d, {"textures": tex, "scale": s, "height_cm": 170, "bones": bones})


def do_orc():
    d = out_dir("Orc")
    arm, body, tex, s, bones = humanoid(src("Enemies", "Orc", "Base_Rigged_Orc.glb"), "Orc", 2.10, d)
    to_cm(arm, [body])
    export_skeletal(arm, [body], os.path.join(d, "SK_Orc.fbx"))
    write_report(d, {"textures": tex, "scale": s, "height_cm": 210, "bones": bones})


def do_valkyrie():
    d = out_dir("Valkyrie")
    arm, body, tex, s, bones = humanoid(src("Enemies", "Valkyrie", "Valkiria.glb"), "Valkyrie", 1.95, d, extra_root=True)
    to_cm(arm, [body])
    export_skeletal(arm, [body], os.path.join(d, "SK_Valkyrie.fbx"))
    write_report(d, {"textures": tex, "scale": s, "height_cm": 195, "bones": bones})


# ------------------------------------------------------------------------------------------------- drone
def do_drone():
    d = out_dir("Drone")
    meshes, _ = load(src("Enemies", "Drone", "Drone.glb"))
    o = meshes[0]
    flatten([o])
    info = islands(o)
    SPAN = 0.80                                       # provisional: 80 cm rotor to rotor
    s = SPAN / 1.0
    hubs = [i for i in info if 0.03 < i["s"].x < 0.045 and 0.03 < i["s"].y < 0.045 and i["c"].z > 0.49]
    hubs.sort(key=lambda i: (math.atan2(i["c"].y, i["c"].x)))
    assert len(hubs) == 4, "drone: expected 4 rotor hubs, found %d" % len(hubs)
    eye = info[0]
    assert eye["s"].x < 0.15 and eye["c"].y < -0.2, "drone: island 0 is not the eye ball"
    groups = {"Eye": set(eye["verts"])}
    rotor_of = {}
    for k, h in enumerate(hubs):
        groups["Rotor_%d" % k] = set(h["verts"])
    lo, hi = eye["min"] - Vector((0.005,) * 3), eye["max"] + Vector((0.005,) * 3)
    for i in info:
        if i is eye or i in hubs:
            continue
        c = i["c"]
        if all(lo[a] <= i["min"][a] and i["max"][a] <= hi[a] for a in range(3)):
            groups["Eye"].update(i["verts"])
            continue
        if c.z >= 0.475:
            dists = [((c.x - h["c"].x) ** 2 + (c.y - h["c"].y) ** 2) ** 0.5 for h in hubs]
            k = min(range(4), key=lambda j: dists[j])
            if dists[k] < 0.03 or (i["s"].z < 0.03 and c.z > 0.49 and dists[k] < 0.30):
                groups["Rotor_%d" % k].update(i["verts"])
    moving = set().union(*groups.values())
    groups["Body"] = set(range(len(o.data.vertices))) - moving
    # export frame: turned to +X, scaled, origin at the centre of the body's bounds
    body_vs = [o.data.vertices[i].co for i in groups["Body"]]
    bmin = Vector((min(v.x for v in body_vs), min(v.y for v in body_vs), min(v.z for v in body_vs)))
    bmax = Vector((max(v.x for v in body_vs), max(v.y for v in body_vs), max(v.z for v in body_vs)))
    centre = (bmin + bmax) / 2
    to_export = Matrix.Scale(s, 4) @ TURN @ Matrix.Translation(-centre)
    set_material_name(o, "M_Drone")
    tex = save_textures(o.data.materials[0], "Drone", d)
    sockets = {}
    for name, verts in groups.items():
        part = split_by_verts(o, verts, "SM_Drone_" + name)
        if name == "Body":
            pivot = centre
        elif name == "Eye":
            pivot = eye["c"]
        else:
            h = hubs[int(name.split("_")[1])]
            pivot = Vector((h["c"].x, h["c"].y, h["c"].z))
        if name != "Body":
            sockets[name] = v3(to_export @ pivot)
        transform_mesh(part, Matrix.Scale(s, 4) @ TURN @ Matrix.Translation(-pivot))
        export_static(part, os.path.join(d, "SM_Drone_%s.fbx" % name))
        print("PART", name, len(verts))
    write_report(d, {"textures": tex, "scale": s, "sockets": sockets, "span_cm": SPAN * 100,
                     "bottom_cm": round((bmin.z - centre.z) * s * 100, 1)})


# ------------------------------------------------------------------------------------------------- sentinel
def do_sentinel():
    d = out_dir("Sentinel")
    meshes, _ = load(src("Enemies", "Sentinel", "Centinel.glb"))
    o = meshes[0]
    flatten([o])
    info = islands(o)
    DIAM = 1.0                                         # provisional: a 1 m sphere
    s = DIAM / 1.0
    core = set()
    for i in info:
        c = i["c"]
        if abs(c.x) < 0.17 and abs(c.z - 0.5) < 0.2 and c.y < -0.38:
            core.update(i["verts"])
    vs = [v.co for v in o.data.vertices]
    centre = Vector(((min(v.x for v in vs) + max(v.x for v in vs)) / 2, (min(v.y for v in vs) + max(v.y for v in vs)) / 2,
                     (min(v.z for v in vs) + max(v.z for v in vs)) / 2))
    core_vs = [o.data.vertices[i].co for i in core]
    core_c = Vector((sum(v.x for v in core_vs) / len(core_vs), sum(v.y for v in core_vs) / len(core_vs),
                     sum(v.z for v in core_vs) / len(core_vs)))
    # The core turns about a point behind its face, inside the sphere, so it rolls in the opening like an eyeball.
    pivot = Vector((core_c.x, core_c.y + 0.12, core_c.z))
    to_export = Matrix.Scale(s, 4) @ TURN @ Matrix.Translation(-centre)
    set_material_name(o, "M_Sentinel")
    tex = save_textures(o.data.materials[0], "Sentinel", d)
    shell = split_by_verts(o, set(range(len(o.data.vertices))) - core, "SM_Sentinel_Shell")
    transform_mesh(shell, to_export)
    export_static(shell, os.path.join(d, "SM_Sentinel_Shell.fbx"))
    c = split_by_verts(o, core, "SM_Sentinel_Core")
    transform_mesh(c, Matrix.Scale(s, 4) @ TURN @ Matrix.Translation(-pivot))
    export_static(c, os.path.join(d, "SM_Sentinel_Core.fbx"))
    write_report(d, {"textures": tex, "scale": s, "sockets": {"Core": v3(to_export @ pivot)},
                     "core_face_cm": v3(to_export @ core_c), "core_verts": len(core), "diameter_cm": DIAM * 100})


# ------------------------------------------------------------------------------------------------- spider (rig + clips)
# The GLB has no skeleton, six legs (at about +-45, +-90, +-133 deg around the body, 0 = the head, -Y) and a round
# display base. The base is dropped; every leg vertex is given to its nearest leg by angle, and inside a leg to the
# nearer of two bones (hip -> knee, knee -> tip), hard weights: it is a machine, its segments are rigid.
LEGS = {"L1": -45.0, "L2": -90.0, "L3": -133.0, "R1": 45.0, "R2": 90.0, "R3": 133.0}
TRIPOD_A = ("L1", "R2", "L3")
FPS = 30


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (a + ab * t - p).length


def do_spider():
    d = out_dir("Spider")
    meshes, _ = load(src("Enemies", "Spider", "Robot_Spider.glb"))
    o = meshes[0]
    flatten([o])
    info = islands(o)
    base = set()
    for i in info:
        if i["max"].z <= 0.046:
            base.update(i["verts"])
    keep = set(range(len(o.data.vertices))) - base
    sp = split_by_verts(o, keep, "SK_Spider")
    bpy.data.objects.remove(o)
    o = sp
    vs = [v.co for v in o.data.vertices]
    zmin = min(v.z for v in vs)
    transform_mesh(o, Matrix.Translation((0, 0, -zmin)))
    info = islands(o)
    body = set()
    for i in info:
        c = i["c"]
        r = (c.x ** 2 + c.y ** 2) ** 0.5
        if (r < 0.2 and i["min"].z > 0.2) or abs(c.x) < 0.05:
            body.update(i["verts"])
    co = [v.co.copy() for v in o.data.vertices]
    leg_of = {}
    for vi, p in enumerate(co):
        if vi in body:
            continue
        ang = math.degrees(math.atan2(p.x, -p.y))
        leg_of[vi] = min(LEGS, key=lambda k: abs(((ang - LEGS[k]) + 180) % 360 - 180))
    joints = {}
    for leg in LEGS:
        pts = [co[vi] for vi, l in leg_of.items() if l == leg]
        rad = lambda p: (p.x ** 2 + p.y ** 2) ** 0.5
        hip = min(pts, key=rad)
        tip = max(pts, key=lambda p: rad(p) - 3 * p.z)            # far out and low
        knee = max((p for p in pts if rad(p) > rad(hip) + 0.06), key=lambda p: p.z)
        joints[leg] = (hip.copy(), knee.copy(), tip.copy())
    body_vs = [co[i] for i in body]
    bc = Vector((0, 0, sum(v.z for v in body_vs) / len(body_vs)))
    # armature
    arm_data = bpy.data.armatures.new("Armature")
    arm = bpy.data.objects.new("Armature", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm_data.edit_bones
    root = eb.new("root"); root.head = (0, 0, 0); root.tail = (0, 0.15, 0)
    bb = eb.new("body"); bb.head = bc; bb.tail = bc + Vector((0, 0.15, 0)); bb.parent = root
    for leg, (hip, knee, tip) in joints.items():
        u = eb.new("leg_%s_upper" % leg); u.head = hip; u.tail = knee; u.parent = bb
        l = eb.new("leg_%s_lower" % leg); l.head = knee; l.tail = tip; l.parent = u; l.use_connect = True
    bpy.ops.object.mode_set(mode="OBJECT")
    # weights
    for g in ["body"] + ["leg_%s_%s" % (k, s) for k in LEGS for s in ("upper", "lower")]:
        o.vertex_groups.new(name=g)
    for vi, p in enumerate(co):
        if vi in body:
            o.vertex_groups["body"].add([vi], 1.0, "REPLACE")
            continue
        leg = leg_of[vi]
        hip, knee, tip = joints[leg]
        if (p - hip).length < 0.03 and p.z > hip.z - 0.03:
            o.vertex_groups["body"].add([vi], 1.0, "REPLACE")
            continue
        g = "upper" if seg_dist(p, hip, knee) <= seg_dist(p, knee, tip) else "lower"
        o.vertex_groups["leg_%s_%s" % (leg, g)].add([vi], 1.0, "REPLACE")
    o.parent = arm
    mod = o.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    set_material_name(o, "M_Spider")
    tex = save_textures(o.data.materials[0], "Spider", d)
    to_cm(arm, [o])
    make_spider_clips(arm, joints, bc)
    export_skeletal(arm, [o], os.path.join(d, "SK_Spider.fbx"), anim=True)
    write_report(d, {"textures": tex, "height_cm": round(max(v.co.z for v in o.data.vertices) * 100, 1),
                     "joints": {k: [v3(x) for x in j] for k, j in joints.items()},
                     "clips": [a.name for a in bpy.data.actions]})


def spider_pose(arm, joints, body_z=0.0, body_pitch=0.0, legs=None, body_roll=0.0):
    """legs: {leg: (swing_deg, lift_deg, knee_deg)}; swing = yaw about the body's Z (+ = towards the head), lift =
    the upper bone raised about its hinge, knee = extra bend of the lower bone about the same hinge."""
    pb = arm.pose.bones
    pb["body"].location = (0, 0, 0)
    rest_b = arm.data.bones["body"].matrix_local.to_3x3()
    # body translation in armature space -> bone local
    pb["body"].location = rest_b.inverted() @ Vector((0, 0, body_z * UNIT))
    q = Matrix.Rotation(math.radians(body_roll), 3, "Y") @ Matrix.Rotation(math.radians(body_pitch), 3, "X")
    pb["body"].rotation_quaternion = (rest_b.inverted() @ q @ rest_b).to_quaternion()
    for leg, (hip, knee, tip) in joints.items():
        swing, lift, bend = (legs or {}).get(leg, (0.0, 0.0, 0.0))
        radial = Vector((hip.x, hip.y, 0)).normalized()
        hinge = Vector((0, 0, 1)).cross(radial).normalized()      # horizontal, perpendicular to the leg
        side = 1.0 if leg.startswith("L") else -1.0
        # + swing moves the foot towards the head (-Y): yaw sign depends on the side
        q_up = Matrix.Rotation(math.radians(swing * side), 3, "Z") @ Matrix.Rotation(math.radians(-lift), 3, hinge)
        ru = arm.data.bones["leg_%s_upper" % leg].matrix_local.to_3x3()
        pb["leg_%s_upper" % leg].rotation_quaternion = (ru.inverted() @ q_up @ ru).to_quaternion()
        rl = arm.data.bones["leg_%s_lower" % leg].matrix_local.to_3x3()
        q_lo = Matrix.Rotation(math.radians(bend), 3, hinge)
        pb["leg_%s_lower" % leg].rotation_quaternion = (rl.inverted() @ q_lo @ rl).to_quaternion()


def key_all(arm, frame):
    for p in arm.pose.bones:
        p.keyframe_insert("location", frame=frame)
        p.keyframe_insert("rotation_quaternion", frame=frame)


def make_spider_clips(arm, joints, bc):
    for p in arm.pose.bones:
        p.rotation_mode = "QUATERNION"
    arm.animation_data_create()
    scene = bpy.context.scene
    scene.render.fps = FPS

    def clip(name, frames, fn):
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data.action = act
        for f in range(frames + 1):
            fn(f / frames, f)
            key_all(arm, f)
        print("CLIP", name, frames)

    def idle(t, f):
        w = math.sin(2 * math.pi * t)
        spider_pose(arm, joints, body_z=0.01 * w, body_pitch=1.5 * w,
                    legs={k: (0, 2 * math.sin(2 * math.pi * t + i), 3 * w) for i, k in enumerate(LEGS)})

    def walk(t, f):
        legs = {}
        for k in LEGS:
            ph = (t + (0.0 if k in TRIPOD_A else 0.5)) % 1.0
            if ph < 0.5:   # stance: foot slides back on the ground
                s = 1 - 4 * ph
                legs[k] = (14 * s, 0, 0)
            else:          # swing: lifted, forward
                u = (ph - 0.5) * 2
                s = -1 + 2 * u
                lift = math.sin(math.pi * u)
                legs[k] = (14 * s, 22 * lift, -18 * lift)
        spider_pose(arm, joints, body_z=0.012 * abs(math.sin(4 * math.pi * t)), body_pitch=0, legs=legs)

    def attack(t, f):
        if t < 0.45:      # rear up, front legs raised
            u = t / 0.45
            e = math.sin(u * math.pi / 2)
            spider_pose(arm, joints, body_z=0.06 * e, body_pitch=-14 * e,
                        legs={"L1": (10 * e, 45 * e, -35 * e), "R1": (10 * e, 45 * e, -35 * e)})
        elif t < 0.65:    # strike down and forward
            u = (t - 0.45) / 0.2
            spider_pose(arm, joints, body_z=0.06 * (1 - u) - 0.03 * u, body_pitch=-14 + 26 * u,
                        legs={"L1": (10 + 10 * u, 45 - 55 * u, -35 + 25 * u), "R1": (10 + 10 * u, 45 - 55 * u, -35 + 25 * u)})
        else:             # back to rest
            u = (t - 0.65) / 0.35
            e = 1 - u
            spider_pose(arm, joints, body_z=-0.03 * e, body_pitch=12 * e,
                        legs={"L1": (20 * e, -10 * e, -10 * e), "R1": (20 * e, -10 * e, -10 * e)})

    def hit(t, f):
        e = math.sin(math.pi * t)
        spider_pose(arm, joints, body_z=0.02 * e, body_pitch=-10 * e, legs={k: (-6 * e, 8 * e, -6 * e) for k in LEGS})

    def death(t, f):
        u = min(1.0, t / 0.6)
        e = 1 - (1 - u) ** 2
        # flips onto its back (dead bug) and settles with the legs curled over the belly
        spider_pose(arm, joints, body_z=-(bc.z - 0.22) * e, body_pitch=0, body_roll=170 * e,
                    legs={k: (0, 10 * e, -70 * e) for k in LEGS})

    clip("Idle", 60, idle)
    clip("Walk", 18, walk)        # 0.6 s per cycle
    clip("Attack", 21, attack)
    clip("Hit", 8, hit)
    clip("Death", 24, death)
    arm.animation_data.action = None


# ------------------------------------------------------------------------------------------------- orc armor
# Orc_Armor_1.glb and Orc_Armor_2.glb are line-ups of loose pieces, four sets in four columns (A plain dark, B riveted
# and spiked, C bone and gold, D green scales), all facing -Y, standing up. Armor 1 rows: helmet, chest, one gauntlet,
# one boot. Armor 2 rows: belt + two thigh plates (+ knee pieces), two upper-arm plates + two round shoulder pads, two
# greaves. Each piece is fitted to the orc's body region of its slot (frame of the slot's bone: long axis along the
# bone, front -Y), scaled per axis to the region's size times a margin, then skinned by copying the weights of the
# nearest body vertex. A single gauntlet or boot is mirrored for the other side. One skeletal mesh per set and slot
# (left and right merged), all on the orc's skeleton: SK_OrcArmor_<Set>_<Slot>.
ARMOR_COLS = {"A": -0.39, "B": -0.13, "C": 0.125, "D": 0.385}
SIDES = {"r": "_r", "l": "_l"}
FRONT_GAP = 0.012
SLOT_DEF = {
    # slot: (region bones (prefix match, {s} = side), axis (from bone head, to bone head), margin (side, depth, along),
    #        coverage along the axis (0..1 of the region length, centred at `along_centre` of it))
    "Helmet": (["head"], ("neck_02", "head"), (1.18, 1.15, 1.05), 1.0, 0.55),
    "Chest": (["spine_03", "spine_04", "spine_05", "clavicle"], ("spine_02", "neck_01"), (1.08, 1.15, 1.0), 1.0, 0.5),
    "Pelvis": (["pelvis", "spine_01"], ("pelvis", "spine_02"), (1.1, 1.15, 1.0), 1.0, 0.45),
    "Thigh": (["thigh_{s}", "thigh_twist_01_{s}", "thigh_twist_02_{s}"], ("thigh_{s}", "calf_{s}"), (1.2, 1.2, 1.0), 0.8, 0.55),
    "Shin": (["calf_{s}", "calf_twist_01_{s}", "calf_twist_02_{s}"], ("calf_{s}", "foot_{s}"), (1.22, 1.22, 1.0), 0.85, 0.45),
    "UpperArm": (["upperarm_{s}", "upperarm_twist_01_{s}", "upperarm_twist_02_{s}"], ("upperarm_{s}", "lowerarm_{s}"), (1.25, 1.25, 1.0), 0.7, 0.55),
    "Shoulder": (["upperarm_{s}", "upperarm_twist_01_{s}"], ("upperarm_{s}", "lowerarm_{s}"), (1.6, 1.6, 1.0), 0.35, 0.12),
    "Hand": (["lowerarm_{s}", "lowerarm_twist_01_{s}", "lowerarm_twist_02_{s}", "hand_{s}", "index", "middle",
              "ring", "pinky", "thumb"], ("lowerarm_{s}", "hand_{s}"), (1.15, 1.2, 1.02), 1.0, 0.5),
    "Foot": (["foot_{s}", "ball_{s}", "calf_twist_02_{s}"], None, (1.15, 1.12, 1.05), 1.0, 0.5),
}


def armor_slot(fn, c, size):
    """(set, slot, side or None) of an island centre c in the line-up of file fn."""
    st = min(ARMOR_COLS, key=lambda k: abs(c.x - ARMOR_COLS[k]))
    dx = c.x - ARMOR_COLS[st]
    side = "r" if dx < 0 else "l"
    if fn == 1:
        slot = "Helmet" if c.z > 0.8 else "Chest" if c.z > 0.5 else "Hand" if c.z > 0.25 else "Foot"
        return st, slot, None
    if c.z > 0.55:
        return (st, "Pelvis", None) if abs(dx) < 0.06 and (size.x > 0.15 or c.z > 0.745) else (st, "Thigh", side)
    if c.z > 0.34:
        return st, "UpperArm", side
    if c.z > 0.25:
        return st, "Shoulder", side
    return st, "Shin", side


def append_glb(path):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.context.scene.objects if o not in before]
    for o in list(new):
        if o.name.startswith("Icosphere"):
            bpy.data.objects.remove(o)
            new.remove(o)
    return [o for o in new if o.type == "MESH"]


def body_regions(body, arm):
    """vertex index -> name of the bone with the largest weight."""
    names = {g.index: g.name for g in body.vertex_groups}
    dom = {}
    for v in body.data.vertices:
        if v.groups:
            g = max(v.groups, key=lambda x: x.weight)
            dom[v.index] = names[g.group]
    return dom


def fit_piece(piece, body, arm, dom, slot, side, mirror):
    bones_pat, axis, margin, cover, along_c = SLOT_DEF[slot]
    sd = SIDES[side] if side else ""
    pats = [p.replace("_{s}", sd) for p in bones_pat]
    pv0 = [v.co for v in piece.data.vertices]
    if slot == "Pelvis" and (max(v.z for v in pv0) - min(v.z for v in pv0)) > 0.6 * (max(v.x for v in pv0) - min(v.x for v in pv0)):
        # set B's belt and thigh plates are one island: fit it to the hips and both thighs
        pats = pats + ["thigh_"]
        cover = 0.75
    wm = body.matrix_world
    pts = [wm @ body.data.vertices[i].co for i, b in dom.items()
           if any(b == p or (not side and b.startswith(p)) or (side and b.startswith(p) and b.endswith(sd)) for p in pats)]
    if slot == "Foot":   # the boot also covers the ankle: add the lower part of the calf
        calf = [wm @ body.data.vertices[i].co for i, b in dom.items() if b in ("calf" + sd, "calf_twist_01" + sd)]
        if calf:
            zc = sorted(p.z for p in calf)
            pts += [p for p in calf if p.z < zc[len(zc) // 3]]
    if not pts:
        raise RuntimeError("no body region for %s %s" % (slot, side))
    if axis:
        h = arm.data.bones[axis[0].replace("_{s}", sd)].head_local
        t = arm.data.bones[axis[1].replace("_{s}", sd)].head_local
        a = (arm.matrix_world.to_3x3() @ (t - h)).normalized()
    else:
        a = Vector((0, 0, 1))
    f = Vector((0, -1, 0))
    f = (f - a * f.dot(a)).normalized()
    s = (-f).cross(a).normalized()
    R = Matrix((s, -f, a)).transposed()           # columns: piece X, Y, Z -> s, -f, a
    loc = [Vector((p.dot(s), p.dot(-f), p.dot(a))) for p in pts]
    rmin = Vector((min(v.x for v in loc), min(v.y for v in loc), min(v.z for v in loc)))
    rmax = Vector((max(v.x for v in loc), max(v.y for v in loc), max(v.z for v in loc)))
    rsize = rmax - rmin
    rc = (rmin + rmax) / 2
    rc.z = rmin.z + rsize.z * along_c
    # piece in its own frame (already centred by the caller)
    pv = [v.co for v in piece.data.vertices]
    if mirror:
        piece.data.transform(Matrix.Scale(-1, 4, (1, 0, 0)))
        piece.data.flip_normals()
    if slot == "Foot":
        # the line-up shows the boots from the side, toe towards -X: turn the toe to the front (-Y)
        piece.data.transform(Matrix.Rotation(math.pi / 2, 4, "Z"))
        pv = [v.co for v in piece.data.vertices]
    pmin = Vector((min(v.x for v in pv), min(v.y for v in pv), min(v.z for v in pv)))
    pmax = Vector((max(v.x for v in pv), max(v.y for v in pv), max(v.z for v in pv)))
    psize = pmax - pmin
    sc = Vector((rsize.x * margin[0] / psize.x, rsize.y * margin[1] / psize.y, rsize.z * cover * margin[2] / psize.z))
    g = (sc.x * sc.y * sc.z) ** (1 / 3)
    sc = Vector([max(g * 0.6, min(g * 1.6, x)) for x in sc])   # no wild stretching
    S = Matrix.Diagonal((sc.x, sc.y, sc.z, 1.0))
    # Plates are shells open at the back: centring them in the region buries them in the body. Every piece is
    # put with its front FRONT_GAP in front of the region's front instead (a tube still ends up around the limb).
    if slot in ("Thigh", "Shin", "UpperArm"):
        rc.y = rmin.y - FRONT_GAP - pmin.y * sc.y
    world_c = s * rc.x + (-f) * rc.y + a * rc.z
    M = Matrix.Translation(world_c) @ R.to_4x4() @ S
    piece.data.transform(M)
    piece.data.update()
    return {"scale": [round(x, 3) for x in sc], "centre_cm": v3(world_c)}


def transfer_weights(piece, body):
    from mathutils.kdtree import KDTree
    wm = body.matrix_world
    kd = KDTree(len(body.data.vertices))
    for v in body.data.vertices:
        kd.insert(wm @ v.co, v.index)
    kd.balance()
    names = {g.index: g.name for g in body.vertex_groups}
    for g in body.vertex_groups:
        piece.vertex_groups.new(name=g.name)
    for v in piece.data.vertices:
        near = kd.find_n(v.co, 4)
        acc = {}
        wsum = 0.0
        for co, idx, dist in near:
            w = 1.0 / max(dist, 1e-4)
            wsum += w
            for ge in body.data.vertices[idx].groups:
                acc[names[ge.group]] = acc.get(names[ge.group], 0.0) + ge.weight * w
        for n, w in acc.items():
            if w / wsum > 0.02:
                piece.vertex_groups[n].add([v.index], w / wsum, "REPLACE")


def do_armor():
    d = out_dir("OrcArmor")
    arm, body, tex, s, bones = humanoid(src("Enemies", "Orc", "Base_Rigged_Orc.glb"), "Orc", 2.10, d)
    dom = body_regions(body, arm)
    report = {"pieces": {}, "textures": {}}
    for fn in (1, 2):
        meshes = append_glb(src("Enemies", "Orc", "Orc_Armor_%d.glb" % fn))
        src_ob = meshes[0]
        flatten([src_ob])
        report["textures"]["Armor%d" % fn] = save_textures(src_ob.data.materials[0], "OrcArmor%d" % fn, d)
        src_ob.data.materials[0].name = "M_OrcArmor%d" % fn
        groups = {}
        for i in islands(src_ob):
            key = armor_slot(fn, i["c"], i["s"])
            groups.setdefault(key, set()).update(i["verts"])
        for (st, slot, side), verts in sorted(groups.items(), key=lambda kv: str(kv[0])):
            sides = [side] if side else (["r", "l"] if slot in ("Hand", "Foot") else [None])
            for sd in sides:
                p = split_by_verts(src_ob, verts, "SK_OrcArmor_%s_%s_%s" % (st, slot, sd or "c"))
                vs = [v.co for v in p.data.vertices]
                c = Vector(((min(v.x for v in vs) + max(v.x for v in vs)) / 2, (min(v.y for v in vs) + max(v.y for v in vs)) / 2,
                            (min(v.z for v in vs) + max(v.z for v in vs)) / 2))
                p.data.transform(Matrix.Translation(-c))
                # a single gauntlet / boot in Armor 1: assume it is the right one, mirror it for the left
                info = fit_piece(p, body, arm, dom, slot, sd, mirror=(fn == 1 and sd == "l"))
                transfer_weights(p, body)
                report["pieces"]["%s_%s_%s" % (st, slot, sd or "c")] = info
                print("FIT", st, slot, sd, info)
        bpy.data.objects.remove(src_ob)
    to_cm(arm, [o for o in bpy.context.scene.objects if o.type == "MESH"])
    # merge sides: one mesh per set and slot, then export each with the orc's armature
    by_slot = {}
    for o in [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith("SK_OrcArmor_")]:
        _, _, st, slot, _ = o.name.split("_")[:5]
        by_slot.setdefault((st, slot), []).append(o)
    for (st, slot), obs in sorted(by_slot.items()):
        bpy.ops.object.select_all(action="DESELECT")
        for o in obs:
            o.select_set(True)
        bpy.context.view_layer.objects.active = obs[0]
        if len(obs) > 1:
            bpy.ops.object.join()
        m = bpy.context.view_layer.objects.active
        m.name = "SK_OrcArmor_%s_%s" % (st, slot)
        m.parent = arm
        mod = m.modifiers.new("Armature", "ARMATURE")
        mod.object = arm
        export_skeletal(arm, [m], os.path.join(d, m.name + ".fbx"))
    body.hide_set(True)
    write_report(d, report)
    if "--blend" in ARGS:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(d, "armor_fit_debug.blend"))


# ------------------------------------------------------------------------------------------------- Valkyrie's sword
def do_sword():
    d = out_dir("ValkyrieSword")
    meshes, _ = load(src("Weapons", "ValkyrieSword", "Valkiria_Sword.glb"))
    o = meshes[0]
    flatten([o])
    LENGTH = 1.10                         # provisional; the GLB stands point down, hilt up, 1.0 long
    grip = Vector((0, 0, 0.83))           # middle of the grip (islands 4/5), read off a front render
    transform_mesh(o, Matrix.Rotation(-math.pi / 2, 4, "Y") @ Matrix.Scale(LENGTH, 4) @ Matrix.Translation(-grip))
    vs = [v.co for v in o.data.vertices]
    tip = max(vs, key=lambda v: v.x)
    o.name = "SM_ValkyrieSword"
    set_material_name(o, "M_ValkyrieSword")
    tex = save_textures(o.data.materials[0], "ValkyrieSword", d)
    export_static(o, os.path.join(d, "SM_ValkyrieSword.fbx"))
    write_report(d, {"textures": tex, "sockets": {"Tip": v3(tip)}, "length_cm": LENGTH * 100})


# ------------------------------------------------------------------------------------------------- dispatch
for a in ASSETS:
    if not a.startswith("--"):
        globals()["do_" + a]()
print("DONE", ASSETS)
