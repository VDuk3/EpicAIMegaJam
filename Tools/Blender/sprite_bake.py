# Bakes animated 3D characters into 8-direction pixel-art sprite sheets (Prompts/sprite_sheet_format.md).
#   blender -b --python Tools/Blender/sprite_bake.py -- <SheetId|All> [--clips Idle,Walk] [--dirs 0,2] [--debug]
# Jobs: Tools/Blender/sprite_jobs/<SheetId>.json. Output: Glyphbound/Content/Data/Sprites/<SheetId>.png + .json,
# review images in Glyphbound/Saved/SpriteBake/<SheetId>/ (contact sheets, a GIF per clip at d=0 and d=2).
#
# How a frame is made: Cycles renders, at Supersample x the cell size and with point sampling (1 sample, tiny pixel
# filter), the passes that matter: albedo, a material id, a bone-region id, the shading normal and depth. numpy then
# classifies each subpixel into a region (skin, leather, iron...), lights it with a fixed key light from the top left
# of the VIEW (the same for every direction), reduces each Supersample x Supersample block to one pixel (majority
# region, mean tone), quantizes the tone into the region's palette ramp, cleans islands, holes and specks, draws inner
# lines where a part overlaps another and a 1 px black outline outside.
import bpy, sys, os, json, math, time, shutil, subprocess
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True     # no __pycache__ next to the tools
sys.path.insert(0, HERE)
import importlib
import sprite_pixel as px
importlib.reload(px)
import OpenImageIO as oiio

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
JOBS = os.path.join(HERE, "sprite_jobs")
OUT_DATA = os.path.join(REPO, "Glyphbound", "Content", "Data", "Sprites")
OUT_REVIEW = os.path.join(REPO, "Glyphbound", "Saved", "SpriteBake")
TMP = os.path.join(OUT_REVIEW, "_tmp")

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def opt(name, default=None):
    if name in ARGS:
        i = ARGS.index(name)
        return ARGS[i + 1] if i + 1 < len(ARGS) and not ARGS[i + 1].startswith("--") else True
    return default


def log(*a):
    print("[sprite]", *a, flush=True)


# ---------------------------------------------------------------------------------------------- scene setup

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_fbx(path):
    before = set(bpy.data.objects)
    acts_before = set(bpy.data.actions)
    bpy.ops.import_scene.fbx(filepath=path, use_anim=True)
    new = [o for o in bpy.data.objects if o not in before]
    acts = [a for a in bpy.data.actions if a not in acts_before]
    return new, acts


def top_parent(o):
    while o.parent is not None:
        o = o.parent
    return o


def load_image(path):
    for im in bpy.data.images:
        if im.filepath == path:
            return im
    im = bpy.data.images.load(path)
    im.colorspace_settings.name = "sRGB"
    return im


SURVEY = int(opt("--survey", 0) or 0)
DEBUG_STATS = "--stats" in ARGS
MAT_IDS = {}       # material name (job key) -> integer id (1..)


def mat_key(name):
    # FBX import can suffix material names (.001); the job uses the base name
    return name.split(".")[0]


def setup_material(mat, tex_path, flat_color=None, alpha_cut=False):
    key = mat_key(mat.name)
    if key not in MAT_IDS:
        MAT_IDS[key] = len(MAT_IDS) + 1
    mid = MAT_IDS[key]
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    emi = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(emi.outputs[0], out.inputs["Surface"])
    if tex_path:
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = load_image(tex_path)
        tex.interpolation = "Closest"
        col = tex.outputs["Color"]
        if alpha_cut:
            # cut-out cards (hair): below half alpha the surface is a hole
            gt = nt.nodes.new("ShaderNodeMath")
            gt.operation = "GREATER_THAN"
            gt.inputs[1].default_value = 0.5
            nt.links.new(tex.outputs["Alpha"], gt.inputs[0])
            tr = nt.nodes.new("ShaderNodeBsdfTransparent")
            mix = nt.nodes.new("ShaderNodeMixShader")
            nt.links.new(gt.outputs[0], mix.inputs[0])
            nt.links.new(tr.outputs[0], mix.inputs[1])
            nt.links.new(emi.outputs[0], mix.inputs[2])
            nt.links.new(mix.outputs[0], out.inputs["Surface"])
    else:
        rgb = nt.nodes.new("ShaderNodeRGB")
        c = flat_color or (0.5, 0.5, 0.5)
        rgb.outputs[0].default_value = (c[0], c[1], c[2], 1.0)
        col = rgb.outputs[0]
    nt.links.new(col, emi.inputs["Color"])
    a1 = nt.nodes.new("ShaderNodeOutputAOV")
    a1.aov_name = "albedo"
    nt.links.new(col, a1.inputs["Color"])
    val = nt.nodes.new("ShaderNodeValue")
    val.outputs[0].default_value = float(mid)
    a2 = nt.nodes.new("ShaderNodeOutputAOV")
    a2.aov_name = "matid"
    nt.links.new(val.outputs[0], a2.inputs["Value"])
    attr = nt.nodes.new("ShaderNodeAttribute")
    attr.attribute_name = "gb_bone"
    a3 = nt.nodes.new("ShaderNodeOutputAOV")
    a3.aov_name = "boneid"
    nt.links.new(attr.outputs["Fac"], a3.inputs["Value"])


def bone_region_attribute(mesh_obj, bone_regions):
    """Writes a float point attribute gb_bone = 1 + index of the region of each vertex.
    bone_regions: list of [region_name, [bone name prefixes...]] or [region_name, [prefixes], {conditions}]; the first
    entry that matches wins. An empty prefix list matches any bone. Conditions test the vertex's rest position in
    metres, character frame (x, y, z world at import): x_abs_min, x_abs_max, y_min, y_max, z_min, z_max; and
    "mesh": a substring of the mesh object's name."""
    me = mesh_obj.data
    names = [g.name for g in mesh_obj.vertex_groups]
    mw = mesh_obj.matrix_world
    entries = []
    for e in bone_regions:
        entries.append((e[1], e[2] if len(e) > 2 else {}))

    def bone_ok(bone, prefixes):
        if not prefixes:
            return True
        return any(bone == p or bone.startswith(p) for p in prefixes)

    def cond_ok(co, cond):
        if "mesh" in cond and cond["mesh"] not in mesh_obj.name:
            return False
        if "x_abs_min" in cond and abs(co.x) < cond["x_abs_min"]:
            return False
        if "x_abs_max" in cond and abs(co.x) > cond["x_abs_max"]:
            return False
        for k, ax in (("y", 1), ("z", 2)):
            if k + "_min" in cond and co[ax] < cond[k + "_min"]:
                return False
            if k + "_max" in cond and co[ax] > cond[k + "_max"]:
                return False
        return True

    vals = np.zeros(len(me.vertices), np.float32)
    for v in me.vertices:
        best, bw = None, -1.0
        for g in v.groups:
            if g.weight > bw:
                best, bw = g.group, g.weight
        bone = names[best] if best is not None else ""
        co = mw @ v.co
        for i, (prefixes, cond) in enumerate(entries):
            if bone_ok(bone, prefixes) and cond_ok(co, cond):
                vals[v.index] = i + 1
                gr = cond.get("gradient")
                if gr:
                    # a designed root-to-tip gradient carried in the fraction (rigid parts such as wings: light
                    # edge, mid body, dark root, the same in every frame)
                    d = (co - Vector(gr["root"])).length
                    vals[v.index] += 0.9 * min(max((d - gr["min"]) / (gr["max"] - gr["min"]), 0.0), 1.0)
                break
    if "gb_bone" in me.attributes:
        me.attributes.remove(me.attributes["gb_bone"])
    at = me.attributes.new("gb_bone", "FLOAT", "POINT")
    at.data.foreach_set("value", vals)


class Rig:
    def __init__(self):
        self.parts = []        # (top empty, armature, [meshes])
        self.arm = None        # primary armature
        self.tops = []


def build_character(job, src_root):
    rig = Rig()
    bone_regions = job.get("BoneRegions", [])
    for part in job["Parts"]:
        new, _ = import_fbx(os.path.join(src_root, part["fbx"]))
        arm = next(o for o in new if o.type == "ARMATURE")
        meshes = [o for o in new if o.type == "MESH"]
        top = top_parent(arm)
        texmap = part.get("textures", {})
        for m in meshes:
            for slot in m.material_slots:
                if slot.material is None:
                    continue
                key = mat_key(slot.material.name)
                tp = texmap.get(key)
                if slot.material.users > 1 and key in MAT_IDS:
                    continue
                setup_material(slot.material, os.path.join(src_root, tp) if tp else None,
                               part.get("flat_colors", {}).get(key), key in part.get("alpha_cut", []))
            if bone_regions:
                bone_region_attribute(m, bone_regions)
        top.scale = top.scale * job.get("Scale", 1.0)
        rig.parts.append((top, arm, meshes))
        rig.tops.append(top)
        if rig.arm is None:
            rig.arm = arm
    bpy.context.view_layer.update()
    return rig


def axis_vec(s):
    sgn = -1.0 if s[0] == "-" else 1.0
    return Vector({"X": (1, 0, 0), "Y": (0, 1, 0), "Z": (0, 0, 1)}[s[-1]]) * sgn


def attach_weapon(job, rig, src_root):
    w = job.get("Weapon")
    if not w:
        return None
    new, _ = import_fbx(os.path.join(src_root, w["fbx"]))
    obj = next(o for o in new if o.type == "MESH")
    texmap = w.get("textures", {})
    for slot in obj.material_slots:
        if slot.material is None:
            continue
        key = mat_key(slot.material.name)
        tp = texmap.get(key)
        setup_material(slot.material, os.path.join(src_root, tp) if tp else None, w.get("flat_colors", {}).get(key))
    if not obj.material_slots:
        m = bpy.data.materials.new(w.get("material_name", "M_Weapon"))
        obj.data.materials.append(m)
        setup_material(m, None, w.get("flat_color"))
    # bake the import transform (scale 0.01) into the mesh so its local frame is metres
    mw = obj.matrix_world.copy()
    obj.parent = None
    obj.data.transform(mw)
    obj.matrix_world = Matrix.Identity(4)
    s = w.get("scale", 1.0) * job.get("Scale", 1.0)
    arm = rig.arm
    bpy.context.view_layer.update()
    bone = arm.data.bones[w.get("bone", "hand_r")]
    aw = arm.matrix_world
    if w.get("mode", "fist") == "fist":
        side = w.get("bone", "hand_r")[-1]
        def hp(name):
            return aw @ arm.data.bones[name].head_local
        Hh = hp(w.get("bone", "hand_r"))
        K = hp("middle_01_" + side)
        I = hp("index_01_" + side)
        P = hp("pinky_01_" + side)
        A = (I - P).normalized()                       # shaft direction: out of the thumb side of the fist
        fwd = (K - Hh) - A * (K - Hh).dot(A)
        fwd.normalize()                                # toward the knuckles
        nrm = A.cross(fwd).normalized()
        o = w.get("offset", [0.0, 0.0, 0.0])           # in (along shaft, toward knuckles, palm normal) metres
        C = Hh + (K - Hh) * w.get("grip_along_hand", 0.8) + A * o[0] + fwd * o[1] + nrm * o[2]
        # optional tilt of the shaft toward the knuckles (degrees), around nrm
        tilt = math.radians(w.get("tilt", 0.0))
        A2 = (Matrix.Rotation(tilt, 3, nrm) @ A).normalized()
        roll = math.radians(w.get("roll", 0.0))
        F2 = (Matrix.Rotation(roll, 3, A2) @ (fwd - A2 * fwd.dot(A2))).normalized()
        if w.get("flip", False):
            A2 = -A2
        wa = axis_vec(w.get("axis", "+Z"))
        wu = axis_vec(w.get("up", "+X"))
        src = Matrix((wa, wu, wa.cross(wu))).transposed()
        dst = Matrix((A2, F2, A2.cross(F2))).transposed()
        R = (dst @ src.inverted()).to_4x4()
        grip = Vector(w.get("grip", [0.0, 0.0, 0.0]))  # weapon local point held in the hand (metres)
        M = Matrix.Translation(C) @ R @ Matrix.Scale(s, 4) @ Matrix.Translation(-grip)
    else:
        # "unreal": socket offset in the bone frame, Unreal convention: x y z (cm) pitch yaw roll (deg)
        loc = Vector(w["ue_loc"]) * 0.01
        pitch, yaw, roll = [math.radians(a) for a in w["ue_rot"]]
        # Unreal rotator -> matrix (left-handed, Z up), then mirror Y into Blender's right-handed frame
        cp, sp, cy, sy, cr, sr = math.cos(pitch), math.sin(pitch), math.cos(yaw), math.sin(yaw), math.cos(roll), math.sin(roll)
        # Unreal FRotationMatrix rows = axes X, Y, Z
        X = Vector((cp * cy, cp * sy, sp))
        Y = Vector((sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp))
        Z = Vector((-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp))
        flipY = Matrix(((1, 0, 0), (0, -1, 0), (0, 0, 1)))
        Rue = Matrix((X, Y, Z)).transposed()
        Rb = flipY @ Rue @ flipY
        locb = flipY @ loc
        bl, br, _bs = (aw @ bone.matrix_local).decompose()
        bw = Matrix.LocRotScale(bl, br, None)
        # Unreal bone frame -> Blender bone frame: the FBX importer's bone axis correction
        corr = Matrix(w.get("bone_corr", [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]))
        M = bw @ corr @ Matrix.Translation(locb) @ Rb.to_4x4() @ Matrix.Scale(s, 4)
    obj.parent = arm
    obj.parent_type = "BONE"
    obj.parent_bone = bone.name
    bpy.context.view_layer.update()
    obj.matrix_world = M
    bpy.context.view_layer.update()
    return obj


def setup_render(job):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 1
    sc.cycles.use_adaptive_sampling = False
    sc.cycles.use_denoising = False
    sc.cycles.filter_width = 0.01
    sc.cycles.max_bounces = 0
    sc.cycles.pixel_filter_type = "BOX"
    sc.render.film_transparent = True
    W, H = job["Cell"]
    F = job.get("Supersample", 4)
    sc.render.resolution_x = W * F
    sc.render.resolution_y = H * F
    sc.render.resolution_percentage = 100
    sc.render.image_settings.media_type = "MULTI_LAYER_IMAGE"
    sc.render.image_settings.file_format = "OPEN_EXR_MULTILAYER"
    sc.render.image_settings.color_depth = "32"
    sc.render.image_settings.exr_codec = "NONE"
    sc.view_settings.view_transform = "Standard"
    vl = sc.view_layers[0]
    vl.use_pass_normal = True
    vl.use_pass_z = True
    for name, typ in (("albedo", "COLOR"), ("matid", "VALUE"), ("boneid", "VALUE")):
        a = vl.aovs.add()
        a.name = name
        a.type = typ
    world = bpy.data.worlds.new("W")
    sc.world = world
    cam_data = bpy.data.cameras.new("SpriteCam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = max(W, H) / job["PixelsPerMeter"]
    cam_data.clip_start = 0.1
    cam_data.clip_end = 100.0
    px_, py_ = job["Pivot"]
    cam_data.shift_x = (W / 2.0 - px_) / max(W, H)
    cam_data.shift_y = (py_ - H / 2.0) / max(W, H)
    cam = bpy.data.objects.new("SpriteCam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam


def place_camera(cam, job, d):
    fwd = Vector(job.get("Forward", [0, -1, 0])).normalized()
    ang = math.radians(45.0 * d)
    v = Matrix.Rotation(ang, 3, "Z") @ fwd                    # CCW seen from above
    pitch = math.radians(job.get("PitchDeg", 12.0))
    dist = 20.0
    z = Vector((v.x * math.cos(pitch), v.y * math.cos(pitch), math.sin(pitch))).normalized()   # camera +Z (backwards)
    x = Vector((0, 0, 1)).cross(z).normalized()
    y = z.cross(x)
    R = Matrix((x, y, z)).transposed()
    cam.matrix_world = Matrix.Translation(z * dist) @ R.to_4x4()
    return R


# ---------------------------------------------------------------------------------------------- clips

def load_clip(rig, src_root, fbx):
    new, acts = import_fbx(os.path.join(src_root, fbx))
    act = acts[0]
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)
    # strip object-level channels (the Unreal root bone became the armature object: its motion is root motion)
    for lay in act.layers:
        for st in lay.strips:
            for cb in st.channelbags:
                for fc in list(cb.fcurves):
                    if not fc.data_path.startswith("pose.bones"):
                        cb.fcurves.remove(fc)
    for top, arm, meshes in rig.parts:
        ad = arm.animation_data_create()
        ad.action = act
        ad.action_slot = act.slots[0]
    return act


def set_time(f):
    sc = bpy.context.scene
    fi = int(math.floor(f))
    sc.frame_set(fi, subframe=f - fi)


def lowest_point(rig):
    """Lowest world z of the evaluated character meshes (sampled vertices)."""
    dg = bpy.context.evaluated_depsgraph_get()
    low = 1e9
    for top, arm, meshes in rig.parts:
        for m in meshes:
            ev = m.evaluated_get(dg)
            me = ev.to_mesh()
            n = len(me.vertices)
            co = np.zeros(n * 3, np.float32)
            me.vertices.foreach_get("co", co)
            co = co.reshape(-1, 3)
            mw = np.array(m.matrix_world)
            z = co @ mw[2, :3] + mw[2, 3]
            low = min(low, float(z.min()))
            ev.to_mesh_clear()
    return low


def pelvis_world(rig, bone):
    arm = rig.arm
    pb = arm.pose.bones[bone]
    return arm.matrix_world @ pb.head


# ---------------------------------------------------------------------------------------------- passes

_CHANNELS = None


def render_passes(path):
    global _CHANNELS
    sc = bpy.context.scene
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    inp = oiio.ImageInput.open(path)
    out = {}
    names = []
    sub = 0
    while True:
        if not inp.seek_subimage(sub, 0):
            break
        spec = inp.spec()
        data = inp.read_image(sub, 0, 0, spec.nchannels, "float")
        for i, n in enumerate(spec.channelnames):
            names.append(n)
            out[n] = np.asarray(data)[:, :, i]
        sub += 1
    inp.close()
    if _CHANNELS is None:
        _CHANNELS = names
        log("EXR channels:", names)

    def ch(*cands):
        for c in cands:
            for n in out:
                if n.endswith(c):
                    return out[n]
        raise KeyError(cands)

    res = {
        "alpha": ch("Combined.A", ".A"),
        "albedo": np.stack([ch("albedo.R"), ch("albedo.G"), ch("albedo.B")], axis=2),
        "matid": ch("matid.V", "matid.X", "matid.R"),
        "boneid": ch("boneid.V", "boneid.X", "boneid.R"),
        "normal": np.stack([ch("Normal.X"), ch("Normal.Y"), ch("Normal.Z")], axis=2),
        "depth": ch("Depth.Z", "Depth.V"),
    }
    return res


def lin_to_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)


def rgb_to_hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = np.max(rgb, axis=-1)
    mn = np.min(rgb, axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    m = d > 1e-6
    rc = np.where(m, (mx - r) / np.where(m, d, 1), 0)
    gc = np.where(m, (mx - g) / np.where(m, d, 1), 0)
    bc = np.where(m, (mx - b) / np.where(m, d, 1), 0)
    h = np.where(r == mx, bc - gc, np.where(g == mx, 2.0 + rc - bc, 4.0 + gc - rc))
    h = (h / 6.0) % 1.0
    h = np.where(m, h, 0)
    s = np.where(mx > 1e-6, d / np.where(mx > 1e-6, mx, 1), 0)
    return h * 360.0, s, mx


class Classifier:
    def __init__(self, job):
        self.regions = job["Regions"]
        self.names = [r["name"] for r in self.regions]
        self.rid = {n: i + 1 for i, n in enumerate(self.names)}
        self.rules = job["Rules"]
        self.bone_names = [b[0] for b in job.get("BoneRegions", [])]
        self.ref = {}

    def classify(self, P):
        alpha = P["alpha"] > 0.5
        srgb = lin_to_srgb(P["albedo"])
        lum = 0.299 * srgb[..., 0] + 0.587 * srgb[..., 1] + 0.114 * srgb[..., 2]
        hue, sat, val = rgb_to_hsv(srgb)
        matid = np.rint(P["matid"]).astype(np.int32)
        boneid = np.floor(P["boneid"] + 0.02).astype(np.int32)
        reg = np.zeros(alpha.shape, np.int32)
        todo = alpha.copy()
        for rule in self.rules:
            m = todo.copy()
            if "material" in rule:
                mats = rule["material"] if isinstance(rule["material"], list) else [rule["material"]]
                ids = [MAT_IDS[k] for k in mats if k in MAT_IDS]
                m &= np.isin(matid, ids)
            if "bone" in rule:
                bl = rule["bone"] if isinstance(rule["bone"], list) else [rule["bone"]]
                ids = [i + 1 for i, b in enumerate(self.bone_names) if b in bl]
                m &= np.isin(boneid, ids)
            if "lum" in rule:
                m &= (lum >= rule["lum"][0]) & (lum < rule["lum"][1])
            if "sat" in rule:
                m &= (sat >= rule["sat"][0]) & (sat < rule["sat"][1])
            if "hue" in rule:
                h0, h1 = rule["hue"]
                m &= ((hue >= h0) & (hue < h1)) if h0 <= h1 else ((hue >= h0) | (hue < h1))
            reg[m] = self.rid[rule["region"]]
            todo &= ~m
        return reg, lum


def shade(P, R, job, cls, reg, lum):
    """Tone value per subpixel: lambert from a fixed view-space key light + ambient, modulated by the texture's
    luminance relative to the region's reference luminance."""
    L = Vector(job.get("Light", {}).get("dir", [-0.55, 0.65, 0.5])).normalized()
    Lw = R @ L                                           # view space -> world
    n = P["normal"]
    nl = n[..., 0] * Lw.x + n[..., 1] * Lw.y + n[..., 2] * Lw.z
    lj = job.get("Light", {})
    amb = lj.get("ambient", 0.35)
    wrap = lj.get("wrap", 0.15)
    lam = np.clip((nl + wrap) / (1 + wrap), 0, 1)
    s = amb + (1 - amb) * lam
    # half vector for specular (viewer is +Z of the camera)
    V = R @ Vector((0, 0, 1))
    Hh = (Lw + V).normalized()
    nh = np.clip(n[..., 0] * Hh.x + n[..., 1] * Hh.y + n[..., 2] * Hh.z, 0, 1)
    val = s.copy()
    spec = np.zeros_like(s)
    for i, r in enumerate(cls.regions):
        m = reg == i + 1
        if not m.any():
            continue
        aw = r.get("albedo_weight", 0.0)
        if aw:
            ref = cls.ref.get(r["name"])
            if ref is None:
                ref = float(np.median(lum[m])) if m.sum() > 50 else 0.3
                cls.ref[r["name"]] = ref
            val[m] = s[m] * np.clip(1.0 + aw * (lum[m] - ref) / max(ref, 0.05), 0.3, 1.8)
        gw = r.get("gradient_weight", 0.0)
        if gw:
            frac = np.clip(P["boneid"][m] - np.floor(P["boneid"][m] + 0.02), 0, 1) / 0.9
            val[m] = (1 - gw) * val[m] + gw * (0.3 + 0.75 * frac)
        if r.get("spec"):
            spec[m] = np.power(nh[m], r.get("spec_power", 24.0))
    return val, spec


def blur_region(reg, val, r, passes):
    """Box-blurs the tone value inside region r (same-region neighbours only): flat, calm bands on busy geometry
    such as feather fans."""
    m = reg == r
    for _ in range(passes):
        nv = px.neighbours8(np.where(m, val, 0.0), 0.0)
        nm = px.neighbours8(m.astype(np.float32), 0.0)
        s = sum(nv) + val * 2
        c = sum(nm) + 2
        val = np.where(m, s / c, val)
    return val


def make_cell(P, R, job, cls, prev):
    F = job.get("Supersample", 4)
    reg, lum = cls.classify(P)
    val, spec = shade(P, R, job, cls, reg, lum)
    nreg = len(cls.regions) + 1
    weights = np.ones(nreg, np.float32)
    min_cov = np.zeros(nreg, np.float32)
    for i, r in enumerate(cls.regions):
        weights[i + 1] = r.get("weight", 1.0)
        min_cov[i + 1] = r.get("min_cov", 0.0)
    depth = np.where(P["alpha"] > 0.5, P["depth"], 1e9)
    if DEBUG_STATS:
        hb = (np.rint(P["boneid"]) == 2) & (P["alpha"] > 0.5)
        if hb.any():
            log("head lum pct", np.percentile(lum[hb], [1, 3, 5, 10, 25, 50, 75, 95]).round(3).tolist())
        log("stats", {k: (float(np.min(v)), float(np.max(v))) for k, v in P.items() if k in ("matid", "boneid", "depth", "alpha")}, "reg", np.bincount(reg.ravel()).tolist(), "mats", MAT_IDS)
    creg, cval, cdep, cspec, cov = px.block_reduce(reg, val, depth, spec, F, nreg, weights, min_cov,
                                                   job.get("CoverageOn", 0.45))
    cl = job.get("Cleanup", {})
    for i, r in enumerate(cls.regions):
        if r.get("close"):
            creg, cval, cdep = px.close_region(creg, cval, cdep, i + 1, int(r["close"]))
    for i, r in enumerate(cls.regions):
        if r.get("open"):
            creg = px.open_region(creg, i + 1)
    creg = px.clean_mask(creg, cl.get("min_island", 4), cl.get("max_hole", 2))
    keep_ids = [i + 1 for i, r in enumerate(cls.regions) if r.get("keep_single")]
    for i, r in enumerate(cls.regions):
        if r.get("absorb"):
            creg, cval, cdep = px.absorb_into_region(creg, cval, cdep, i + 1, int(r["absorb"]), keep_ids)
    protect = [i + 1 for i, r in enumerate(cls.regions) if r.get("thin") or r.get("keep_single")]
    creg = px.clean_region_specks(creg, protect)
    for i, r in enumerate(cls.regions):
        if r.get("blur"):
            cval = blur_region(creg, cval, i + 1, int(r["blur"]))
    ramp_n = {i + 1: len(job["Ramps"][r["ramp"]]) for i, r in enumerate(cls.regions)}
    rng = {i + 1: tuple(r.get("range", [0.3, 1.0])) for i, r in enumerate(cls.regions)}
    ptone = prev["tone"] if prev is not None else None
    if ptone is not None:
        ptone = np.where(prev["reg"] == creg, ptone, -9)
    tone = px.quantize(cval, creg, ramp_n, rng, ptone, None, job.get("Hysteresis", 0.0))
    tone = px.smooth_tones(tone, creg, cl.get("tone_passes", 2))
    for i, r in enumerate(cls.regions):
        if r.get("mode_passes"):
            tone = px.mode_tones(tone, creg, i + 1, int(r["mode_passes"]))
        if r.get("min_band"):
            tone = px.merge_small_bands(tone, creg, i + 1, int(r["min_band"]))
    edge = None
    io = job.get("InnerOutline", {})
    if io.get("enabled", True):
        ok = [i + 1 for i, r in enumerate(cls.regions) if r.get("inner_outline", True)]
        tone, edge = px.inner_outline(creg, cdep, tone, io.get("depth", 0.15), ok)
    # palette index
    pal = job["_palette"]
    idx = np.zeros(creg.shape, np.int32)
    for i, r in enumerate(cls.regions):
        m = creg == i + 1
        idx[m] = pal.base[r["ramp"]] + tone[m]
        thr = r.get("spec_threshold")
        if r.get("spec") and thr:
            sm = m & (cspec > thr)
            # specular only as small glints: at most a few pixels, never on edges
            if sm.sum() <= r.get("spec_max", 6):
                idx[sm] = 2
    keep = [i + 1 for i, r in enumerate(cls.regions) if r.get("keep_single")]
    idx = px.despeckle_index(idx, creg, keep)
    idx = px.outer_outline(idx)
    idx = px.fill_pinholes(idx)
    return idx, {"reg": creg, "tone": tone}


# ---------------------------------------------------------------------------------------------- output

def write_png(path, rgba):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    h, w = rgba.shape[:2]
    out = oiio.ImageOutput.create(path)
    out.open(path, oiio.ImageSpec(w, h, rgba.shape[2], "uint8"))
    out.write_image(np.ascontiguousarray(rgba))
    out.close()


def find_ffmpeg():
    p = shutil.which("ffmpeg")
    if p:
        return p
    base = os.path.expandvars(r"%LOCALAPPDATA%\Microsoft\WinGet\Packages")
    for root, dirs, files in os.walk(base):
        if "ffmpeg.exe" in files:
            return os.path.join(root, "ffmpeg.exe")
    return None


def bake(job_id, only_clips=None, only_dirs=None, debug=False):
    t0 = time.time()
    job = json.load(open(os.path.join(JOBS, job_id + ".json")))
    src_root = os.path.join(REPO, job.get("SourceRoot", "Glyphbound/Saved/bake_src"))
    reset()
    MAT_IDS.clear()
    rig = build_character(job, src_root)
    weapon = attach_weapon(job, rig, src_root)
    cam = setup_render(job)
    pal = px.Palette(job["Ramps"], job.get("Specular", "#F0F0F0"))
    job["_palette"] = pal
    cls = Classifier(job)
    W, H = job["Cell"]
    dirs = list(range(8)) if only_dirs is None else only_dirs
    review = os.path.join(OUT_REVIEW, job["Id"])
    os.makedirs(review, exist_ok=True)
    os.makedirs(TMP, exist_ok=True)
    exr = os.path.join(TMP, "frame.exr")
    clips_out = {}
    cells = []            # list of (clip, frame, [8 idx arrays])
    row = 0
    pelvis = job.get("PelvisBone", "pelvis")
    for clip in job["Clips"]:
        if only_clips and clip["name"] not in only_clips:
            continue
        if SURVEY:
            clip = dict(clip)
            clip.pop("keys", None)
            clip.pop("range", None)
            clip["frames"] = SURVEY
            clip["loop"] = False
        act = load_clip(rig, src_root, clip["src"])
        for top, arm, meshes in rig.parts:
            for m in meshes:
                m.hide_render = False
        a0, a1 = act.frame_range
        s0, s1 = clip.get("range", [a0, a1])
        n = clip["frames"]
        if "keys" in clip:
            times = clip["keys"]
        elif clip.get("loop"):
            times = [s0 + i * (s1 - s0) / n for i in range(n)]
        else:
            times = [s0 + i * (s1 - s0) / max(n - 1, 1) for i in range(n)]
        base = [top.matrix_world.copy() for top in rig.tops] if not hasattr(rig, "base") else rig.base
        rig.base = base
        for top, b in zip(rig.tops, base):
            top.matrix_world = b
        bpy.context.view_layer.update()
        set_time(times[0])
        bpy.context.view_layer.update()
        ref = pelvis_world(rig, pelvis)
        root_mode = clip.get("root", "none")
        if root_mode == "linear":
            set_time(s0)
            bpy.context.view_layer.update()
            lin0 = pelvis_world(rig, pelvis)
            set_time(s1)
            bpy.context.view_layer.update()
            lin1 = pelvis_world(rig, pelvis)
            ref = lin0
        tilt = clip.get("tilt")
        fall = Vector(job.get("Forward", [0, -1, 0])).normalized()
        if tilt:
            tdir = tilt.get("dir", "back")
            fwd_v = fall.copy()
            left_v = Matrix.Rotation(math.radians(90), 3, "Z") @ fwd_v
            fall = {"back": -fwd_v, "front": fwd_v, "left": left_v, "right": -left_v}[tdir]
        frames = []
        prev = {d: None for d in dirs}
        for fi, t in enumerate(times):
            for top, b in zip(rig.tops, base):
                top.matrix_world = b
            bpy.context.view_layer.update()
            set_time(t)
            bpy.context.view_layer.update()
            off = Vector((0, 0, 0))
            if root_mode in ("cancel", "partial"):
                p = pelvis_world(rig, pelvis)
                k = 1.0 if root_mode == "cancel" else clip.get("root_factor", 0.5)
                off = Vector((-(p.x - ref.x) * k, -(p.y - ref.y) * k, 0))
            elif root_mode == "linear":
                u = (t - s0) / max(s1 - s0, 1e-6)
                q = lin0 + (lin1 - lin0) * u
                off = Vector((-(q.x - ref.x), -(q.y - ref.y), 0))
            off += Vector(clip.get("offset", [0, 0, 0]))
            M = Matrix.Translation(off)
            if tilt:
                ang = tilt["angles"][fi] if fi < len(tilt["angles"]) else tilt["angles"][-1]
                shs = tilt.get("shift", [0.0])
                sh = shs[fi] if fi < len(shs) else shs[-1]
                pp = fall * tilt.get("pivot", 0.15)
                axis = Vector((0, 0, 1)).cross(fall).normalized()
                M = (Matrix.Translation(off - fall * sh) @ Matrix.Translation(pp) @
                     Matrix.Rotation(math.radians(ang), 4, axis) @ Matrix.Translation(-pp))
                # the topple pivots on the ground: never let a part sink below it (lift by the lowest point)
            for top, b in zip(rig.tops, base):
                top.matrix_world = M @ b
            bpy.context.view_layer.update()
            if tilt and tilt.get("ground", True):
                low = lowest_point(rig)
                if low < 0.0 or (fi >= tilt.get("ground_from", 99)):
                    M = Matrix.Translation((0, 0, -low)) @ M
                    for top, b in zip(rig.tops, base):
                        top.matrix_world = M @ b
                    bpy.context.view_layer.update()
            # parts hidden from a sample index on (an arrow that has left the bow)
            for key, k0 in clip.get("hide", {}).items():
                for top, arm, meshes in rig.parts:
                    for m in meshes:
                        if key in m.name:
                            m.hide_render = fi >= k0
            # projectile origin: a bone's head projected into the d=0 cell at the hit frame
            if "origin_bone" in clip and fi == clip.get("hit", 0):
                R0 = place_camera(cam, job, 0)
                pb = rig.arm.pose.bones[clip["origin_bone"]]
                pw = rig.arm.matrix_world @ pb.head
                cx, cy = R0.col[0], R0.col[1]
                clip = dict(clip)
                clip["origin"] = [int(round(job["Pivot"][0] + job["PixelsPerMeter"] * pw.dot(cx))),
                                  int(round(job["Pivot"][1] - job["PixelsPerMeter"] * pw.dot(cy)))]
                log("origin", clip["name"], clip["origin"])
            per_dir = {}
            for d in dirs:
                R = place_camera(cam, job, d)
                P = render_passes(exr)
                idx, st = make_cell(P, R, job, cls, prev[d] if job.get("Hysteresis", 0) > 0 and clip.get("loop") else None)
                prev[d] = st
                per_dir[d] = idx
                if debug and fi == 0 and d in (0, 2):
                    alb = lin_to_srgb(P["albedo"])
                    rgba = np.concatenate([alb, (P["alpha"] > 0.5)[..., None].astype(np.float32)], axis=2)
                    write_png(os.path.join(review, "debug_%s_d%d.png" % (clip["name"], d)),
                              (np.flipud(rgba) * 255).astype(np.uint8) if False else (rgba * 255).astype(np.uint8))
            frames.append(per_dir)
        if SURVEY:
            K2 = 2
            strip = np.tile(np.array([128, 128, 128, 255], np.uint8), (H * K2 * len(dirs) + 20, W * K2 * len(frames), 1))
            for i, per_dir in enumerate(frames):
                for j, d in enumerate(dirs):
                    c = px.scale_nearest(pal.table[per_dir[d]], K2)
                    y, x = 20 + j * H * K2, i * W * K2
                    strip[y:y + H * K2, x:x + W * K2] = np.where(c[..., 3:4] > 0, c, strip[y:y + H * K2, x:x + W * K2])
                px.draw_text(strip, i * W * K2 + 4, 4, "%d:%.0f" % (i, times[i]), [255, 255, 0, 255], 2)
            write_png(os.path.join(review, "survey_%s.png" % os.path.basename(clip["src"])[:-4]), strip)
        entry = {"Row": row, "Frames": len(frames), "Fps": clip.get("fps", 8), "Loop": bool(clip.get("loop", False))}
        if "hit" in clip:
            entry["HitFrame"] = clip["hit"]
        clips_out[clip["name"]] = entry
        cells.append((clip, frames, entry))
        row += len(frames)
        log("clip", clip["name"], "frames", len(frames), "t=%.1fs" % (time.time() - t0))
    # sheet
    sheet = np.zeros((row * H, 8 * W), np.int32)
    for clip, frames, entry in cells:
        for i, per_dir in enumerate(frames):
            for d, idx in per_dir.items():
                y = (entry["Row"] + i) * H
                sheet[y:y + H, d * W:(d + 1) * W] = idx
    used = np.unique(sheet)
    rgba = pal.table[sheet]
    ncol = len([u for u in used if u != 0])
    full = only_clips is None and only_dirs is None
    # origins (projectile exits) are given in the job in cell pixels of the d=0 column
    for clip, frames, entry in cells:
        if "origin" in clip:
            entry["Origin"] = clip["origin"]
    meta = {"Id": job["Id"], "CellWidth": W, "CellHeight": H, "PivotX": job["Pivot"][0], "PivotY": job["Pivot"][1],
            "PixelsPerMeter": job["PixelsPerMeter"], "Columns": 8, "Clips": clips_out, "Source": job.get("SourceText", "")}
    if full:
        write_png(os.path.join(OUT_DATA, job["Id"] + ".png"), rgba)
        with open(os.path.join(OUT_DATA, job["Id"] + ".json"), "w", newline="\n") as f:
            json.dump(meta, f, indent=2)
            f.write("\n")
    else:
        write_png(os.path.join(review, "partial_sheet.png"), rgba)
    # self-check numbers (the image reader shrinks big images: count the defects instead of eyeballing them)
    ramp_of = np.full(len(pal.colors), -1, np.int32)
    ramp_of[0] = -9
    for k, (name, b0) in enumerate(pal.base.items()):
        ramp_of[b0:b0 + pal.size[name]] = k
    lines = []
    for clip, frames, entry in cells:
        for d in dirs:
            m = px.metrics([f[d] for f in frames], entry["Loop"], ramp_of)
            lines.append("%-8s d%d %s" % (clip["name"], d, json.dumps(m)))
    png_path = os.path.join(OUT_DATA, job["Id"] + ".png") if full else os.path.join(review, "partial_sheet.png")
    inp = oiio.ImageInput.open(png_path)
    back = np.asarray(inp.read_image(0, 0, 0, 4, "uint8"))
    inp.close()
    alphas = sorted(set(np.unique(back[..., 3]).tolist()))
    opaque = back[back[..., 3] > 0][:, :3]
    cols = {tuple(c) for c in np.unique(opaque, axis=0).tolist()} if len(opaque) else set()
    palset = {tuple(c[:3]) for c in pal.colors[1:]}
    lines.append("PNG %s: %dx%d alpha values %s, colours %d, outside palette %d" % (
        os.path.basename(png_path), back.shape[1], back.shape[0], alphas, len(cols), len(cols - palset)))
    os.makedirs(review, exist_ok=True)
    with open(os.path.join(review, "metrics.txt" if full else "metrics_partial.txt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    for l in lines:
        if '"isolated": 0, "holes": 0, "border": 0' not in l or l.startswith("PNG"):
            log("CHECK", l)
    # review: contact sheet (4x, mid grey, labelled), one per clip too, and GIFs at d=0 and d=2
    K = 4
    grey = np.array([128, 128, 128, 255], np.uint8)
    blocks = []
    for clip, frames, entry in cells:
        lab_h = 24
        blk = np.tile(grey, (lab_h + len(frames) * H * K, 8 * W * K, 1))
        px.draw_text(blk, 8, 6, "%s  %d FR  %d FPS" % (clip["name"], len(frames), entry["Fps"]), [255, 255, 0, 255], 3)
        for i, per_dir in enumerate(frames):
            for d, idx in per_dir.items():
                c = pal.table[idx]
                big = px.scale_nearest(c, K)
                y = lab_h + i * H * K
                x = d * W * K
                a = big[..., 3:4] > 0
                blk[y:y + H * K, x:x + W * K] = np.where(a, big, blk[y:y + H * K, x:x + W * K])
            if i == 0:
                for d in range(8):
                    px.draw_text(blk, d * W * K + 6, lab_h + 4, "D%d" % d, [255, 255, 255, 255], 3)
        # pivot marks
        for d in range(8):
            for i in range(len(frames)):
                y = lab_h + i * H * K + job["Pivot"][1] * K
                x = d * W * K + job["Pivot"][0] * K
                blk[y:y + 2, x - 6:x + 6] = [255, 0, 255, 255]
        write_png(os.path.join(review, "contact_%s.png" % clip["name"]), blk)
        blocks.append(blk)
    if blocks:
        write_png(os.path.join(review, "contact.png" if full else "contact_partial.png"), np.concatenate(blocks, axis=0))
    ff = find_ffmpeg()
    if ff:
        for clip, frames, entry in cells:
            for d in (0, 2):
                if d not in dirs:
                    continue
                gdir = os.path.join(TMP, "gif")
                shutil.rmtree(gdir, ignore_errors=True)
                os.makedirs(gdir)
                for i, per_dir in enumerate(frames):
                    c = px.scale_nearest(pal.table[per_dir[d]], K)
                    bg = np.tile(grey, (H * K, W * K, 1))
                    img = np.where(c[..., 3:4] > 0, c, bg)
                    write_png(os.path.join(gdir, "f%03d.png" % i), img)
                gif = os.path.join(review, "%s_d%d.gif" % (clip["name"], d))
                subprocess.run([ff, "-y", "-loglevel", "error", "-framerate", str(entry["Fps"]), "-i",
                                os.path.join(gdir, "f%03d.png"), "-vf",
                                "split[a][b];[a]palettegen=reserve_transparent=0:stats_mode=full[p];[b][p]paletteuse=dither=none",
                                "-loop", "0", gif], check=False)
    log("DONE", job["Id"], "rows", row, "colours", ncol, "time %.1fs" % (time.time() - t0))
    return ncol


def main():
    if not ARGS:
        print("usage: -- <SheetId|All> [--clips A,B] [--dirs 0,2] [--debug]")
        return
    target = ARGS[0]
    clips = opt("--clips")
    dirs = opt("--dirs")
    clips = clips.split(",") if isinstance(clips, str) else None
    dirs = [int(x) for x in dirs.split(",")] if isinstance(dirs, str) else None
    debug = bool(opt("--debug", False))
    order = ["OrcWarrior", "ValkyriePixel", "Barghest", "CentaurArcher", "OrcShaman", "OrcBrute"]
    ids = [j for j in order if os.path.exists(os.path.join(JOBS, j + ".json"))] if target == "All" else [target]
    for j in ids:
        bake(j, clips, dirs, debug)


main()
