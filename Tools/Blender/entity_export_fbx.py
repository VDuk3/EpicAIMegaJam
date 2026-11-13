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
TENTACLE_RATIO = 0.3                          # decimate: the hologram does not need the panel detail
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


def set_sway_uv(ob):
    """UV 1 'Sway' on the tentacles: U = 0 at the shell, 1 at the farthest tip; V = random phase per tentacle."""
    me = ob.data
    b = bmesh.new()
    b.from_mesh(me)
    b.faces.ensure_lookup_table()
    layer = b.loops.layers.uv.get("Sway") or b.loops.layers.uv.new("Sway")
    rmax = max(shell_r(v.co) for v in b.verts)
    rng = random.Random(26)
    count = 0
    comps = islands(b, set(b.faces))
    print("TENTACLE islands after decimate:", sorted(len(c) for c in comps))
    for comp in comps:
        phase = rng.random()
        count += 1
        for i in comp:
            for loop in b.faces[i].loops:
                w = min(1.0, max(0.0, (shell_r(loop.vert.co) - 1.0) / (rmax - 1.0)))
                loop[layer].uv = (w, phase)
    b.to_mesh(me)
    b.free()
    return count, rmax


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
tentacles, rmax = set_sway_uv(parts[2])
report["tentacle_islands"] = tentacles
report["source_tris"] = source_tris
report["tentacle_tris_before_decimate"] = tent_before
for ob in parts:
    ob.data.transform(EXPORT)
    ob.data.update()
    report["parts"][ob.name] = info(ob)
    print("PART", ob.name, json.dumps(report["parts"][ob.name]))
    export(ob)


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
