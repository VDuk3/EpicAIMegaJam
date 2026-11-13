# Builds the assault rifle's one source .blend from the owner's Tripo GLB (carabina-roja.zip, model.glb).
# The GLB is a 5.5k-triangle low-poly export with one 4096 albedo that carries baked light, smears and soft edges, and
# no roughness, metallic, normal or AO. Shown as it comes it reads soft ("made of jelly", owner). This script rebuilds
# the look on the same shape:
#   geometry  vertices welded (the GLB is split into ~1000 loose panels), Tripo's lumpy custom normals dropped, hard
#             edges above SHARP_ANGLE, face-area weighted normals so the large flat panels shade flat
#   BaseColor one flat tone per panel (the panel's median brightness snapped to a small palette), plus the small drawn
#             details (holes, panel lines) re-cut with crisp edges; baked light and smears are gone
#   ORM       R = ambient occlusion baked in Cycles, G = roughness and B = metallic by tone (white and grey polymer,
#             dark metal)
#   Normal    baked in Cycles (OpenGL, +Y): rounded edges (Bevel node) and the drawn details pressed in (Bump)
#   Emissive  the GLB's neon mask, edges re-cut
#   top rail  the GLB's rail teeth have uneven lengths and gaps (7 to 34 mm, no rhythm; owner: "the pattern breaks").
#             Its panels are deleted and a new rail is modelled here: same section and length, RAIL_TEETH equal teeth,
#             real chamfers, its own flat dark-metal material slot (Rifle_Rail)
# Objects saved: Rifle_Body (slots carabina_roja, Rifle_Rail), Rifle_Trigger, Rifle_Lens (source frame: muzzle -Y,
# up +Z, metres).
# Usage: blender -b --factory-startup --python rifle_build_blend.py -- <model.glb> <out.blend> [review_dir]
import bpy, bmesh, sys, os, math
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
glb, out_blend = argv[0], argv[1]
review_dir = argv[2] if len(argv) > 2 else None

SHARP_ANGLE = 30.0
BEVEL_RADIUS = 0.0009      # metres: rounded-edge width baked into the normal map
DETAIL_DEPTH = 0.0005      # metres: how deep the drawn holes and panel lines are pressed in
AO_DISTANCE = 0.06
BAKE_SIZE = 4096
PAD = 24                   # texels of padding around every UV island

# Tone palette: (upper limit of the panel's median brightness, albedo sRGB, roughness, metallic)
TONES = [
    (0.24, (0.210, 0.220, 0.235), 0.40, 0.65),   # dark metal: barrel, lower handguard
    (0.42, (0.300, 0.305, 0.315), 0.72, 0.0),    # mid grey: grip, stock pad, sight housing (rubbery polymer)
    (0.62, (0.520, 0.525, 0.535), 0.62, 0.0),    # light grey polymer
    (9.99, (0.790, 0.800, 0.815), 0.55, 0.0),    # white polymer shell
]
DARK_DETAIL = ((0.060, 0.062, 0.066), 0.80, 0.0)
LIGHT_DETAIL = ((0.600, 0.605, 0.615), 0.60, 0.0)
NEON_BASE = ((0.030, 0.030, 0.030), 0.30, 0.0)
RAIL_METAL = ((0.210, 0.220, 0.235), 0.40, 0.65)   # the new rail's flat material (Blender review; Unreal: MI_Rifle_Rail)
# The top rail (source frame, metres). Section taken from the GLB's rail: underside, deck, tooth foot, tooth top.
RAIL_BOX = ((-0.0245, -0.312, 0.105), (0.0135, 0.064, 0.125))   # panels fully inside are the old rail
RAIL_Y = (-0.3088, 0.0623)
RAIL_BASE = [(-0.0203, 0.1093), (0.0095, 0.1093), (0.0120, 0.1170), (-0.0227, 0.1170)]     # (x, z)
RAIL_TOOTH = [(-0.0217, 0.1165), (0.0110, 0.1165), (0.0056, 0.1218), (-0.0164, 0.1218)]
RAIL_TEETH = 23
RAIL_TOOTH_LEN = 0.0100
RAIL_CHAMFER = 0.0004

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
scene = bpy.context.scene
SRC = [o for o in scene.objects if o.type == "MESH"][0]
me = SRC.data
color_im = next(i for i in bpy.data.images if i.name.startswith("Color_"))
neon_im = bpy.data.images["neon_mask"]


def load(im):
    w, h = im.size
    a = np.empty(w * h * 4, np.float32)
    im.pixels.foreach_get(a)
    return a.reshape(h, w, 4)


def new_image(name, rgb, srgb):
    h, w = rgb.shape[:2]
    im = bpy.data.images.get(name) or bpy.data.images.new(name, w, h, alpha=False)
    im.colorspace_settings.name = "sRGB" if srgb else "Non-Color"
    a = np.ones((h, w, 4), np.float32)
    a[..., :3] = rgb if rgb.ndim == 3 else rgb[..., None]
    im.pixels.foreach_set(np.ascontiguousarray(a).ravel())
    im.update()
    im.pack()
    return im


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def box_blur(a, r):
    p = np.pad(a, ((r + 1, r), (r + 1, r)), mode="edge").astype(np.float64)
    s = p.cumsum(0).cumsum(1)
    k = 2 * r + 1
    return ((s[k:, k:] - s[:-k, k:] - s[k:, :-k] + s[:-k, :-k]) / (k * k)).astype(np.float32)


# ---------------------------------------------------------------- panels (loose parts of the raw mesh)
bm = bmesh.new()
bm.from_mesh(me)
bm.faces.ensure_lookup_table()
face_panel = np.full(len(bm.faces), -1, np.int32)
panels = []
for f in bm.faces:
    if face_panel[f.index] >= 0:
        continue
    pid = len(panels)
    stack, comp = [f], []
    face_panel[f.index] = pid
    while stack:
        a = stack.pop()
        comp.append(a.index)
        for e in a.edges:
            for b in e.link_faces:
                if face_panel[b.index] < 0:
                    face_panel[b.index] = pid
                    stack.append(b)
    panels.append(comp)


def inside(vs, lo, hi):
    return all(lo[i] <= min(v.co[i] for v in vs) and max(v.co[i] for v in vs) <= hi[i] for i in range(3))


# Moving and glowing parts, picked by the bounding box of their panels (source frame): 1 trigger, 2 sight lenses,
# 3 the old top rail (deleted).
face_part = np.zeros(len(bm.faces), np.int32)
for comp in panels:
    vs = {v for i in comp for v in bm.faces[i].verts}
    if inside(vs, (-0.0105, 0.116, -0.043), (-0.0005, 0.134, 0.0035)):
        face_part[comp] = 1
    elif len(comp) >= 15 and inside(vs, (-0.042, 0.104, 0.105), (0.031, 0.170, 0.140)) \
            and max(v.co.x for v in vs) - min(v.co.x for v in vs) < 0.006:
        face_part[comp] = 2
    elif inside(vs, *RAIL_BOX):
        face_part[comp] = 3
bm.free()
print("PANELS", len(panels), "trigger faces", int((face_part == 1).sum()), "lens faces", int((face_part == 2).sum()),
      "old rail faces", int((face_part == 3).sum()))
if not (face_part == 1).any() or not (face_part == 2).any():
    raise SystemExit("rifle_build_blend: trigger or lens panels not found")

# ---------------------------------------------------------------- textures
col = load(color_im)
neon_raw = load(neon_im)[..., 0]
H, W = col.shape[:2]
lum = col[..., 0] * 0.2126 + col[..., 1] * 0.7152 + col[..., 2] * 0.0722

n_loops = len(me.loops)
uv = np.empty(n_loops * 2, np.float32)
me.uv_layers[0].uv.foreach_get("vector", uv)
uv = uv.reshape(n_loops, 2)
loop_start = np.empty(len(me.polygons), np.int32)
me.polygons.foreach_get("loop_start", loop_start)

ids = np.full((H, W), -1, np.int32)
hits = np.zeros((H, W), np.uint8)
for f in range(len(me.polygons)):
    p = uv[loop_start[f]:loop_start[f] + 3] * (W, H)
    x0, x1 = max(int(math.floor(p[:, 0].min() - 1)), 0), min(int(math.ceil(p[:, 0].max() + 1)), W)
    y0, y1 = max(int(math.floor(p[:, 1].min() - 1)), 0), min(int(math.ceil(p[:, 1].max() + 1)), H)
    if x1 <= x0 or y1 <= y0:
        continue
    d = (p[1, 0] - p[0, 0]) * (p[2, 1] - p[0, 1]) - (p[2, 0] - p[0, 0]) * (p[1, 1] - p[0, 1])
    if abs(d) < 1e-9:
        continue
    px = (np.arange(x0, x1) + 0.5)[None, :]
    py = (np.arange(y0, y1) + 0.5)[:, None]
    w1 = ((px - p[0, 0]) * (p[2, 1] - p[0, 1]) - (p[2, 0] - p[0, 0]) * (py - p[0, 1])) / d
    w2 = ((p[1, 0] - p[0, 0]) * (py - p[0, 1]) - (px - p[0, 0]) * (p[1, 1] - p[0, 1])) / d
    m = (w1 >= 0) & (w2 >= 0) & (w1 + w2 <= 1)
    sub = ids[y0:y1, x0:x1]
    hits[y0:y1, x0:x1][m & (sub >= 0) & (sub != face_panel[f])] = 1
    sub[m] = face_panel[f]
covered = ids >= 0
print("ATLAS covered %.1f%%, texels claimed by two panels %.2f%%" % (100 * covered.mean(), 100 * hits.mean()))

neon = neon_raw > 0.5
valid = covered & ~neon
flat_ids, flat_l = ids[valid], lum[valid]
order = np.argsort(flat_ids, kind="stable")
sid, sl = flat_ids[order], flat_l[order]
cuts = np.searchsorted(sid, np.arange(len(panels) + 1))
med = np.full(len(panels), np.nan, np.float32)
area = np.zeros(len(panels), np.int64)
for i in range(len(panels)):
    seg = sl[cuts[i]:cuts[i + 1]]
    area[i] = len(seg)
    if len(seg):
        med[i] = np.median(seg)
# Panels too small to own a texel: brightness at the UV centre of their first face.
for i in np.where(np.isnan(med))[0]:
    c = uv[loop_start[panels[i][0]]:loop_start[panels[i][0]] + 3].mean(0)
    med[i] = lum[min(int(c[1] * H), H - 1), min(int(c[0] * W), W - 1)]
hist, edges = np.histogram(med, bins=20, range=(0, 1), weights=area)
print("MEDIAN HISTOGRAM (share of texels):", " ".join("%.2f:%.1f%%" % (edges[i], 100 * hist[i] / hist.sum()) for i in range(20)))

tone_of = np.array([next(k for k, t in enumerate(TONES) if m < t[0]) for m in med], np.int32)

# Padding: grow every island PAD texels into the empty atlas so filtering and mips never pull in a neighbour.
for _ in range(PAD):
    empty = ids < 0
    if not empty.any():
        break
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        shifted = np.roll(ids, (dy, dx), axis=(0, 1))
        take = empty & (shifted >= 0)
        ids[take] = shifted[take]
        empty &= ~take
filled = ids >= 0
safe = np.where(filled, ids, 0)
pmed = np.where(filled, med[safe], 0.5)
ptone = np.where(filled, tone_of[safe], len(TONES) - 1)

# Small drawn details, re-cut with crisp edges. Dark ones (holes, panel lines) on panels that are not dark; light
# ones (markings) on dark and mid panels.
ratio = lum / np.maximum(pmed, 0.05)
# On mid-grey panels only strong contrast counts: weaker marks there are bake smears, not design.
is_light = pmed >= 0.45
dark = smoothstep(np.where(is_light, 0.70, 0.55), np.where(is_light, 0.50, 0.38), ratio) * (pmed >= 0.24)
light = smoothstep(0.22, 0.36, lum - pmed) * (pmed < 0.42)
neon_a = smoothstep(0.35, 0.65, box_blur(neon_raw, 1))

tone_rgb = np.array([t[1] for t in TONES], np.float32)
tone_rough = np.array([t[2] for t in TONES], np.float32)
tone_metal = np.array([t[3] for t in TONES], np.float32)


def layer(base, value, alpha):
    return base + (np.asarray(value, np.float32) - base) * (alpha[..., None] if base.ndim == 3 else alpha)


base = tone_rgb[ptone]
rough = tone_rough[ptone]
metal = tone_metal[ptone]
for (rgb, r, m), a in ((DARK_DETAIL, dark), (LIGHT_DETAIL, light), (NEON_BASE, neon_a)):
    base = layer(base, rgb, a)
    rough = layer(rough, r, a)
    metal = layer(metal, m, a)
height = 1.0 - box_blur(np.maximum(dark, neon_a * 0.6), 1)

im_base = new_image("T_Rifle_BaseColor", base, True)
im_height = new_image("Rifle_Height", height, False)
im_mask = new_image("T_Rifle_EmissiveMask", np.repeat(neon_a[..., None], 3, 2), False)

# ---------------------------------------------------------------- geometry
for o in scene.objects:
    o.select_set(False)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
attr = me.attributes.new("part", "INT", "FACE")
attr.data.foreach_set("value", face_part)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.remove_doubles(threshold=1e-5)
bpy.ops.mesh.select_all(action="DESELECT")
bpy.ops.object.mode_set(mode="OBJECT")
values = np.empty(len(me.polygons), np.int32)
me.attributes["part"].data.foreach_get("value", values)
me.polygons.foreach_set("select", (values == 3).tolist())
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.delete(type="FACE")
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.mesh.customdata_custom_splitnormals_clear()
me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))
mod = SRC.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
mod.mode = "FACE_AREA"
mod.weight = 50
mod.keep_sharp = True
bpy.ops.object.modifier_apply(modifier=mod.name)
bm = bmesh.new()
bm.from_mesh(me)
print("WELDED verts", len(bm.verts), "tris", len(bm.faces), "open edges", sum(1 for e in bm.edges if len(e.link_faces) == 1),
      "edges with >2 faces", sum(1 for e in bm.edges if len(e.link_faces) > 2),
      "sharp edges", sum(1 for e in bm.edges if not e.smooth), "of", len(bm.edges))
bm.free()


# ---------------------------------------------------------------- the new top rail
def build_rail():
    rb = bmesh.new()

    def prism(section, y0, y1):
        a = [rb.verts.new((x, y0, z)) for x, z in section]
        b = [rb.verts.new((x, y1, z)) for x, z in section]
        n = len(section)
        for i in range(n):
            rb.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
        rb.faces.new(a)
        rb.faces.new(list(reversed(b)))

    prism(RAIL_BASE, RAIL_Y[0], RAIL_Y[1])
    gap = (RAIL_Y[1] - RAIL_Y[0] - RAIL_TEETH * RAIL_TOOTH_LEN) / (RAIL_TEETH - 1)
    for k in range(RAIL_TEETH):
        y = RAIL_Y[0] + k * (RAIL_TOOTH_LEN + gap)
        prism(RAIL_TOOTH, y, y + RAIL_TOOTH_LEN)
    bmesh.ops.recalc_face_normals(rb, faces=rb.faces[:])
    bmesh.ops.bevel(rb, geom=rb.edges[:], offset=RAIL_CHAMFER, offset_type="OFFSET", segments=2, profile=0.5,
                    affect="EDGES", clamp_overlap=True)
    bmesh.ops.triangulate(rb, faces=rb.faces[:])
    layer_uv = rb.loops.layers.uv.new(me.uv_layers[0].name)
    for f in rb.faces:
        f.smooth = True
        for loop in f.loops:
            loop[layer_uv].uv = (0.5, 0.5)
    rail_me = bpy.data.meshes.new("Rifle_Rail")
    rb.to_mesh(rail_me)
    rb.free()
    rail_mat = bpy.data.materials.new("Rifle_Rail")
    rail_mat.use_nodes = True
    rb_bsdf = next(n for n in rail_mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    rb_bsdf.inputs["Base Color"].default_value = tuple(c ** 2.2 for c in RAIL_METAL[0]) + (1.0,)
    rb_bsdf.inputs["Roughness"].default_value = RAIL_METAL[1]
    rb_bsdf.inputs["Metallic"].default_value = RAIL_METAL[2]
    rail_me.materials.append(rail_mat)
    rail = bpy.data.objects.new("Rifle_Rail", rail_me)
    scene.collection.objects.link(rail)
    SRC.select_set(False)
    rail.select_set(True)
    bpy.context.view_layer.objects.active = rail
    wn = rail.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
    wn.mode = "FACE_AREA"
    wn.weight = 50
    wn.keep_sharp = True
    bpy.ops.object.modifier_apply(modifier=wn.name)
    rail.select_set(False)
    SRC.select_set(True)
    bpy.context.view_layer.objects.active = SRC
    print("RAIL teeth", RAIL_TEETH, "tooth %.1f mm, gap %.1f mm, tris %d" % (RAIL_TOOTH_LEN * 1000, gap * 1000, len(rail_me.polygons)))
    return rail


RAIL = build_rail()   # in the scene before the bake, so it shades what is around it

# ---------------------------------------------------------------- bake: normal (rounded edges + details) and AO
scene.render.engine = "CYCLES"
prefs = bpy.context.preferences.addons["cycles"].preferences
for kind in ("OPTIX", "CUDA"):
    try:
        prefs.compute_device_type = kind
        prefs.get_devices()
        for dev in prefs.devices:
            dev.use = True
        scene.cycles.device = "GPU"
        break
    except Exception as e:
        print("no", kind, e)
world = bpy.data.worlds.new("BakeWorld")
world.light_settings.distance = AO_DISTANCE
scene.world = world

mat = me.materials[0]
mat.name = "carabina_roja"
nt = mat.node_tree
nt.nodes.clear()
out = nt.nodes.new("ShaderNodeOutputMaterial")
bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])


def tex(image, x, y):
    n = nt.nodes.new("ShaderNodeTexImage")
    n.image = image
    n.location = (x, y)
    return n


t_height = tex(im_height, -900, -300)
bump = nt.nodes.new("ShaderNodeBump")
bump.inputs["Strength"].default_value = 1.0
bump.inputs["Distance"].default_value = DETAIL_DEPTH
nt.links.new(t_height.outputs["Color"], bump.inputs["Height"])
bevel = nt.nodes.new("ShaderNodeBevel")
bevel.samples = 12
bevel.inputs["Radius"].default_value = BEVEL_RADIUS
nt.links.new(bump.outputs["Normal"], bevel.inputs["Normal"])
nt.links.new(bevel.outputs["Normal"], bsdf.inputs["Normal"])


def bake(kind, name, samples, **kwargs):
    image = bpy.data.images.new(name, BAKE_SIZE, BAKE_SIZE, alpha=False)
    image.colorspace_settings.name = "Non-Color"
    node = tex(image, -600, 300)
    for n in nt.nodes:
        n.select = False
    node.select = True
    nt.nodes.active = node
    scene.cycles.samples = samples
    scene.render.bake.margin = PAD
    scene.render.bake.margin_type = "EXTEND"
    bpy.ops.object.bake(type=kind, use_clear=True, **kwargs)
    nt.nodes.remove(node)
    print("BAKED", name)
    return image


im_normal = bake("NORMAL", "T_Rifle_Normal", 24, normal_space="TANGENT")
im_ao = bake("AO", "Rifle_AO", 96)
im_normal.pack()

ao = load(im_ao)[..., 0]
ao = np.clip(ao, 0.0, 1.0) * (1.0 - 0.45 * dark)
im_orm = new_image("T_Rifle_ORM", np.stack([ao, rough, metal], axis=2), False)
bpy.data.images.remove(im_ao)

# ---------------------------------------------------------------- final material (Blender review; Unreal builds its own)
nt.nodes.remove(bump)
nt.nodes.remove(bevel)
nt.nodes.remove(t_height)
t_base, t_orm, t_nrm, t_mask = tex(im_base, -900, 400), tex(im_orm, -900, 100), tex(im_normal, -900, -200), tex(im_mask, -900, -500)
sep = nt.nodes.new("ShaderNodeSeparateColor")
nt.links.new(t_orm.outputs["Color"], sep.inputs["Color"])
mix = nt.nodes.new("ShaderNodeMix")
mix.data_type = "RGBA"
mix.blend_type = "MULTIPLY"
mix.inputs["Factor"].default_value = 1.0
nt.links.new(t_base.outputs["Color"], mix.inputs["A"])
nt.links.new(sep.outputs["Red"], mix.inputs["B"])
nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
nt.links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
nt.links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
nmap = nt.nodes.new("ShaderNodeNormalMap")
nt.links.new(t_nrm.outputs["Color"], nmap.inputs["Color"])
nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
glow = nt.nodes.new("ShaderNodeMix")
glow.data_type = "RGBA"
glow.blend_type = "MULTIPLY"
glow.inputs["Factor"].default_value = 1.0
glow.inputs["B"].default_value = (0.05, 0.9, 0.6, 1.0)     # the rifle's own mint
nt.links.new(t_mask.outputs["Color"], glow.inputs["A"])
nt.links.new(glow.outputs["Result"], bsdf.inputs["Emission Color"])
bsdf.inputs["Emission Strength"].default_value = 4.0
bpy.data.images.remove(im_height)
for im in (color_im, neon_im):
    bpy.data.images.remove(im)

# ---------------------------------------------------------------- split the parts
bpy.ops.object.mode_set(mode="EDIT")
for part, name in ((1, "Rifle_Trigger"), (2, "Rifle_Lens")):
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    values = np.empty(len(me.polygons), np.int32)
    me.attributes["part"].data.foreach_get("value", values)
    me.polygons.foreach_set("select", (values == part).tolist())
    bpy.ops.object.mode_set(mode="EDIT")
    before = set(scene.objects)
    bpy.ops.mesh.separate(type="SELECTED")
    new = next(o for o in scene.objects if o not in before)
    new.name = name
    new.data.name = name
bpy.ops.object.mode_set(mode="OBJECT")
for o in scene.objects:
    o.select_set(False)
RAIL.select_set(True)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
bpy.ops.object.join()
SRC.name = "Rifle_Body"
me.name = "Rifle_Body"
for o in scene.objects:
    if o.type == "MESH":
        print("OBJECT", o.name, "tris", len(o.data.polygons), "custom normals", o.data.has_custom_normals)

if review_dir:
    os.makedirs(review_dir, exist_ok=True)
    for im in (im_base, im_orm, im_normal, im_mask):
        c = im.copy()
        c.scale(2048, 2048)
        c.filepath_raw = os.path.join(review_dir, im.name + ".png")
        c.file_format = "PNG"
        c.save()
        bpy.data.images.remove(c)
bpy.ops.wm.save_as_mainfile(filepath=out_blend, compress=True)
print("SAVED", out_blend)
