# Exports the Entity (Glyphbound/SourceArt/Enemies/Entity/Entity.blend, one fused Tripo mesh, variant #26) as three
# static-mesh FBX files for Unreal, plus its textures. The look is split by part (owner, 2026-11-12):
#   SM_Entity_Body       the shell and the central machinery: metal. Slot "Entity_Body".
#   SM_Entity_Eye        the big central eye (its ring, iris and pupil): the weak point. Slot "Entity_Eye".
#   SM_Entity_Tentacles  every tentacle, simplified (decimated): a hologram. Slot "Entity_Holo".
#                        UV 1 ("Sway"): U = 0 at the root, 1 at the tip (how much the tip sways);
#                        V = a random phase per tentacle, so they do not move in step.
# How the fused mesh is split: a face is tentacle if its centre is outside a tilted ellipsoid fitted to the shell
# (picked by eye on coloured renders); then every UV chart that is at least CHART_SHARE tentacle becomes tentacle whole
# (this carries each tentacle's root into the shell); small islands of either side are given to the other. The eye is the body faces whose UVs
# fall in the eye's two islands of the albedo atlas.
# Source frame: eye towards -Y, up +Z, metres, ~1 m tall. Export frame: turned +90 deg around Z so the eye looks to +X
# (Unreal forward), scaled by SCALE, origin at the centre of the shell. Same FBX settings as the guns.
# Textures written: T_Entity_BaseColor, T_Entity_Normal (OpenGL, Unreal flips green), T_Entity_GlowMask (the blue
# lights of the albedo, the eyes and the dots along the tentacles).
# Usage: blender -b Entity.blend --python entity_export_fbx.py -- <out_dir>
import bpy, bmesh, sys, os, json, math, random
import numpy as np
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
os.makedirs(os.path.join(out_dir, "Export"), exist_ok=True)
os.makedirs(os.path.join(out_dir, "Textures"), exist_ok=True)

scene = bpy.context.scene
SRC = [o for o in scene.objects if o.type == "MESH"][0]
SCALE = 3.0                                    # provisional: about 2.9 m tall, the shell about 1.5 m across
BODY_C = Vector((0.0, 0.05, 0.70))             # centre of the shell (source frame), also the export origin
BODY_R = Vector((0.25, 0.33, 0.27))            # ellipsoid radii
BODY_TILT = math.radians(25.0)                 # the shell leans forward: its long axis rises towards the back
TILT = Matrix.Rotation(-BODY_TILT, 3, "X")
CHART_SHARE = 0.3                              # a UV chart this much outside the shell is tentacle, whole
MIN_DECIMATED = 10                             # decimation leaves a few 2-triangle crumbs; islands under this are deleted
MIN_ISLAND = 150                             # faces; smaller islands of one side go to the other
EYE_UV = [((0.374, 0.702), 0.052), ((0.137, 0.121), 0.026)]   # (centre, radius) in UV space: ring + iris, pupil
EYE_FRONT, EYE_RADIUS = 0.2, 0.09             # eye faces: in front of the shell centre by this much, within this radius
BONES_PER_CHAIN = 8                            # rig: bones along each tentacle
ROOT_BAND = 0.05                               # rig: vertices this close to a tentacle's nearest point to the shell are its root
DEBUG_BLEND = "--debug-blend" in argv          # also save the scene (rig included) as entity_rig_debug.blend in out_dir
TENTACLE_RATIO = 0.3                         # decimate: the hologram does not need the panel detail
TEX_SIZE = 2048
GLOW_BLUE, GLOW_SOFT = 0.15, 0.1               # albedo blue - max(red, green) above this glows (soft ramp)
TURN = Matrix.Rotation(math.pi / 2, 4, "Z")
EXPORT = Matrix.Scale(SCALE, 4) @ TURN @ Matrix.Translation(-BODY_C)


def shell_r(co):
    d = TILT @ (co - BODY_C)
    return math.sqrt((d.x / BODY_R.x) ** 2 + (d.y / BODY_R.y) ** 2 + (d.z / BODY_R.z) ** 2)


def islands(bm, faces):
    """Face islands (connected by edges) inside the set faces."""
    seen, out = set(), []
    for f in faces:
        if f.index in seen:
            continue
        stack, comp = [f], []
        seen.add(f.index)
        while stack:
            a = stack.pop()
            comp.append(a.index)
            for e in a.edges:
                for b in e.link_faces:
                    if b.index not in seen and b in faces:
                        seen.add(b.index)
                        stack.append(b)
        out.append(comp)
    return out


def uv_charts(bm, uv):
    """Face islands connected by edges whose two faces share the same UVs (no seam)."""
    def joined(e):
        if len(e.link_faces) != 2:
            return False
        f1, f2 = e.link_faces
        for v in e.verts:
            u1 = next(l[uv].uv for l in f1.loops if l.vert == v)
            u2 = next(l[uv].uv for l in f2.loops if l.vert == v)
            if (u1 - u2).length > 1e-4:
                return False
        return True
    seen, out = set(), []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, comp = [f], []
        seen.add(f.index)
        while stack:
            a = stack.pop()
            comp.append(a.index)
            for e in a.edges:
                if joined(e):
                    for b in e.link_faces:
                        if b.index not in seen:
                            seen.add(b.index)
                            stack.append(b)
        out.append(comp)
    return out


# ---- classify the faces ----
bm = bmesh.new()
bm.from_mesh(SRC.data)
bm.faces.ensure_lookup_table()
uv = bm.loops.layers.uv.active
tent = {f for f in bm.faces if shell_r(f.calc_center_median()) >= 1.0}
# The ellipsoid alone left the first ~5% of each tentacle on the body (owner, 2026-11-12). Tripo's atlas cuts the mesh
# into UV charts that follow the parts, so a chart that is mostly tentacle goes to the tentacles whole, its root
# inside the shell included.
charts = uv_charts(bm, uv)
for chart in charts:
    if sum(bm.faces[i] in tent for i in chart) >= CHART_SHARE * len(chart):
        tent.update(bm.faces[i] for i in chart)
body = set(bm.faces) - tent
moved = 0
for comp in islands(bm, tent):
    if len(comp) < MIN_ISLAND:
        body.update(bm.faces[i] for i in comp); moved += len(comp)
tent = set(bm.faces) - body
for comp in sorted(islands(bm, body), key=len)[:-1]:
    if len(comp) < MIN_ISLAND:
        tent.update(bm.faces[i] for i in comp); moved += len(comp)
body = set(bm.faces) - tent


def in_eye(f):
    u = sum(l[uv].uv.x for l in f.loops) / len(f.loops)
    v = sum(l[uv].uv.y for l in f.loops) / len(f.loops)
    return any((u - c[0]) ** 2 + (v - c[1]) ** 2 < r * r for c, r in EYE_UV)


uv_hits = [f for f in body if in_eye(f)]
# The UV circles also catch bits of other atlas islands: keep only the hits at the front of the shell, around the eye.
front = [f.calc_center_median() for f in uv_hits if f.calc_center_median().y < BODY_C.y - EYE_FRONT]
if not front:
    raise SystemExit("entity_export_fbx: no eye faces at the front")
EYE_C = sum(front, Vector()) / len(front)
eye = {f for f in uv_hits if (f.calc_center_median() - EYE_C).length < EYE_RADIUS}
print("EYE centre", tuple(round(c, 3) for c in EYE_C), "uv hits", len(uv_hits), "kept", len(eye))
body -= eye
BODY_IDX = {f.index for f in body}
EYE_IDX = {f.index for f in eye}
TENT_IDX = {f.index for f in tent}
bm.free()
print("SPLIT body %d eye %d tentacles %d (islands moved %d faces)" % (len(BODY_IDX), len(EYE_IDX), len(TENT_IDX), moved))
if len(EYE_IDX) < 20 or len(BODY_IDX) < 2000 or len(TENT_IDX) < 2000:
    raise SystemExit("entity_export_fbx: the split looks wrong")


# ---- materials (slot names only; Unreal builds the real ones) ----
def slot(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def build(name, keep, material):
    b = bmesh.new()
    b.from_mesh(SRC.data)
    b.faces.ensure_lookup_table()
    bmesh.ops.delete(b, geom=[f for f in b.faces if f.index not in keep], context="FACES")
    for f in b.faces:
        f.material_index = 0
    me = bpy.data.meshes.new(name)
    b.to_mesh(me)
    b.free()
    me.materials.append(slot(material))
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    return ob


def decimate(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    mod = ob.modifiers.new("Decimate", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = TENTACLE_RATIO
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    b = bmesh.new()
    b.from_mesh(ob.data)
    b.faces.ensure_lookup_table()
    crumbs = [b.faces[i] for c in islands(b, set(b.faces)) if len(c) < MIN_DECIMATED for i in c]
    bmesh.ops.delete(b, geom=crumbs, context="FACES")
    b.to_mesh(ob.data)
    b.free()
    print("DECIMATE removed %d crumb faces" % len(crumbs))
    for p in ob.data.polygons:
        p.use_smooth = True                    # the hologram's rim light needs smooth normals


VERT_T, VERT_CHAIN = {}, {}       # per tentacle vertex: distance along its tentacle (0 root .. 1 tip), its tentacle


def geodesic(b, comp):
    """Distance from the root over the surface (Dijkstra on edges), normalized to 0..1 for one tentacle. The root is
    the vertices nearest the shell. Curled tentacles are why this is not the distance from the shell."""
    import heapq
    verts = {v for i in comp for v in b.faces[i].verts}
    rmin = min(shell_r(v.co) for v in verts)
    dist = {v: (0.0 if shell_r(v.co) <= rmin + ROOT_BAND else float("inf")) for v in verts}
    heap = [(0.0, v.index, v) for v in verts if dist[v] == 0.0]
    heapq.heapify(heap)
    while heap:
        d, _, v = heapq.heappop(heap)
        if d > dist[v]:
            continue
        for e in v.link_edges:
            w = e.other_vert(v)
            if w in dist:
                nd = d + e.calc_length()
                if nd < dist[w]:
                    dist[w] = nd
                    heapq.heappush(heap, (nd, w.index, w))
    dmax = max(dist.values())
    return {v.index: dist[v] / dmax for v in verts}, dmax


def set_sway_uv(ob):
    """UV 1 'Sway' on the tentacles: U = distance along the tentacle over the surface (0 root, 1 tip); V = random
    phase per tentacle. Also fills VERT_T / VERT_CHAIN for the rig."""
    me = ob.data
    b = bmesh.new()
    b.from_mesh(me)
    b.faces.ensure_lookup_table()
    layer = b.loops.layers.uv.get("Sway") or b.loops.layers.uv.new("Sway")
    rng = random.Random(26)
    comps = sorted(islands(b, set(b.faces)), key=lambda c: min(f for f in c))   # stable tentacle order
    print("TENTACLE islands after decimate:", sorted(len(c) for c in comps))
    lengths = []
    for chain, comp in enumerate(comps):
        phase = rng.random()
        t, dmax = geodesic(b, comp)
        lengths.append(dmax)
        for i, tv in t.items():
            VERT_T[i] = tv
            VERT_CHAIN[i] = chain
        for i in comp:
            for loop in b.faces[i].loops:
                loop[layer].uv = (t[loop.vert.index], phase)
    b.to_mesh(me)
    b.free()
    return len(comps), lengths


def build_rig(mesh_ob):
    """Armature for the tentacles, in the export frame: a 'root' bone at the origin (centre of the shell) and a chain
    of BONES_PER_CHAIN bones per tentacle along its centre line (centroids of the vertices at each distance along
    it). Each vertex is weighted to the two bones nearest its distance along the tentacle. Returns the armature and a
    skinned copy of the mesh named SK_Entity_Tentacles."""
    me = mesh_ob.data
    n = BONES_PER_CHAIN
    chains = sorted(set(VERT_CHAIN.values()))
    joints = {}
    for c in chains:
        vs = [v for v in me.vertices if VERT_CHAIN.get(v.index) == c]
        pts = []
        for j in range(n + 1):
            lo, hi = (j - 0.5) / n, (j + 0.5) / n
            band = [v.co for v in vs if lo <= VERT_T[v.index] < hi or (j == n and VERT_T[v.index] >= hi)]
            pts.append(sum(band, Vector()) / len(band) if band else None)
        for j in range(n + 1):                      # fill empty bands by interpolation
            if pts[j] is None:
                a = max(k for k in range(j) if pts[k] is not None) if any(pts[k] is not None for k in range(j)) else None
                bnext = next((k for k in range(j + 1, n + 1) if pts[k] is not None), None)
                pts[j] = pts[a] if bnext is None else pts[bnext] if a is None else \
                    pts[a].lerp(pts[bnext], (j - a) / (bnext - a))
        joints[c] = pts
    arm_data = bpy.data.armatures.new("Armature")
    arm = bpy.data.objects.new("Armature", arm_data)          # "Armature": Unreal does not add it as an extra bone
    scene.collection.objects.link(arm)
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm_data.edit_bones.new("root")
    root.head, root.tail = Vector((0, 0, 0)), Vector((0, 0, 0.1))
    for c in chains:
        parent = root
        for i in range(n):
            eb = arm_data.edit_bones.new("T%02d_%02d" % (c, i))
            eb.head, eb.tail = joints[c][i], joints[c][i + 1]
            if (eb.tail - eb.head).length < 1e-4:
                eb.tail = eb.head + Vector((0, 0, 0.01))
            eb.parent = parent
            eb.use_connect = i > 0
            parent = eb
    bpy.ops.object.mode_set(mode="OBJECT")
    sk = mesh_ob.copy()
    sk.data = me.copy()
    sk.name = "SK_Entity_Tentacles"
    scene.collection.objects.link(sk)
    groups = {}
    for c in chains:
        for i in range(n):
            groups[(c, i)] = sk.vertex_groups.new(name="T%02d_%02d" % (c, i))
    for v in sk.data.vertices:
        c, t = VERT_CHAIN[v.index], VERT_T[v.index]
        f = min(max(t * n - 0.5, 0.0), n - 1.0)       # position between bone centres
        i0 = int(math.floor(f))
        i1 = min(i0 + 1, n - 1)
        w1 = f - i0
        groups[(c, i0)].add([v.index], 1.0 - w1, "REPLACE")
        if i1 != i0 and w1 > 0.0:
            groups[(c, i1)].add([v.index], w1, "REPLACE")
    sk.parent = arm
    mod = sk.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    rig = {"bones_per_chain": n, "chains": len(chains), "bone_names": "T<chain 2 digits>_<bone 2 digits>, parent root",
           "chain_roots_cm": [[round(100 * joints[c][0].x, 1), round(-100 * joints[c][0].y, 1),
                               round(100 * joints[c][0].z, 1)] for c in chains],
           "chain_lengths_cm": [round(100 * sum((joints[c][i + 1] - joints[c][i]).length for i in range(n)), 1)
                                for c in chains]}
    print("RIG", json.dumps(rig))
    return arm, sk, rig


def export_skeletal(arm, sk):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    arm.select_set(True)
    sk.select_set(True)
    bpy.context.view_layer.objects.active = arm
    path = os.path.join(out_dir, "Export", sk.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE",
                             use_mesh_modifiers=False, add_leaf_bones=False, use_armature_deform_only=True,
                             primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
                             bake_anim=False, use_custom_props=False, path_mode="STRIP",
                             axis_forward="-Z", axis_up="Y")
    print("WROTE", path)


def info(ob):
    me = ob.data
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
    return {"tris": sum(len(p.vertices) - 2 for p in me.polygons), "verts": len(me.vertices),
            "min_cm": [round(100 * min(a), 1) for a in (xs, ys, zs)],
            "max_cm": [round(100 * max(a), 1) for a in (xs, ys, zs)],
            "uv_maps": [l.name for l in me.uv_layers]}


def export(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(out_dir, "Export", ob.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", use_mesh_modifiers=False,
                             add_leaf_bones=False, bake_anim=False, use_custom_props=False, path_mode="STRIP",
                             axis_forward="-Z", axis_up="Y")
    print("WROTE", path)


report = {"scale": SCALE, "origin": "centre of the shell", "forward": "+X (the eye)", "parts": {},
          # The weak point for the game code (Unreal cm, actor frame): a sphere around the eye.
          "eye_centre_cm": [round(100 * c, 1) for c in (EXPORT @ EYE_C)],
          "eye_radius_cm": round(100 * SCALE * EYE_RADIUS, 1)}
parts = [build("SM_Entity_Body", BODY_IDX, "Entity_Body"), build("SM_Entity_Eye", EYE_IDX, "Entity_Eye"),
         build("SM_Entity_Tentacles", TENT_IDX, "Entity_Holo")]
source_tris = sum(len(p.vertices) - 2 for p in SRC.data.polygons)
tent_before = sum(len(p.vertices) - 2 for p in parts[2].data.polygons)
decimate(parts[2])
tentacles, lengths = set_sway_uv(parts[2])
report["tentacle_islands"] = tentacles
report["source_tris"] = source_tris
report["tentacle_tris_before_decimate"] = tent_before
for ob in parts:
    ob.data.transform(EXPORT)
    ob.data.update()
    report["parts"][ob.name] = info(ob)
    print("PART", ob.name, json.dumps(report["parts"][ob.name]))
    export(ob)
arm, sk, report["rig"] = build_rig(parts[2])
export_skeletal(arm, sk)
if DEBUG_BLEND:
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "entity_rig_debug.blend"), copy=True)


# ---- textures ----
def save(img_name, pixels, size, path):
    im = bpy.data.images.new(img_name, size, size, alpha=False)
    im.pixels.foreach_set(pixels)
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()


images = {im.name.rsplit("_", 1)[-1]: im for im in bpy.data.images if im.size[0] > 0}
albedo = images["basecolor"].copy()
albedo.scale(TEX_SIZE, TEX_SIZE)
px = np.empty(TEX_SIZE * TEX_SIZE * 4, dtype=np.float32)
albedo.pixels.foreach_get(px)
albedo.filepath_raw = os.path.join(out_dir, "Textures", "T_Entity_BaseColor.png")
albedo.file_format = "PNG"
albedo.save()
normal = images["normal"].copy()
if normal.size[0] != TEX_SIZE:
    normal.scale(TEX_SIZE, TEX_SIZE)
normal.filepath_raw = os.path.join(out_dir, "Textures", "T_Entity_Normal.png")
normal.file_format = "PNG"
normal.save()
p = px.reshape(-1, 4)
blue = p[:, 2] - np.maximum(p[:, 0], p[:, 1])
mask = np.clip((blue - GLOW_BLUE) / GLOW_SOFT, 0, 1) * np.clip((p[:, 2] - 0.3) / 0.2, 0, 1)
save("glow", np.stack([mask, mask, mask, np.ones_like(mask)], 1).ravel(), TEX_SIZE,
     os.path.join(out_dir, "Textures", "T_Entity_GlowMask.png"))
report["glow_coverage"] = round(float((mask > 0.5).mean()), 4)
with open(os.path.join(out_dir, "export_report.json"), "w") as fh:
    json.dump(report, fh, indent=2)
print("REPORT", json.dumps(report))
