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
# and spiked, C bone and gold, D green scales), all facing -Y, standing in the pose of the body's right side. Armor 1
# rows: helmet, cuirass, one gauntlet, one boot. Armor 2 rows: belt + two thigh tubes (+ knee pieces in A and C), two
# upper-arm tubes, two round pads, two greaves. One skeletal mesh per set and slot (left and right merged), all on the
# orc's skeleton: SK_OrcArmor_<Set>_<Slot>. How each one is fitted: orc_armor_fit.py.
ARMOR_COLS = {"A": -0.39, "B": -0.13, "C": 0.125, "D": 0.385}
SIDES = {"r": "_r", "l": "_l"}


def armor_slot(fn, c, size):
    """(set, slot, side or None) of an island centre c in the line-up of file fn."""
    st = min(ARMOR_COLS, key=lambda k: abs(c.x - ARMOR_COLS[k]))
    dx = c.x - ARMOR_COLS[st]
    side = "r" if dx < 0 else "l"
    if fn == 1:
        slot = "Helmet" if c.z > 0.8 else "Chest" if c.z > 0.5 else "Hand" if c.z > 0.25 else "Foot"
        return st, slot, None
    if c.z > 0.55:
        if c.z > 0.75:
            return st, "Pelvis", None
        return (st, "Knee", side) if c.z < 0.615 else (st, "Thigh", side)
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


BOOT_H = 0.30        # collar height above the sole (m); the greave overlaps its top
BOOT_WALL = 0.024    # boot outer surface this far outside the bare foot (m)
BOOT_SOLE_DROP = 0.004


def _pct(vals, q):
    s = sorted(vals)
    return s[min(len(s) - 1, max(0, int(round(q * (len(s) - 1)))))]


def _smooth(a, k=2):
    out = []
    for i in range(len(a)):
        w = a[max(0, i - k): i + k + 1]
        out.append(sum(w) / len(w))
    return out


def add_boot_liner(piece, body, dom, sd, offset=0.003):
    """The line-up boot is a kit of separate plates; wherever they leave a gap the bare foot would show. A liner
    (the foot and ankle faces of the body, pushed LINER offset out along the normals, textured with the sole's flat
    black) closes the gaps. It is inside the plates everywhere else (the plates stand at least 6 mm off the foot)."""
    import bmesh
    wm = body.matrix_world
    body.data.calc_loop_triangles()
    keep = []
    for poly in body.data.polygons:
        names = [dom.get(i, "") for i in poly.vertices]
        if not all(n.startswith(("foot", "ball", "calf")) and n.endswith(sd) for n in names):
            continue
        if (wm @ poly.center).z > BOOT_H - 0.02:
            continue
        keep.append(poly)
    # UV of the sole's underside (flat black rubber)
    uvl = piece.data.uv_layers.active
    low = sorted(piece.data.vertices, key=lambda v: v.co.z)[:20]
    lowset = {v.index for v in low}
    uvs = [uvl.data[l].uv.copy() for p in piece.data.polygons for l in p.loop_indices
           if piece.data.loops[l].vertex_index in lowset]
    uv = sum(uvs, Vector((0, 0))) / len(uvs) if uvs else Vector((0.5, 0.5))
    bm = bmesh.new()
    bm.from_mesh(piece.data)
    uvlay = bm.loops.layers.uv.active
    vmap = {}
    for poly in keep:
        vl = []
        for i in poly.vertices:
            if i not in vmap:
                v = body.data.vertices[i]
                vmap[i] = bm.verts.new(wm @ v.co + (wm.to_3x3() @ v.normal).normalized() * offset)
            vl.append(vmap[i])
        try:
            f = bm.faces.new(vl)
        except ValueError:
            continue
        f.material_index = 0
        f.smooth = True
        for l in f.loops:
            l[uvlay].uv = uv
    bm.to_mesh(piece.data)
    bm.free()
    piece.data.update()


def fit_boot(piece, body, arm, dom, sd):
    """Wraps a line-up boot around the orc's foot and ankle. The boot is turned so its toe points where the foot
    points (foot bone head -> ball bone tip, yaw), then every vertex is mapped slice by slice (by height) from the
    boot's own cross-section box to the foot's cross-section box plus the wall, so the boot encloses the foot and
    ankle (toes included) whatever the proportions of the two. Frame L': origin at the ankle, toe towards -Y."""
    wm = body.matrix_world
    pts = [wm @ body.data.vertices[i].co for i, b in dom.items()
           if b.startswith(("foot", "ball", "calf")) and b.endswith(sd)]
    fh = arm.data.bones["foot" + sd].head_local
    bt = arm.data.bones["ball" + sd].tail_local
    P = Vector((fh.x, fh.y, 0.0))
    dv = Vector((bt.x - fh.x, bt.y - fh.y))
    phi = math.atan2(dv.y, dv.x) + math.pi / 2           # toe (-Y) turned by phi gives the foot direction
    Rf = Matrix.Rotation(phi, 3, "Z")
    Rfi = Rf.inverted()
    loc = [Rfi @ (p - P) for p in pts]
    # --- the boot: toe direction from the sole's centre against the shaft's centre (the line-up has it diagonal)
    vs = [v.co.copy() for v in piece.data.vertices]
    zmin_b = min(v.z for v in vs)
    zmax_b = max(v.z for v in vs)
    hb = zmax_b - zmin_b
    lo = [v for v in vs if v.z < zmin_b + 0.18 * hb]
    hi = [v for v in vs if v.z > zmin_b + 0.6 * hb]
    cl = sum((v for v in lo), Vector()) / len(lo)
    ch = sum((v for v in hi), Vector()) / len(hi)
    toe = Vector((cl.x - ch.x, cl.y - ch.y))
    ang = (-math.pi / 2) - math.atan2(toe.y, toe.x)
    R1 = Matrix.Rotation(ang, 3, "Z")
    vs = [R1 @ v for v in vs]
    # --- slices
    NB = 12
    zlo, zhi = -BOOT_SOLE_DROP, BOOT_H

    def knee_height(items, hmax, step, lo_frac, hi_frac):
        """Height where the footprint length (Y extent) drops below 62 % of its value at the sole: the top of the
        foot proper (toe box + instep) before the ankle and shaft begin."""
        ext = []
        h = 0.0
        while h <= hmax:
            sel = [p.y for p in items if abs(p.z - h) < step]
            ext.append((h, (max(sel) - min(sel)) if len(sel) > 3 else 0.0))
            h += step
        ref = max(e for _, e in ext[:3])
        for h, e in ext:
            if h > lo_frac * hmax and e < 0.62 * ref:
                return min(h, hi_frac * hmax)
        return hi_frac * hmax

    t_items = [Vector((p.x, p.y, (p.z - zmin_b) / hb)) for p in vs]
    t_i = knee_height(t_items, 1.0, 0.03, 0.2, 0.6)
    z_f = knee_height(loc, BOOT_H, 0.01, 0.25, 0.7)
    print("BOOT knee: boot t=%.2f -> foot z=%.1f cm" % (t_i, z_f * 100))

    def zmap(t):
        if t <= t_i:
            return zlo + (z_f - zlo) * t / t_i
        return z_f + (zhi - z_f) * (t - t_i) / (1 - t_i)

    def box(items, zc, half, minpts=12):
        sel = []
        while len(sel) < minpts and half < 0.2:
            sel = [p for p in items if abs(p.z - zc) < half]
            half *= 1.5
        if not sel:
            return None
        xs = [p.x for p in sel]
        ys = [p.y for p in sel]
        return (_pct(xs, 0.01), _pct(xs, 0.99), _pct(ys, 0.01), _pct(ys, 0.99))

    tb = []
    for k in range(NB + 1):
        t = k / NB
        tb.append(box([Vector((p.x, p.y, (p.z - zmin_b) / hb)) for p in vs], t, 0.5 / NB * 1.6, 40))
    tt = []
    last = None
    for k in range(NB + 1):
        zc = zmap(k / NB)
        b = box(loc, max(zc, 0.0), 0.02)
        tt.append(b if b else last)
        last = tt[-1]
    for k in range(NB + 1):               # same-shaped holes: fill
        if tt[k] is None:
            tt[k] = next(x for x in tt if x)
        if tb[k] is None:
            tb[k] = next(x for x in tb if x)

    def smooth_boxes(bx):
        cols = list(zip(*bx))
        return list(zip(*[_smooth(list(c), 1) for c in cols]))

    tb = smooth_boxes(tb)
    tt = smooth_boxes(tt)
    mat_warn = 0
    for v in piece.data.vertices:
        p = vs[v.index]
        t = (p.z - zmin_b) / hb
        f = min(NB - 1e-6, max(0.0, t * NB))
        k = int(f)
        u = f - k
        bb = [tb[k][i] * (1 - u) + tb[k + 1][i] * u for i in range(4)]
        ta = [tt[k][i] * (1 - u) + tt[k + 1][i] * u for i in range(4)]
        out = []
        for a0 in (0, 2):
            cb = (bb[a0] + bb[a0 + 1]) / 2
            eb = max(bb[a0 + 1] - bb[a0], 1e-4)
            ct = (ta[a0] + ta[a0 + 1]) / 2
            et = (ta[a0 + 1] - ta[a0]) + 2 * BOOT_WALL
            r = et / eb
            out.append(ct + (p[a0 // 2] - cb) * r)
        z = zmap(min(1.0, max(0.0, t)))
        q = Vector((out[0], out[1], z))
        w = P + Rf @ q
        v.co = Vector((w.x, w.y, z))
    piece.data.update()
    # --- poke test: body foot vertices outside the boot's surface (normal side)
    import bmesh
    from mathutils.bvhtree import BVHTree
    from mathutils.kdtree import KDTree
    chk = [p for p in pts if p.z < BOOT_H - 0.02]
    RAD = 0.04
    MARGIN = 0.004                           # the bare foot ends this far inside the boot's surface

    def pokes():
        bm = bmesh.new()
        bm.from_mesh(piece.data)
        bm.normal_update()
        tree = BVHTree.FromBMesh(bm)
        out = []
        for p in chk:
            loc_, nrm, idx, dist = tree.find_nearest(p)
            if loc_ is not None:
                s = (p - loc_).dot(nrm)
                if s > -MARGIN:
                    out.append((p, loc_, nrm, s))
        bm.free()
        return out

    # inflate the boot where the foot still pokes through (a local, smooth push along the surface normal)
    for it in range(int(os.environ.get("BOOT_INFLATE", "4"))):
        pk = pokes()
        if not pk:
            break
        kd = KDTree(len(piece.data.vertices))
        for v in piece.data.vertices:
            kd.insert(v.co, v.index)
        kd.balance()
        push = {}
        for p, loc_, nrm, s in pk:
            amount = s + MARGIN
            for co, idx, dist in kd.find_range(loc_, RAD):
                wgt = math.exp(-(dist / (RAD * 0.6)) ** 2) * amount
                if wgt > push.get(idx, (0, None))[0]:
                    push[idx] = (wgt, nrm.copy())
        for idx, (a, n) in push.items():
            piece.data.vertices[idx].co += n * a
        piece.data.update()
    poke = len([1 for p, loc_, nrm, s in pokes() if s > 0.002])
    add_boot_liner(piece, body, dom, sd)
    return {"boot_toe_turn_deg": round(math.degrees(ang), 1), "foot_yaw_deg": round(math.degrees(phi), 1),
            "poke_verts": poke, "checked": len(chk)}


_BODY_TREES = {}


def _body_tree(body):
    import bmesh
    from mathutils.bvhtree import BVHTree
    if id(body) not in _BODY_TREES:
        bm = bmesh.new()
        bm.from_mesh(body.data)
        bm.transform(body.matrix_world)
        bm.normal_update()
        _BODY_TREES[id(body)] = BVHTree.FromBMesh(bm)
        bm.free()
    return _BODY_TREES[id(body)]


MAX_EDGE = 0.03      # m: plates are cut into pieces no longer than this before they are wrapped round the body


def subdivide_long_edges(piece, max_edge, passes=3):
    """Long flat triangles of a low-poly plate cut straight through the body's curves even when every corner is
    outside: cut them down first (UVs interpolate)."""
    import bmesh
    for _ in range(passes):
        bm = bmesh.new()
        bm.from_mesh(piece.data)
        long_edges = [e for e in bm.edges if e.calc_length() > max_edge]
        if not long_edges:
            bm.free()
            break
        bmesh.ops.subdivide_edges(bm, edges=long_edges, cuts=1, use_grid_fill=True)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
        bm.to_mesh(piece.data)
        bm.free()
        piece.data.update()


def wrap_outside(piece, body, pts, axis=None, margin=0.008, max_depth=0.05, smooth_iters=12):
    """Plates are thin and the orc is bulky: where a vertex of the piece ended up inside the body it is pushed out,
    radially from the slot's axis (a bone line; a vertical line through the region for torso, head and belt), to
    just outside the body's surface along that direction. The push is a scalar field per vertex, dilated and smoothed
    over the piece's edges (so the plate bulges like cloth instead of tearing, and ornaments move with it), and the
    radial direction is smooth where the body's own normals are not. Plates are cut into small triangles first so
    they do not slice through the body's curves between their corners."""
    tree = _body_tree(body)
    subdivide_long_edges(piece, MAX_EDGE)
    me = piece.data
    n = len(me.vertices)
    if axis is None:
        cx = sum(p.x for p in pts) / len(pts)
        cy = sum(p.y for p in pts) / len(pts)
        a0, a1 = Vector((cx, cy, 0.0)), Vector((cx, cy, 3.0))
    else:
        a0, a1 = Vector(axis[0]), Vector(axis[1])
    ad = (a1 - a0).normalized()
    dirs = []
    req = [0.0] * n
    hit = 0
    for v in me.vertices:
        c = a0 + ad * (v.co - a0).dot(ad)
        d = v.co - c
        if d.length < 0.02:
            dirs.append(None)
            continue
        d.normalize()
        dirs.append(d)
        loc, nrm, idx, dist = tree.ray_cast(v.co, d, max_depth)
        if loc is not None and nrm.dot(d) > 0:
            req[v.index] = dist + margin
            hit += 1
    if not hit:
        return 0
    adj = [[] for _ in range(n)]
    for e in me.edges:
        x, y = e.vertices
        adj[x].append(y)
        adj[y].append(x)
    f = req[:]
    for _ in range(smooth_iters):
        new = []
        for i in range(n):
            if adj[i]:
                avg = sum(f[j] for j in adj[i]) / len(adj[i])
                new.append(f[i] * 0.5 + avg * 0.5)
            else:
                new.append(f[i])
        f = [max(new[i], req[i]) for i in range(n)]
    for v in me.vertices:
        if dirs[v.index] is not None and f[v.index] > 1e-5:
            v.co += dirs[v.index] * f[v.index]
    me.update()
    return hit


# The fit itself (tubes, cuirass, helmet, gauntlets, belt, the painted undersuit, do_armor) is in orc_armor_fit.py.
exec(compile(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "orc_armor_fit.py")).read(), "orc_armor_fit.py", "exec"))


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
