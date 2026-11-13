# Builds the assault rifle's one source .blend from the owner's Tripo GLB (carabina-roja.zip, model.glb).
# The GLB is a 5.5k-triangle low-poly export with one 4096 albedo that carries baked light, smears and soft edges, and
# no roughness, metallic, normal or AO. Shown as it comes it reads soft ("made of jelly", owner). This script rebuilds
# the look on the same shape:
#   geometry  vertices welded (the GLB is split into ~1000 loose panels), Tripo's lumpy custom normals dropped, hard
#             edges above SHARP_ANGLE, face-area weighted normals so the large flat panels shade flat
#   BaseColor one flat tone per panel (the panel's median brightness snapped to a small palette), plus the small drawn
#             details (holes, panel lines) re-cut with crisp edges; baked light and smears are gone. Whole parts that
#             came out patchy (barrel, front sight, eyepiece, the band beside the rail) are forced to one tone
#   ORM       R = ambient occlusion baked in Cycles, G = roughness and B = metallic by tone (white and grey polymer,
#             dark metal)
#   Normal    baked in Cycles (OpenGL, +Y): rounded edges (Bevel node) and the drawn details pressed in (Bump)
#   Emissive  the GLB's neon mask, edges re-cut; the ragged side strip of the handguard is removed from it
# Modelled here, because the GLB's version was broken or missing:
#   top rail      the GLB's teeth have uneven lengths and gaps (7 to 34 mm; owner: "the pattern breaks"): deleted, and
#                 a rail with RAIL_TEETH equal teeth takes its place
#   energy bars   one on each side of the handguard, on its upper sloped face: a dark bezel with a straight glowing
#                 strip (owner: the glowing shape "is hard to read" from the player's view). The game cuts the strip
#                 into 13 cells that drain with the enchant
#   ammo screen   a dark tilted housing on the rear of the sight's eyepiece, and the hologram pane in it
#   magazine      the GLB has none: a curved box magazine, base plate and a glowing slit on each side
# Objects saved (source frame: muzzle -Y, up +Z, metres): Rifle_Body (slots carabina_roja, Rifle_Rail), Rifle_Trigger,
# Rifle_Lens, Rifle_AmmoDisplay, Rifle_Magazine. Pivots and motion are custom properties, read by rifle_export_fbx.py.
# Usage: blender -b --factory-startup --python rifle_build_blend.py -- <model.glb> <out.blend> [review_dir]
import bpy, bmesh, sys, os, math
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
glb, out_blend = argv[0], argv[1]
review_dir = argv[2] if len(argv) > 2 else None

SHARP_ANGLE = 30.0
BEVEL_RADIUS = 0.0009      # metres: rounded-edge width baked into the normal map
DETAIL_DEPTH = 0.0005      # metres: how deep the drawn holes and panel lines are pressed in
AO_DISTANCE = 0.06
BAKE_SIZE = 4096
PAD = 24                   # texels of padding around every UV island
CX = -0.0055               # the model's centre line in X (its left and right panels mirror around it)

# Tone palette: (upper limit of the panel's median brightness, albedo sRGB, roughness, metallic)
TONES = [
    (0.24, (0.210, 0.220, 0.235), 0.40, 0.65),   # 0 dark metal: barrel, lower handguard
    (0.42, (0.300, 0.305, 0.315), 0.72, 0.0),    # 1 mid grey: grip, stock pad, sight housing (rubbery polymer)
    (0.62, (0.520, 0.525, 0.535), 0.62, 0.0),    # 2 light grey polymer
    (9.99, (0.790, 0.800, 0.815), 0.55, 0.0),    # 3 white polymer shell
]
DARK_DETAIL = ((0.060, 0.062, 0.066), 0.80, 0.0)
LIGHT_DETAIL = ((0.600, 0.605, 0.615), 0.60, 0.0)
NEON_BASE = ((0.030, 0.030, 0.030), 0.30, 0.0)
METAL = ((0.210, 0.220, 0.235), 0.40, 0.65)   # flat material of the modelled metal parts (Unreal: MI_Rifle_Rail)

# Whole welded pieces forced to one tone, without drawn details: their panels came out as patches of different
# tones (owner: "patches of other colours" at the muzzle end). (box lo, box hi, tone index); a piece fully inside.
PIECE_TONES = [
    ((-0.030, -0.510, 0.040), (0.020, -0.340, 0.090), 0),   # barrel and muzzle device
    ((-0.045, -0.395, 0.078), (0.035, -0.305, 0.150), 0),   # front sight block
    ((-0.025, 0.170, 0.100), (0.015, 0.215, 0.145), 0),     # sight eyepiece (behind it sits the ammo screen)
]
# Boxes in the source frame, applied per texel of the atlas (each texel knows its place on the gun).
# Dark metal, no details: the band beside the top rail (the ledge at z = 98.9 mm, the wall and the top under the
# rail). Its panels reach beyond it, so it cannot be picked by panel.
DARK_BOXES = [((CX - 0.0296, -0.335, 0.09865), (CX + 0.0296, 0.066, 0.1185))]
# No drawn details (blurred marks that read as stains from the player's view): the handguard's upper sloped face in
# front of the side window (a dark teardrop on each side), a squiggle near the nose, a smear on the stock's front.
NO_DETAIL_BOXES = [
    ((-0.070, -0.375, 0.0635), (0.060, -0.278, 0.106)),
    ((-0.070, -0.356, 0.053), (0.060, -0.336, 0.068)),
    ((0.010, 0.225, 0.055), (0.035, 0.255, 0.085)),
]
# The GLB's side neon (a ragged strip, a dark window and a flap) is removed in front of this Y, below this Z.
NEON_KILL = (-0.090, 0.100)

# The top rail (source frame, metres). Section taken from the GLB's rail: underside, deck, tooth foot, tooth top.
RAIL_BOX = ((-0.0245, -0.312, 0.105), (0.0135, 0.064, 0.125))   # panels fully inside are the old rail
RAIL_Y = (-0.3088, 0.0623)
RAIL_BASE = [(-0.0203, 0.1093), (0.0095, 0.1093), (0.0120, 0.1170), (-0.0227, 0.1170)]     # (x, z)
RAIL_TOOTH = [(-0.0217, 0.1165), (0.0110, 0.1165), (0.0056, 0.1218), (-0.0164, 0.1218)]
RAIL_TEETH = 23
RAIL_TOOTH_LEN = 0.0100
CHAMFER = 0.0004

# The energy bars. The handguard's upper side face is a plane leaning 26.6 deg inwards: at height z its distance from
# the centre line is BAR_X0 - 0.5 * (z - BAR_Z0). The bar lies on it, centred at BAR_ZC.
BAR_X0, BAR_Z0, BAR_ZC = 0.046, 0.065, 0.0725
BAR_Y = (-0.338, -0.100)
BAR_HALF_W, BAR_PROUD, BAR_DEEP = 0.0070, 0.0025, 0.0060
STRIP_HALF_W, STRIP_INSET, STRIP_LIFT = 0.0033, 0.0040, 0.0002
STRIP_Y = (BAR_Y[0] + STRIP_INSET, BAR_Y[1] - STRIP_INSET)
STRIP_ZMAX = 0.1
STRIP_CELLS = 13           # as in M_RifleAtlas (Unreal); here only for the review material
CELL_GAP = 0.12

# The ammo screen: a 2:1 hologram pane tilted SCREEN_TILT deg up towards the player's eye, in a dark housing on the
# rear face of the eyepiece.
SCREEN_C = Vector((CX, 0.2140, 0.1275))
SCREEN_TILT = 14.0
SCREEN_HALF = (0.0160, 0.0080)
SCREEN_FRAME = 0.0022      # housing rim around the pane
SCREEN_DEEP = 0.0130       # housing depth behind the pane (it sinks into the eyepiece)

# The magazine.
MAG_PIVOT = Vector((CX, 0.032, -0.028))
MAG_OUT = Vector((0.0, -0.02, -0.17))   # slide-out vector on reload
MAG_HALF_W = 0.0125
MAG_TOP_Z, MAG_BOTTOM_Z = -0.028, -0.163
MAG_TOP_Y = (0.006, 0.050)          # back and front faces at the top (front = smaller Y, towards the muzzle)
MAG_FORWARD = 0.04                  # how far the bottom curves towards the muzzle
MAG_RINGS = 8
TRIGGER_PIVOT = Vector((CX, 0.125, 0.003))
HAND = Vector((CX, 0.19, 0.0))      # top of the pistol grip: the body's origin in Unreal

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
scene = bpy.context.scene
SRC = [o for o in scene.objects if o.type == "MESH"][0]
me = SRC.data
color_im = next(i for i in bpy.data.images if i.name.startswith("Color_"))
neon_im = bpy.data.images["neon_mask"]
UV_NAME = me.uv_layers[0].name


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


def components(b):
    """Connected face groups of a bmesh, as lists of face indices."""
    b.faces.ensure_lookup_table()
    b.faces.index_update()
    owner = np.full(len(b.faces), -1, np.int32)
    out = []
    for f in b.faces:
        if owner[f.index] >= 0:
            continue
        cid = len(out)
        stack, comp = [f], []
        owner[f.index] = cid
        while stack:
            a = stack.pop()
            comp.append(a.index)
            for e in a.edges:
                for o in e.link_faces:
                    if owner[o.index] < 0:
                        owner[o.index] = cid
                        stack.append(o)
        out.append(comp)
    return out, owner


def inside(vs, lo, hi):
    return all(lo[i] <= min(v.co[i] for v in vs) and max(v.co[i] for v in vs) <= hi[i] for i in range(3))


# ---------------------------------------------------------------- panels (loose parts of the raw mesh) and pieces
bm = bmesh.new()
bm.from_mesh(me)
panels, face_panel = components(bm)
n_faces = len(bm.faces)

# Moving and glowing parts, picked by the bounding box of their panels (source frame): 1 trigger, 2 sight lenses,
# 3 the old top rail (deleted).
face_part = np.zeros(n_faces, np.int32)
force_tone = np.full(len(panels), -1, np.int32)
for pid, comp in enumerate(panels):
    vs = {v for i in comp for v in bm.faces[i].verts}
    if inside(vs, (-0.0105, 0.116, -0.043), (-0.0005, 0.134, 0.0035)):
        face_part[comp] = 1
    elif len(comp) >= 15 and inside(vs, (-0.042, 0.104, 0.105), (0.031, 0.170, 0.140)) \
            and max(v.co.x for v in vs) - min(v.co.x for v in vs) < 0.006:
        face_part[comp] = 2
    elif inside(vs, *RAIL_BOX):
        face_part[comp] = 3
print("PANELS", len(panels), "trigger faces", int((face_part == 1).sum()), "lens faces", int((face_part == 2).sum()),
      "old rail faces", int((face_part == 3).sum()))
if not (face_part == 1).any() or not (face_part == 2).any():
    raise SystemExit("rifle_build_blend: trigger or lens panels not found")

# Welded pieces (the mesh as an artist would see it: barrel, front sight, shell...), to force whole parts to one tone.
welded = bm.copy()
bmesh.ops.remove_doubles(welded, verts=welded.verts[:], dist=1e-5)
if len(welded.faces) != n_faces:
    raise SystemExit("rifle_build_blend: welding removed faces, piece tones would be misassigned")
pieces, _ = components(welded)
for comp in pieces:
    vs = {v for i in comp for v in welded.faces[i].verts}
    for lo, hi, tone in PIECE_TONES:
        if inside(vs, lo, hi):
            force_tone[np.unique(face_panel[comp])] = tone
            print("PIECE forced to tone", tone, "tris", len(comp))
welded.free()
bm.free()


# ---------------------------------------------------------------- textures
col = load(color_im)
neon_src = load(neon_im)[..., 0]
H, W = col.shape[:2]
lum = col[..., 0] * 0.2126 + col[..., 1] * 0.7152 + col[..., 2] * 0.0722

n_loops = len(me.loops)
uv = np.empty(n_loops * 2, np.float32)
me.uv_layers[0].uv.foreach_get("vector", uv)
uv = uv.reshape(n_loops, 2)
loop_start = np.empty(len(me.polygons), np.int32)
me.polygons.foreach_get("loop_start", loop_start)
vert_co = np.empty(len(me.vertices) * 3, np.float32)
me.vertices.foreach_get("co", vert_co)
loop_vert = np.empty(n_loops, np.int32)
me.loops.foreach_get("vertex_index", loop_vert)
loop_co = vert_co.reshape(-1, 3)[loop_vert]

ids = np.full((H, W), -1, np.int32)
pos = np.zeros((H, W, 3), np.float32)      # where on the gun each texel is (source frame)
for f in range(len(me.polygons)):
    if face_part[f] == 3:
        continue                    # the old rail is deleted: its share of the atlas is free
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
    ids[y0:y1, x0:x1][m] = face_panel[f]
    c = loop_co[loop_start[f]:loop_start[f] + 3]
    pos[y0:y1, x0:x1][m] = (c[0] + w1[..., None] * (c[1] - c[0]) + w2[..., None] * (c[2] - c[0]))[m]
covered = ids >= 0


def in_box(lo, hi):
    return covered & np.all((pos >= np.asarray(lo, np.float32)) & (pos <= np.asarray(hi, np.float32)), axis=2)


# Per-texel flags: 1 = the old side neon goes, 2 = no drawn details, 4 = dark metal.
flags = np.zeros((H, W), np.uint8)
flags[covered & (pos[..., 1] < NEON_KILL[0]) & (pos[..., 2] < NEON_KILL[1])] |= 1
for lo, hi in NO_DETAIL_BOXES:
    flags[in_box(lo, hi)] |= 2
for lo, hi in DARK_BOXES:
    flags[in_box(lo, hi)] |= 6
del pos
print("ATLAS covered %.1f%%" % (100 * covered.mean()))

valid = covered & ~(neon_src > 0.5)
flat_ids, flat_l = ids[valid], lum[valid]
order = np.argsort(flat_ids, kind="stable")
sid, sl = flat_ids[order], flat_l[order]
cuts = np.searchsorted(sid, np.arange(len(panels) + 1))
med = np.full(len(panels), np.nan, np.float32)
for i in range(len(panels)):
    seg = sl[cuts[i]:cuts[i + 1]]
    if len(seg):
        med[i] = np.median(seg)
# Panels too small to own a texel: brightness at the UV centre of their first face.
for i in np.where(np.isnan(med))[0]:
    c = uv[loop_start[panels[i][0]]:loop_start[panels[i][0]] + 3].mean(0)
    med[i] = lum[min(int(c[1] * H), H - 1), min(int(c[0] * W), W - 1)]

tone_of = np.array([next(k for k, t in enumerate(TONES) if m < t[0]) for m in med], np.int32)
forced = force_tone >= 0
tone_of[forced] = force_tone[forced]
print("TONES forced on", int(forced.sum()), "panels")

# Padding: grow every island PAD texels into the empty atlas so filtering and mips never pull in a neighbour.
for _ in range(PAD):
    empty = ids < 0
    if not empty.any():
        break
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        shifted = np.roll(ids, (dy, dx), axis=(0, 1))
        take = empty & (shifted >= 0)
        ids[take] = shifted[take]
        flags[take] = np.roll(flags, (dy, dx), axis=(0, 1))[take]
        empty &= ~take
filled = ids >= 0
safe = np.where(filled, ids, 0)
pmed = np.where(filled, med[safe], 0.5)
ptone = np.where(filled, tone_of[safe], len(TONES) - 1)
ptone[(flags & 4) > 0] = 0
plain = (filled & forced[safe]) | ((flags & 2) > 0)          # no drawn details here
gone = (flags & 1) > 0                                       # the old side neon
neon_raw = np.where(gone, 0.0, neon_src)
plain |= gone & (box_blur(neon_src, 5) > 0.02)               # and no ghost of it (the albedo under it is red)

# Small drawn details, re-cut with crisp edges. Dark ones (holes, panel lines) on panels that are not dark; light
# ones (markings) on dark and mid panels. On mid-grey panels only strong contrast counts: weaker marks there are
# bake smears, not design.
ratio = lum / np.maximum(pmed, 0.05)
is_light = pmed >= 0.45
dark = smoothstep(np.where(is_light, 0.70, 0.55), np.where(is_light, 0.50, 0.38), ratio) * (pmed >= 0.24) * ~plain
light = smoothstep(0.22, 0.36, lum - pmed) * (pmed < 0.42) * ~plain
neon_a = smoothstep(0.35, 0.65, box_blur(neon_raw, 1))

# A free block of the atlas for the modelled glowing strips: neon on, flat everywhere else.
for reach in (40, 28, 20, 14):
    free = box_blur((~filled).astype(np.float32), reach) > 0.9999
    free[:48, :] = free[-48:, :] = False
    free[:, :48] = free[:, -48:] = False
    if free.any():
        break
else:
    raise SystemExit("rifle_build_blend: ERROR no free block in the atlas for the energy strip")
fy, fx = np.argwhere(free)[0]
half = reach - 8
BLOCK = (slice(fy - half, fy + half + 1), slice(fx - half, fx + half + 1))
STRIP_UV = ((fx + 0.5) / W, (fy + 0.5) / H, half / 3.0 / W)      # centre u, v and half size of the strips' UV square
neon_a[BLOCK] = 1.0
dark[BLOCK] = 0.0
light[BLOCK] = 0.0
print("STRIP block at texel", int(fx), int(fy), "half size", half)

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
height[BLOCK] = 1.0

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


def weighted_normals(ob):
    for o in scene.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    mod = ob.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
    mod.mode = "FACE_AREA"
    mod.weight = 50
    mod.keep_sharp = True
    bpy.ops.object.modifier_apply(modifier=mod.name)


weighted_normals(SRC)
bm = bmesh.new()
bm.from_mesh(me)
print("WELDED verts", len(bm.verts), "tris", len(bm.faces), "open edges", sum(1 for e in bm.edges if len(e.link_faces) == 1),
      "sharp edges", sum(1 for e in bm.edges if not e.smooth), "of", len(bm.edges))
bm.free()

mat = me.materials[0]
mat.name = "carabina_roja"


def flat_material(name, rgb_srgb, roughness, metallic, glow=None, alpha=1.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = tuple(c ** 2.2 for c in rgb_srgb) + (1.0,)
    b.inputs["Roughness"].default_value = roughness
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Alpha"].default_value = alpha
    if glow is not None:
        b.inputs["Emission Color"].default_value = tuple(glow[:3]) + (1.0,)
        b.inputs["Emission Strength"].default_value = glow[3]
    return m


MINT = (0.05, 0.9, 0.6)
metal_mat = flat_material("Rifle_Rail", *METAL)


# ---------------------------------------------------------------- modelled parts
def prism(b, section, y0, y1, mat_index):
    """A closed bar along Y with the (x, z) polygon as its section."""
    a = [b.verts.new((x, y0, z)) for x, z in section]
    c = [b.verts.new((x, y1, z)) for x, z in section]
    n = len(section)
    faces = [b.faces.new((a[i], a[(i + 1) % n], c[(i + 1) % n], c[i])) for i in range(n)]
    faces += [b.faces.new(a), b.faces.new(list(reversed(c)))]
    for f in faces:
        f.material_index = mat_index
    return faces


def box(b, origin, ax, ay, az, lo, hi, mat_index):
    """A closed box in the frame (ax, ay, az) at origin, from lo to hi along each axis."""
    vs = [b.verts.new(origin + ax * x + ay * y + az * z)
          for z in (lo[2], hi[2]) for y in (lo[1], hi[1]) for x in (lo[0], hi[0])]
    faces = [b.faces.new([vs[i] for i in q]) for q in
             ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3))]
    for f in faces:
        f.material_index = mat_index
    return faces


def quad(b, origin, ax, ay, az, lo, hi, lift, mat_index, uv_rect):
    """A quad in the (ax, ay) plane, lifted along az; uv_rect = (u0, v0, u1, v1) from lo to hi."""
    layer_uv = b.loops.layers.uv.verify()
    pts = [(lo[0], lo[1]), (hi[0], lo[1]), (hi[0], hi[1]), (lo[0], hi[1])]
    f = b.faces.new([b.verts.new(origin + ax * x + ay * y + az * lift) for x, y in pts])
    f.normal_update()
    if f.normal.dot(az) < 0:
        f.normal_flip()
    f.material_index = mat_index
    for loop in f.loops:
        local = loop.vert.co - origin
        s = (local.dot(ax) - lo[0]) / (hi[0] - lo[0])
        t = (local.dot(ay) - lo[1]) / (hi[1] - lo[1])
        loop[layer_uv].uv = (uv_rect[0] + (uv_rect[2] - uv_rect[0]) * s, uv_rect[1] + (uv_rect[3] - uv_rect[1]) * t)
    return f


def finish(b, name, materials, solids, chamfer=True, smooth=True):
    """bmesh -> object. solids: faces of the closed boxes and prisms, whose edges get a real chamfer."""
    layer_uv = b.loops.layers.uv.verify()
    bmesh.ops.recalc_face_normals(b, faces=solids)
    if chamfer:
        edges = list({e for f in solids for e in f.edges})
        bmesh.ops.bevel(b, geom=edges, offset=CHAMFER, offset_type="OFFSET", segments=2, profile=0.5,
                        affect="EDGES", clamp_overlap=True)
    bmesh.ops.triangulate(b, faces=b.faces[:])
    for f in b.faces:
        f.smooth = smooth
    new_me = bpy.data.meshes.new(name)
    b.to_mesh(new_me)
    b.free()
    new_me.uv_layers[0].name = UV_NAME
    for m in materials:
        new_me.materials.append(m)
    ob = bpy.data.objects.new(name, new_me)
    scene.collection.objects.link(ob)
    if smooth:
        weighted_normals(ob)
    else:
        new_me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))
    print("MODELLED", name, "tris", len(new_me.polygons))
    return ob


def build_extras():
    """Rail, energy bars and the ammo screen's housing: one object, joined into the body after the bakes.
    Slot 0 is the atlas material (only the glowing strips use it), slot 1 the flat metal."""
    b = bmesh.new()
    layer_uv = b.loops.layers.uv.verify()
    solids = []
    # top rail
    solids += prism(b, RAIL_BASE, RAIL_Y[0], RAIL_Y[1], 1)
    gap = (RAIL_Y[1] - RAIL_Y[0] - RAIL_TEETH * RAIL_TOOTH_LEN) / (RAIL_TEETH - 1)
    for k in range(RAIL_TEETH):
        y = RAIL_Y[0] + k * (RAIL_TOOTH_LEN + gap)
        solids += prism(b, RAIL_TOOTH, y, y + RAIL_TOOTH_LEN, 1)
    print("RAIL teeth", RAIL_TEETH, "tooth %.1f mm, gap %.1f mm" % (RAIL_TOOTH_LEN * 1000, gap * 1000))
    # energy bars: frame = along the gun (Y), up the sloped face, out of it
    u, v, h = STRIP_UV
    for side in (1, -1):
        up = Vector((-0.5 * side, 0.0, 1.0)).normalized()
        out = Vector((side, 0.0, 0.5)).normalized()
        along = Vector((0.0, 1.0, 0.0))
        origin = Vector((CX + side * (BAR_X0 - 0.5 * (BAR_ZC - BAR_Z0)), 0.0, BAR_ZC))
        solids += box(b, origin, along, up, out, (BAR_Y[0], -BAR_HALF_W, -BAR_DEEP), (BAR_Y[1], BAR_HALF_W, BAR_PROUD), 1)
        quad(b, origin, along, up, out, (STRIP_Y[0], -STRIP_HALF_W), (STRIP_Y[1], STRIP_HALF_W),
             BAR_PROUD + STRIP_LIFT, 0, (u - h, v - h, u + h, v + h))
    # ammo screen housing: frame = viewer's right (-X), up the tilted pane, out of it (towards the player)
    right, s_up, s_out = screen_frame()
    solids += box(b, SCREEN_C, right, s_up, s_out,
                  (-SCREEN_HALF[0] - SCREEN_FRAME, -SCREEN_HALF[1] - SCREEN_FRAME, -SCREEN_DEEP),
                  (SCREEN_HALF[0] + SCREEN_FRAME, SCREEN_HALF[1] + SCREEN_FRAME, -0.0006), 1)
    for f in b.faces:
        if f.material_index == 1:
            for loop in f.loops:
                loop[layer_uv].uv = (0.5, 0.5)
    return finish(b, "Rifle_Extras", [mat, metal_mat], solids)


def screen_frame():
    t = math.radians(SCREEN_TILT)
    return Vector((-1.0, 0.0, 0.0)), Vector((0.0, -math.sin(t), math.cos(t))), Vector((0.0, math.cos(t), math.sin(t)))


def build_display():
    """The hologram pane. UV: U to the viewer's right, V up in Blender (down in Unreal), so a texture reads upright."""
    b = bmesh.new()
    right, s_up, s_out = screen_frame()
    quad(b, SCREEN_C, right, s_up, s_out, (-SCREEN_HALF[0], -SCREEN_HALF[1]), (SCREEN_HALF[0], SCREEN_HALF[1]),
         0.0, 0, (0.0, 0.0, 1.0, 1.0))
    holo = flat_material("Holo_AmmoDisplay", (0.02, 0.05, 0.05), 0.3, 0.0, glow=(MINT[0], MINT[1], MINT[2], 0.25))
    return finish(b, "Rifle_AmmoDisplay", [holo], [], chamfer=False, smooth=False)


def mag_section(t):
    """Front and back Y and the Z of the magazine at t (0 top, 1 bottom): a gentle forward curve."""
    z = MAG_TOP_Z + (MAG_BOTTOM_Z - MAG_TOP_Z) * t
    shift = MAG_FORWARD * t * t
    return MAG_TOP_Y[0] - shift, MAG_TOP_Y[1] - shift, z


def build_magazine():
    """Three slots: body, base plate, glowing slit."""
    b = bmesh.new()
    layer_uv = b.loops.layers.uv.verify()

    def rings_tube(rings, mat_index):
        loops = [[b.verts.new((CX - hw, yf, z)), b.verts.new((CX + hw, yf, z)),
                  b.verts.new((CX + hw, yb, z)), b.verts.new((CX - hw, yb, z))] for yf, yb, z, hw in rings]
        faces = []
        for a, c in zip(loops, loops[1:]):
            for i in range(4):
                j = (i + 1) % 4
                faces.append(b.faces.new((a[i], a[j], c[j], c[i])))
        faces.append(b.faces.new(loops[0]))
        faces.append(b.faces.new(list(reversed(loops[-1]))))
        for f in faces:
            f.material_index = mat_index
        return faces

    body = rings_tube([mag_section(k / MAG_RINGS) + (MAG_HALF_W,) for k in range(MAG_RINGS + 1)], 0)
    yf, yb, z = mag_section(1.0)    # base plate: a little wider and longer, 9 mm tall
    plate = rings_tube([(yf - 0.004, yb + 0.004, z + 0.002, MAG_HALF_W + 0.0015),
                        (yf - 0.004, yb + 0.004, z - 0.007, MAG_HALF_W + 0.0015)], 1)
    # glowing round-counter slit on each side: a thin quad 0.4 mm proud of the side, from 25% to 80% of the length
    for side in (-1, 1):
        x = CX + side * (MAG_HALF_W + 0.0004)
        pts = []
        for t in (0.25, 0.8):
            yf, yb, z = mag_section(t)
            mid = (yf + yb) / 2
            pts.append((mid - 0.003, z)); pts.append((mid + 0.003, z))
        f = b.faces.new([b.verts.new((x, pts[0][0], pts[0][1])), b.verts.new((x, pts[1][0], pts[1][1])),
                         b.verts.new((x, pts[3][0], pts[3][1])), b.verts.new((x, pts[2][0], pts[2][1]))])
        f.material_index = 2
        f.normal_update()
        if (f.normal.x > 0) != (side > 0):
            f.normal_flip()
    for f in b.faces:
        for loop in f.loops:
            loop[layer_uv].uv = (0.5, 0.5)
    mats = [flat_material("Rifle_Mag_Body", (0.22, 0.23, 0.245), 0.35, 0.6),
            flat_material("Rifle_Mag_Plate", (0.86, 0.87, 0.89), 0.45, 0.1),
            flat_material("Rifle_Mag_Glow", (0.1, 0.1, 0.1), 0.5, 0.0, glow=(MINT[0], MINT[1], MINT[2], 4.0))]
    return finish(b, "Rifle_Magazine", mats, body + plate, chamfer=False, smooth=False)


EXTRAS = build_extras()   # in the scene before the bakes, so they shade what is around them

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


def math_node(op, a, b=None):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation = op
    for i, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[i].default_value = v
        else:
            nt.links.new(v, n.inputs[i])
    return n.outputs[0]


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
    for o in scene.objects:
        o.select_set(False)
    SRC.select_set(True)
    bpy.context.view_layer.objects.active = SRC
    bpy.ops.object.bake(type=kind, use_clear=True, **kwargs)
    nt.nodes.remove(node)
    print("BAKED", name)
    return image


im_bake_n = bake("NORMAL", "Rifle_NormalBake", 24, normal_space="TANGENT")
im_ao = bake("AO", "Rifle_AO", 96)

normal = load(im_bake_n)[..., :3].copy()
normal[BLOCK] = (0.5, 0.5, 1.0)                 # the strips' block: flat
im_normal = new_image("T_Rifle_Normal", normal, False)
ao = np.clip(load(im_ao)[..., 0], 0.0, 1.0) * (1.0 - 0.45 * dark)
ao[BLOCK] = 1.0
im_orm = new_image("T_Rifle_ORM", np.stack([ao, rough, metal], axis=2), False)
for im in (im_ao, im_bake_n):
    bpy.data.images.remove(im)

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
glow.inputs["B"].default_value = MINT + (1.0,)     # the rifle's own mint
nt.links.new(t_mask.outputs["Color"], glow.inputs["A"])
nt.links.new(glow.outputs["Result"], bsdf.inputs["Emission Color"])
# The 13 cells of the energy strip, as M_RifleAtlas draws them in the game: a dark gap at the start of each cell.
coord = nt.nodes.new("ShaderNodeTexCoord")
xyz = nt.nodes.new("ShaderNodeSeparateXYZ")
nt.links.new(coord.outputs["Object"], xyz.inputs["Vector"])
cell_pos = math_node("MULTIPLY", math_node("SUBTRACT", xyz.outputs["Y"], STRIP_Y[0]), STRIP_CELLS / (STRIP_Y[1] - STRIP_Y[0]))
on_strip = math_node("MULTIPLY", math_node("GREATER_THAN", cell_pos, 0.0), math_node("LESS_THAN", cell_pos, float(STRIP_CELLS)))
on_strip = math_node("MULTIPLY", on_strip, math_node("LESS_THAN", xyz.outputs["Z"], STRIP_ZMAX))
lit = math_node("GREATER_THAN", math_node("FRACT", cell_pos), CELL_GAP)
cells = math_node("MAXIMUM", lit, math_node("SUBTRACT", 1.0, on_strip))
nt.links.new(math_node("MULTIPLY", cells, 4.0), bsdf.inputs["Emission Strength"])
bpy.data.images.remove(im_height)
for im in (color_im, neon_im):
    bpy.data.images.remove(im)

# ---------------------------------------------------------------- split the parts, join the modelled ones
for o in scene.objects:
    o.select_set(False)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
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
EXTRAS.select_set(True)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
bpy.ops.object.join()
SRC.name = "Rifle_Body"
me.name = "Rifle_Body"
DISPLAY = build_display()
MAGAZINE = build_magazine()

# What the export needs to know, kept with the objects.
SRC["pivot"] = HAND[:]
bpy.data.objects["Rifle_Lens"]["pivot"] = HAND[:]
bpy.data.objects["Rifle_Trigger"]["pivot"] = TRIGGER_PIVOT[:]
DISPLAY["pivot"] = HAND[:]
MAGAZINE["pivot"] = MAG_PIVOT[:]
MAGAZINE["out"] = MAG_OUT[:]
scene["strip_y"] = STRIP_Y
scene["strip_zmax"] = STRIP_ZMAX
for o in scene.objects:
    if o.type == "MESH":
        print("OBJECT", o.name, "tris", len(o.data.polygons), "slots", [m.name for m in o.data.materials],
              "custom normals", o.data.has_custom_normals)

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
