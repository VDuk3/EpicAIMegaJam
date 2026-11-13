# Builds the shotgun's one source .blend from the owner's Tripo GLB (SourceArt/Weapons/Shotgun/model.glb).
# The GLB is one mesh of 20,957 triangles turned 11.3 deg about the vertical axis, with one 4096 colour JPG that
# carries baked light, a frost-like wear and patches where projections meet, and no roughness, metallic, normal, AO or
# emissive map. Its cables have no colour of their own and their imprint is painted on the body behind them.
# This script rebuilds the look on the same shape (the rifle's method, Tools/Blender/rifle_build_blend.py):
#   frame     the gun is turned back and centred: muzzle -Y, up +Z, X = 0 on its centre line, metres
#   geometry  vertices welded, Tripo's normals dropped, hard edges above SHARP_ANGLE, face-area weighted normals
#   cables    split from the body, one clean colour per piece (flat blocks of the atlas), smooth, one subdivision
#             level so the 4 to 6 sided strands become round, fattened back to their thickness
#   BaseColor one navy, the gun's own (the median colour of its undisturbed texels). The brightness of each texel's
#             surroundings (a median on its UV island) has no separate tones on this gun, only one broad hump: cutting
#             it into tones gave patches. It is kept as a faint shade (SHADE) instead, so the frost, the baked light
#             and the cables' imprint are gone. The small drawn details (holes, panel lines) are re-cut crisp; where
#             they are dense, the surface is the carbon weave and gets a darker tone; deep recesses go near black.
#             The circuit board and the small coloured indicators keep their own colours
#   Emissive  cut from the colour: the saturated cyan and blue texels (strips, screens' glyphs, the cell)
#   ORM       R = ambient occlusion baked in Cycles (with the cables in place: their real contact shadow),
#             G = roughness and B = metallic by tone
#   Normal    baked in Cycles (OpenGL, +Y): rounded edges (Bevel node) and the drawn details pressed in (Bump)
# Usage: blender -b --factory-startup --python shotgun_build_blend.py -- <model.glb> <out.blend> [review_dir] [tex|full]
#        "tex" stops after the textures are computed and dumped to review_dir (no bakes, nothing saved).
import bpy, bmesh, sys, os, math
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
glb, out_blend = argv[0], argv[1]
review_dir = argv[2] if len(argv) > 2 else None
stage = argv[3] if len(argv) > 3 else "full"

SHARP_ANGLE = 30.0
BEVEL_RADIUS = 0.0008      # metres: rounded-edge width baked into the normal map
DETAIL_DEPTH = 0.0004      # metres: how deep the drawn details are pressed in
AO_DISTANCE = 0.05
BAKE_SIZE = 4096
PAD = 12                   # texels of padding around every UV island
VOXEL = 0.004              # metres: the cell of space in which small islands look for their surroundings
MEDIAN_R = 5               # texels: half window of the median that gives each texel its surroundings' brightness
SHADE = 0.08               # how much of the surroundings' brightness stays in the navy (1 = all of it)
NAVY = (0.50, 0.20)        # roughness, metallic of the body
# The carbon panels do not differ from the body in brightness but in texture: the mean deviation of a texel's
# brightness from its surroundings, over CARBON_R texels, is about 0.03 on the plain body and 0.16 to 0.20 on the weave.
CARBON = (0.50, 0.36, 0.0)          # the weave's panels: (the navy times this, roughness, metallic)
CARBON_R, CARBON_AT, CARBON_BRIGHT = 5, 0.115, 0.31   # ...and never on bright surroundings (the frost on top faces)
WEAVE_PERIOD, WEAVE_LINE = 0.0030, 0.24   # metres between the weave's lines, drawn by position; share that is line
# Parts in another tone, so the gun is not one flat navy: (the navy times this, roughness, metallic)
PART_TONES = {2: (0.70, 0.42, 0.55),    # sight: darker satin metal
              3: (0.55, 0.38, 0.70),    # trigger: steel
              4: (0.45, 0.82, 0.00)}    # butt pad: rubber
BUTT_BOX = ((-0.060, 0.410, 0.090), (0.060, 0.520, 0.320))
# The cell: its glow is ragged in the texture and its surface is lumpy. A cylinder is modelled over it, along Y (axis
# and radius fitted to the glowing texels; front and rear ends given): dim glass with three bright rings, slightly
# proud, where the texture has its bright bands (centre, half width). The GLB's own cell underneath goes dark, and
# whatever else glows inside the box is put out.
GLOW_UP_ONLY = ((-0.070, -0.520, 0.185), (0.070, -0.240, 0.240))
GLOW_STRIP_HALF = 0.013    # ...and within this of the centre line
GLOW_NONE = [((-0.060, -0.262, 0.185), (0.060, -0.110, 0.270))]   # the barrel's top between handguard and sight: frost
CELL_BOX = ((-0.045, -0.236, 0.098), (0.045, -0.114, 0.178))
CELL_SKIN = (-0.009, 0.004)   # metres from the fitted cylinder that still count as the cell (it has grooves)
SMALL_ISLAND = 3000        # texels: below this an island is too small to tell a weave on its own
GLOW_ISLAND = (1200, 0.05) # an island glows if it has this many glowing texels, or this share of them
CELL_Y = (-0.2295, -0.1235)
CELL_RINGS = [(-0.2205, 0.0065), (-0.1860, 0.0065), (-0.1340, 0.0065)]
CELL_DIM = 0.14
CELL_PROUD, CELL_RING_PROUD, CELL_SIDES = 0.0045, 0.0012, 40   # metres over the fitted radius; sides of the cylinder
DEEP = (0.30, 0.65, 0.0)            # deep recesses: (the navy times this, roughness, metallic)
DEEP_AT = (0.105, 0.065)            # surroundings' brightness where the recess tone starts and where it is full
DARK_DETAIL = (0.36, 0.80, 0.0)     # (the tone's colour times this, roughness, metallic)
EMISSION = 1.0                      # Blender review only
NEON_BASE = ((0.020, 0.024, 0.030), 0.30, 0.0)
KEEP = (0.60, 0.0)                  # roughness and metallic of the texels that keep their own colour
GLOW = (0.10, 0.85, 1.00)           # the gun's own cyan (Blender review; the game sets the element's colour)

# Cable colours (sRGB, roughness, metallic), given to the cable pieces in turn: the colours the texture hints at.
CABLE_COLOURS = [
    ((0.030, 0.032, 0.036), 0.50, 0.0),   # black rubber
    ((0.560, 0.180, 0.060), 0.45, 0.0),   # copper orange
    ((0.300, 0.045, 0.040), 0.45, 0.0),   # dark red
    ((0.060, 0.170, 0.420), 0.45, 0.0),   # blue
]
CABLE_KEEP = 0.55          # share of the subdivided cables' faces kept by the decimation
CABLE_FATTEN = 0.00045     # metres: pushed out along the normals after the subdivision, which thins a strand
# Welded pieces fully inside one of these boxes and longer than CABLE_MIN are cables (straightened frame).
CABLE_BOXES = [
    ((0.012, 0.025, 0.205), (0.060, 0.225, 0.282)),      # left of the receiver
    ((0.012, -0.425, 0.104), (0.072, -0.295, 0.210)),    # left of the handguard
    ((-0.072, -0.425, 0.104), (-0.012, -0.295, 0.210)),  # right of the handguard
    ((0.010, -0.030, 0.195), (0.056, 0.025, 0.250)),     # loops beside the left screen
    ((-0.045, -0.030, 0.195), (-0.015, 0.025, 0.245)),   # loop beside the right screen
]
CABLE_MIN = 0.025
# The body's texels near the cables carry their imprint (orange smears, a painted shadow): no drawn details there.
IMPRINT_GROW = 0.022
# Islands that keep their own colours: the circuit board (left of the receiver) and what is green around it.
BOARD_BOX = ((0.008, -0.030, 0.190), (0.060, 0.230, 0.285))
SIGHT_BOX = ((-0.060, -0.200, 0.225), (0.060, 0.010, 0.400))
TRIGGER_BOX = ((-0.012, 0.060, 0.110), (0.012, 0.120, 0.180))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
scene = bpy.context.scene
SRC = [o for o in scene.objects if o.type == "MESH"][0]
for o in scene.objects:
    o.select_set(False)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
SRC.rotation_mode = "XYZ"
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in [o for o in scene.objects if o.type != "MESH"]:
    bpy.data.objects.remove(o)
me = SRC.data
color_im = next(i for i in bpy.data.images if i.size[0] > 0)
UV_NAME = me.uv_layers[0].name

# ---------------------------------------------------------------- frame: turn the gun back, centre it
co = np.empty(len(me.vertices) * 3, np.float64)
me.vertices.foreach_get("co", co)
co = co.reshape(-1, 3)
width, yaw = min((np.ptp(co[:, 0] * math.cos(a) - co[:, 1] * math.sin(a)), a) for a in np.radians(np.arange(-25, 25, 0.05)))
x = co[:, 0] * math.cos(yaw) - co[:, 1] * math.sin(yaw)
y = co[:, 0] * math.sin(yaw) + co[:, 1] * math.cos(yaw)
co[:, 0], co[:, 1] = x - (x.min() + x.max()) / 2, y
me.vertices.foreach_set("co", co.ravel())
me.update()
print("FRAME yaw %.2f deg, width %.1f mm, y %.4f .. %.4f, z %.4f .. %.4f" %
      (math.degrees(yaw), width * 1000, co[:, 1].min(), co[:, 1].max(), co[:, 2].min(), co[:, 2].max()))


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


def median_sep(a, r):
    """Median along the rows, then along the columns: close to a square median, at a fraction of the cost."""
    p = np.pad(a, ((0, 0), (r, r)), mode="edge")
    a = np.median(sliding_window_view(p, 2 * r + 1, axis=1), axis=-1)
    p = np.pad(a, ((r, r), (0, 0)), mode="edge")
    return np.median(sliding_window_view(p, 2 * r + 1, axis=0), axis=-1).astype(np.float32)


def shifted(a, dy, dx):
    out = np.zeros_like(a)
    h, w = a.shape[:2]
    out[max(dy, 0):h + min(dy, 0), max(dx, 0):w + min(dx, 0)] = a[max(-dy, 0):h + min(-dy, 0), max(-dx, 0):w + min(-dx, 0)]
    return out


def fill_from(valid, a, iters):
    """Grows the valid values of a into the texels around them."""
    a, v = a.copy(), valid.copy()
    for _ in range(iters):
        if v.all():
            break
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            take = ~v & shifted(v, dy, dx)
            a[take] = shifted(a, dy, dx)[take]
            v |= take
    return a, v


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


def inside(lo, hi, blo, bhi):
    return all(blo[i] <= lo[i] and hi[i] <= bhi[i] for i in range(3))


# ---------------------------------------------------------------- UV islands (loose parts of the raw mesh) and pieces
bm = bmesh.new()
bm.from_mesh(me)
islands, face_island = components(bm)
n_faces = len(bm.faces)
tag = bm.faces.layers.int.new("orig")
for f in bm.faces:
    f[tag] = f.index
welded = bm.copy()
bmesh.ops.remove_doubles(welded, verts=welded.verts[:], dist=1e-5)
wtag = welded.faces.layers.int["orig"]
pieces, _ = components(welded)

# Parts by welded piece: 0 body, 1 cable, 2 sight, 3 trigger, 4 butt pad. Cable pieces are numbered (strand) to take a colour each.
face_part = np.zeros(n_faces, np.int32)
face_strand = np.full(n_faces, -1, np.int32)
cable_bounds = []
order = sorted(range(len(pieces)), key=lambda i: -len(pieces[i]))
for rank, pi in enumerate(order):
    comp = pieces[pi]
    vs = {v for i in comp for v in welded.faces[i].verts}
    lo = [min(v.co[k] for v in vs) for k in range(3)]
    hi = [max(v.co[k] for v in vs) for k in range(3)]
    orig = [welded.faces[i][wtag] for i in comp]
    if rank == 0:
        continue
    if max(hi[k] - lo[k] for k in range(3)) > CABLE_MIN and any(inside(lo, hi, *b) for b in CABLE_BOXES):
        face_part[orig] = 1
        face_strand[orig] = len(cable_bounds)
        cable_bounds.append((lo, hi))
    elif len(comp) > 500 and inside(lo, hi, *SIGHT_BOX):
        face_part[orig] = 2
    elif inside(lo, hi, *TRIGGER_BOX):
        face_part[orig] = 3
    elif len(comp) > 300 and inside(lo, hi, *BUTT_BOX):
        face_part[orig] = 4
welded.free()
bm.free()
print("ISLANDS", len(islands), "PIECES", len(pieces), "cable pieces", len(cable_bounds), "cable faces", int((face_part == 1).sum()),
      "sight faces", int((face_part == 2).sum()), "trigger faces", int((face_part == 3).sum()),
      "butt pad faces", int((face_part == 4).sum()))
if not (face_part == 1).any() or not (face_part == 2).any() or not (face_part == 3).any():
    raise SystemExit("shotgun_build_blend: cables, sight or trigger not found")

# ---------------------------------------------------------------- the atlas: which island, where on the gun, facing where
col = load(color_im)[..., :3]
H, W = col.shape[:2]
lum = col[..., 0] * 0.2126 + col[..., 1] * 0.7152 + col[..., 2] * 0.0722

n_loops = len(me.loops)
uv = np.empty(n_loops * 2, np.float32)
me.uv_layers[0].uv.foreach_get("vector", uv)
uv = uv.reshape(n_loops, 2)
loop_start = np.empty(len(me.polygons), np.int32)
me.polygons.foreach_get("loop_start", loop_start)
loop_vert = np.empty(n_loops, np.int32)
me.loops.foreach_get("vertex_index", loop_vert)
loop_co = co.astype(np.float32)[loop_vert]
face_n = np.empty(len(me.polygons) * 3, np.float32)
me.polygons.foreach_get("normal", face_n)
face_n = face_n.reshape(-1, 3)
face_dir = (np.abs(face_n).argmax(1) * 2 + (np.take_along_axis(face_n, np.abs(face_n).argmax(1)[:, None], 1)[:, 0] > 0)).astype(np.uint8)

ids = np.full((H, W), -1, np.int32)
pos = np.zeros((H, W, 3), np.float32)
part = np.zeros((H, W), np.uint8)
facing = np.zeros((H, W), np.uint8)
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
    ids[y0:y1, x0:x1][m] = face_island[f]
    part[y0:y1, x0:x1][m] = face_part[f]
    facing[y0:y1, x0:x1][m] = face_dir[f]
    c = loop_co[loop_start[f]:loop_start[f] + 3]
    pos[y0:y1, x0:x1][m] = (c[0] + w1[..., None] * (c[1] - c[0]) + w2[..., None] * (c[2] - c[0]))[m]
covered = ids >= 0
print("ATLAS covered %.1f%%" % (100 * covered.mean()))


def in_box(lo, hi):
    return covered & np.all((pos >= np.asarray(lo, np.float32)) & (pos <= np.asarray(hi, np.float32)), axis=2)


# ---------------------------------------------------------------- what each texel is, from its colour
mx, mn = col.max(axis=2), col.min(axis=2)
sat = (mx - mn) / np.maximum(mx, 1e-3)
glow_raw = (covered & (part != 1) & (mx > 0.55) & (col[..., 0] < 0.52 * mx) & (sat > 0.50)).astype(np.float32)
glow_raw *= box_blur(glow_raw, 2) > 0.42          # the frost has cyan specks: a glowing texel has glowing neighbours
glow_raw *= box_blur(glow_raw, 6) > 0.35          # ...and belongs to a shape of some size, not to a blob
# The handguard's strip is on its top face; the projection spilled it onto the sides as ragged blobs.
glow_raw[in_box(*GLOW_UP_ONLY) & ((facing != 5) | (np.abs(pos[..., 0]) > GLOW_STRIP_HALF))] = 0.0
for lo_, hi_ in GLOW_NONE:
    glow_raw[in_box(lo_, hi_)] = 0.0
green = (col[..., 1] > col[..., 0] * 1.12) & (col[..., 1] > col[..., 2] * 1.08) & (sat > 0.2)
warm = (col[..., 0] > col[..., 2] * 1.10) & (sat > 0.2) & (mx > 0.12)

# Islands that keep their own colours: mostly green or warm, on the body, in the circuit board's zone; and on the
# sight, the small coloured indicators.
n_isl = len(islands)
flat_ids = ids[covered]


def share(mask):
    return np.bincount(flat_ids, weights=mask[covered].astype(np.float64), minlength=n_isl) / \
        np.maximum(np.bincount(flat_ids, minlength=n_isl), 1)


# Glow only on islands that really glow: a strip, a screen, the cell. A blob on a large plain island is frost.
isl_glow = np.bincount(flat_ids, weights=glow_raw[covered].astype(np.float64), minlength=n_isl)
isl_texels = np.bincount(flat_ids, minlength=n_isl)
glows = (isl_glow >= GLOW_ISLAND[0]) | (isl_glow / np.maximum(isl_texels, 1) >= GLOW_ISLAND[1])
glow_raw *= glows[np.where(covered, ids, 0)]
isl_colour = share(green | warm)
isl_green = share(green)
isl_board = share(in_box(*BOARD_BOX) & (part == 0))
isl_sight = share(part == 2)
keep_isl = ((isl_board > 0.9) & (isl_green > 0.06)) | ((isl_sight > 0.9) & (isl_colour > 0.35))
keep = covered & keep_isl[np.where(covered, ids, 0)]
print("KEEP islands", int(keep_isl.sum()), "texels", int(keep.sum()), "GLOW texels", int(glow_raw.sum()))

# The cables' imprint on the body: no drawn details near the cables.
plain = np.zeros((H, W), bool)
g = IMPRINT_GROW
for lo, hi in cable_bounds:
    plain |= in_box([v - g for v in lo], [v + g for v in hi])
plain &= part != 1

# ---------------------------------------------------------------- each texel's surroundings: a median on its island
neutral = covered & (glow_raw < 0.5) & ~keep
ys, xs = np.nonzero(covered)
y0s, y1s = np.full(n_isl, H, np.int64), np.zeros(n_isl, np.int64)
x0s, x1s = np.full(n_isl, W, np.int64), np.zeros(n_isl, np.int64)
np.minimum.at(y0s, flat_ids, ys)
np.maximum.at(y1s, flat_ids, ys)
np.minimum.at(x0s, flat_ids, xs)
np.maximum.at(x1s, flat_ids, xs)
around = np.zeros((H, W), np.float32)
weave_isl = np.zeros((H, W), np.float32)
mgn = MEDIAN_R + 2
for i in range(n_isl):
    if y1s[i] < y0s[i]:
        continue
    sl = (slice(max(y0s[i] - mgn, 0), min(y1s[i] + mgn + 1, H)), slice(max(x0s[i] - mgn, 0), min(x1s[i] + mgn + 1, W)))
    mask = ids[sl] == i
    valid = mask & neutral[sl]
    if valid.sum() < 4:
        around[sl][mask] = np.median(lum[sl][mask])
        continue
    filled, got = fill_from(valid, np.where(valid, lum[sl], 0.0), mgn + 8)
    filled[~got] = np.median(lum[sl][valid])
    ar = median_sep(filled, MEDIAN_R)
    around[sl][mask] = ar[mask]
    # The weave, decided on the island alone (a blur across islands would mix unrelated neighbours of the atlas),
    # then smoothed twice so a panel has an even outline and no holes.
    wv = (box_blur(np.abs(filled / np.maximum(ar, 0.03) - 1.0), CARBON_R) > CARBON_AT) & (ar < CARBON_BRIGHT)
    wv = box_blur(wv.astype(np.float32), 6) > 0.5
    wv &= box_blur(wv.astype(np.float32), 14) > 0.25          # a panel, not a patch
    weave_isl[sl][mask] = box_blur(wv.astype(np.float32), 8)[mask]
print("AROUND done")

sel = neutral & (part != 1)
# Small islands take the weave of the surface around them: the mean over the texels of large islands in the same small
# cell of space (VOXEL) that face the same way.
big = isl_texels[np.where(covered, ids, 0)] >= SMALL_ISLAND
v_ = np.floor(pos[sel] / VOXEL).astype(np.int64) + 512
key = ((v_[:, 0] * 1024 + v_[:, 1]) * 1024 + v_[:, 2]) * 6 + facing[sel]
_, inv = np.unique(key, return_inverse=True)
w_big = big[sel].astype(np.float64)
v_sum = np.bincount(inv, weights=weave_isl[sel] * w_big)
v_cnt = np.bincount(inv, weights=w_big)
around_weave = np.where(v_cnt > 0, v_sum / np.maximum(v_cnt, 1), -1.0)[inv]
own_weave = weave_isl[sel]
weave_isl[sel] = np.where((w_big > 0) | (around_weave < 0), own_weave, around_weave)
print("WEAVE small-island texels that took their surroundings' value: %d" % int(((w_big == 0) & (around_weave >= 0)).sum()))
hist, edges = np.histogram(around[sel], bins=40, range=(0.0, 0.8))
print("BRIGHTNESS of the surroundings (share of the body's texels per bin):")
for h_, e in zip(hist, edges):
    print("   %.2f %5.1f%% %s" % (e, 100 * h_ / sel.sum(), "#" * int(200 * h_ / sel.sum())))
undisturbed = sel & (np.abs(lum / np.maximum(around, 0.02) - 1.0) < 0.10)
navy = np.median(col[undisturbed], axis=0)
mid = float(np.median(around[sel]))
print("NAVY colour %.3f %.3f %.3f, median brightness of the surroundings %.3f" % (*navy, mid))

# ---------------------------------------------------------------- drawn details, re-cut crisp; the weave; recesses
ratio = lum / np.maximum(around, 0.03)
# Details from a slightly blurred brightness: a line survives it, the frost's single dark specks do not.
dark_all = smoothstep(0.74, 0.56, box_blur(lum, 1) / np.maximum(around, 0.03)) * neutral * (part != 1)
carbon = smoothstep(0.42, 0.58, weave_isl) * sel * (part == 0) * ~plain
# The weave, drawn by where each texel is on the gun: two sets of diagonal lines on the plane the face looks along.
axis = facing // 2
pu = np.where(axis == 0, pos[..., 1], pos[..., 0])
pv = np.where(axis == 2, pos[..., 1], pos[..., 2])


def lines(t):
    d = np.abs((t / WEAVE_PERIOD) % 1.0 - 0.5) * 2.0          # 0 on a line's centre, 1 midway between two lines
    return 1.0 - smoothstep(WEAVE_LINE * 0.6, WEAVE_LINE * 1.4, d)


weave = np.maximum(lines((pu + pv) * 0.7071), lines((pu - pv) * 0.7071)) * carbon
dark = np.maximum(dark_all * ~plain * (1.0 - carbon), weave)
deep = smoothstep(DEEP_AT[0], DEEP_AT[1], around) * neutral * (part != 1) * (1.0 - carbon)
neon_a = smoothstep(0.35, 0.65, box_blur(glow_raw, 1)) * covered
# The cell, by position.
in_cell_box = in_box(*CELL_BOX) & (part == 0)
lit = in_cell_box & (glow_raw > 0.5)
if lit.sum() < 1000:
    raise SystemExit("shotgun_build_blend: the cell's glow was not found")
# A circle through the glowing texels, seen along the gun (x, z): least squares, twice, dropping what is off it.
cx_, cz_ = pos[..., 0][lit].astype(np.float64), pos[..., 2][lit].astype(np.float64)
for _ in range(3):
    sol = np.linalg.lstsq(np.stack([2 * cx_, 2 * cz_, np.ones_like(cx_)], axis=1), cx_ ** 2 + cz_ ** 2, rcond=None)[0]
    cell_r = math.sqrt(sol[2] + sol[0] ** 2 + sol[1] ** 2)
    off = np.abs(np.sqrt((cx_ - sol[0]) ** 2 + (cz_ - sol[1]) ** 2) - cell_r)
    cx_, cz_ = cx_[off < 0.004], cz_[off < 0.004]
print("CELL axis x %.1f mm z %.1f mm, radius %.1f mm, %d of %d glowing texels on it, y %s" %
      (sol[0] * 1000, sol[1] * 1000, cell_r * 1000, len(cx_), lit.sum(),
       " ".join("%.4f" % v_ for v_ in np.percentile(pos[..., 1][lit], (2, 98)))))
radial = np.sqrt((pos[..., 0] - sol[0]) ** 2 + (pos[..., 2] - sol[1]) ** 2)
cell = in_cell_box & (radial - cell_r > CELL_SKIN[0]) & (radial - cell_r < CELL_SKIN[1]) & \
    (pos[..., 1] > CELL_Y[0]) & (pos[..., 1] < CELL_Y[1]) & (box_blur(lit.astype(np.float32), 12) > 0.05)
neon_a[in_cell_box] = 0.0
dark[cell] = 0.0
deep[cell] = 1.0          # the GLB's own cell, under the modelled one
CELL_AXIS, CELL_R = (float(sol[0]), float(sol[1])), float(cell_r)
print("CELL texels under the modelled cylinder", int(cell.sum()))
print("CARBON %.1f%% of the body's texels, DEEP %.1f%%, DARK details %.1f%%" %
      (100 * carbon[sel].mean(), 100 * deep[sel].mean(), 100 * dark[sel].mean()))


def layer(base, value, alpha):
    return base + (np.asarray(value, np.float32) - base) * (alpha[..., None] if base.ndim == 3 else alpha)


shade = 1.0 + SHADE * np.clip(around / mid - 1.0, -1.0, 1.0)
base = navy[None, None, :] * shade[..., None]
rough = np.full((H, W), NAVY[0], np.float32)
metal = np.full((H, W), NAVY[1], np.float32)
for code, (k_, r_, m_) in PART_TONES.items():
    here = part == code
    base[here] *= k_
    rough[here] = r_
    metal[here] = m_
for (k_, r_, m_), a in ((CARBON, carbon), (DEEP, deep), (DARK_DETAIL, dark)):
    base = layer(base, base * k_, a)
    rough = layer(rough, r_, a)
    metal = layer(metal, m_, a)
own_colour = np.stack([box_blur(col[..., k], 1) for k in range(3)], axis=2)
base[keep] = own_colour[keep]
rough[keep] = KEEP[0]
metal[keep] = KEEP[1]
base = layer(base, NEON_BASE[0], neon_a)
rough = layer(rough, NEON_BASE[1], neon_a)
metal = layer(metal, NEON_BASE[2], neon_a)
height = 1.0 - np.maximum(dark, neon_a * 0.6)

# ---------------------------------------------------------------- padding, and flat blocks for the cables
src = np.where(covered, np.arange(H * W, dtype=np.int64).reshape(H, W), -1)
got = covered.copy()
for _ in range(PAD):
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        take = ~got & shifted(got, dy, dx)
        src[take] = shifted(src, dy, dx)[take]
        got |= take
flat_src = src[got]


def padded(a, empty):
    out = np.full(a.shape, empty, np.float32) if a.ndim == 2 else np.full(a.shape, empty, np.float32)
    out[got] = a.reshape(H * W, -1)[flat_src].reshape((-1,) + a.shape[2:]) if a.ndim == 3 else a.ravel()[flat_src]
    return out


base, rough, metal = padded(base, 0.1), padded(rough, 0.6), padded(metal, 0.0)
height, neon_a = padded(height, 1.0), padded(neon_a, 0.0)
used = got.copy()
BLOCKS = []


def claim_block(what):
    for reach in (26, 20, 14):
        free = box_blur((~used).astype(np.float32), reach) > 0.9999
        free[:48, :] = free[-48:, :] = False
        free[:, :48] = free[:, -48:] = False
        if free.any():
            break
    else:
        raise SystemExit("shotgun_build_blend: ERROR no free block in the atlas for " + what)
    fy, fx = np.argwhere(free)[0]
    half = reach - 6
    block = (slice(fy - half, fy + half + 1), slice(fx - half, fx + half + 1))
    used[fy - reach:fy + reach + 1, fx - reach:fx + reach + 1] = True
    BLOCKS.append(block)
    print("BLOCK for", what, "at texel", int(fx), int(fy), "half size", half)
    return block, ((fx + 0.5) / W, (fy + 0.5) / H)


_, CELL_DIM_UV = claim_block("the cell's glass")
neon_a[BLOCKS[-1]] = CELL_DIM
base[BLOCKS[-1]], rough[BLOCKS[-1]], metal[BLOCKS[-1]], height[BLOCKS[-1]] = NEON_BASE[0], 0.18, 0.0, 1.0
_, CELL_RING_UV = claim_block("the cell's rings")
neon_a[BLOCKS[-1]] = 1.0
base[BLOCKS[-1]], rough[BLOCKS[-1]], metal[BLOCKS[-1]], height[BLOCKS[-1]] = NEON_BASE[0], 0.18, 0.0, 1.0
CABLE_UV = []
for k, (rgb, r_, m_) in enumerate(CABLE_COLOURS):
    block, uv_c = claim_block("cable colour %d" % k)
    base[block], rough[block], metal[block], height[block], neon_a[block] = rgb, r_, m_, 1.0, 0.0
    CABLE_UV.append(uv_c)

im_base = new_image("T_Shotgun_BaseColor", base, True)
im_height = new_image("Shotgun_Height", height, False)
im_mask = new_image("T_Shotgun_EmissiveMask", np.repeat(neon_a[..., None], 3, 2), False)


def dump(im_or_array, name, srgb=True):
    if not review_dir:
        return
    os.makedirs(review_dir, exist_ok=True)
    im = im_or_array if isinstance(im_or_array, bpy.types.Image) else new_image("dump_" + name, im_or_array, srgb)
    c = im.copy()
    c.scale(2048, 2048)
    c.filepath_raw = os.path.join(review_dir, name + ".png")
    c.file_format = "PNG"
    c.save()
    bpy.data.images.remove(c)
    if im is not im_or_array:
        bpy.data.images.remove(im)


dump(im_base, "T_Shotgun_BaseColor")
dump(im_mask, "T_Shotgun_EmissiveMask")
if stage == "tex":
    dump(np.repeat((around * covered)[..., None], 3, 2), "debug_around")
    dump(np.stack([dark, carbon, np.maximum(keep.astype(np.float32), deep)], axis=2), "debug_details", False)
    print("TEX stage done")
    sys.exit(0)

# ---------------------------------------------------------------- geometry
attr = me.attributes.new("part", "INT", "FACE")
attr.data.foreach_set("value", face_part)
attr = me.attributes.new("strand", "INT", "FACE")
attr.data.foreach_set("value", face_strand)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.remove_doubles(threshold=1e-5)
bpy.ops.mesh.select_all(action="DESELECT")
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.mesh.customdata_custom_splitnormals_clear()
me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))


def face_values(mesh, name):
    a = np.empty(len(mesh.polygons), np.int32)
    mesh.attributes[name].data.foreach_get("value", a)
    return a


def only(ob):
    for o in scene.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


def separate(code, name):
    only(SRC)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    me.polygons.foreach_set("select", (face_values(me, "part") == code).tolist())
    bpy.ops.object.mode_set(mode="EDIT")
    before = set(scene.objects)
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")
    new = next(o for o in scene.objects if o not in before)
    new.name = name
    new.data.name = name
    return new


def weighted_normals(ob):
    only(ob)
    mod = ob.modifiers.new("WeightedNormal", "WEIGHTED_NORMAL")
    mod.mode = "FACE_AREA"
    mod.weight = 50
    mod.keep_sharp = True
    bpy.ops.object.modifier_apply(modifier=mod.name)


CABLES = separate(1, "Shotgun_Cables")
weighted_normals(SRC)

# Cables: one flat colour per piece, no hard edges, round.
cm = CABLES.data
strand = face_values(cm, "strand")
c_loop_total = np.empty(len(cm.polygons), np.int32)
cm.polygons.foreach_get("loop_total", c_loop_total)
c_uv = np.repeat(np.array(CABLE_UV, np.float32)[strand % len(CABLE_UV)], c_loop_total, axis=0)
cm.uv_layers[0].uv.foreach_set("vector", c_uv.ravel())
cm.set_sharp_from_angle(angle=math.radians(180.0))
only(CABLES)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.tris_convert_to_quads(face_threshold=math.radians(60), shape_threshold=math.radians(60))
bpy.ops.object.mode_set(mode="OBJECT")
sub = CABLES.modifiers.new("Round", "SUBSURF")
sub.levels = 1
sub.uv_smooth = "NONE"
bpy.ops.object.modifier_apply(modifier=sub.name)
dec = CABLES.modifiers.new("Lighter", "DECIMATE")
dec.ratio = CABLE_KEEP
bpy.ops.object.modifier_apply(modifier=dec.name)
dis = CABLES.modifiers.new("Fatten", "DISPLACE")
dis.strength = CABLE_FATTEN
dis.mid_level = 0.0
bpy.ops.object.modifier_apply(modifier=dis.name)
cm.polygons.foreach_set("use_smooth", [True] * len(cm.polygons))
print("CABLES tris", sum(len(p.vertices) - 2 for p in cm.polygons))


def build_cell():
    """The cell: a closed cylinder along Y in bands, glass and rings in turn; each band's UVs sit on its flat block."""
    b = bmesh.new()
    layer_uv = b.loops.layers.uv.verify()
    cuts = [CELL_Y[0]]
    for centre, half in sorted(CELL_RINGS):
        cuts += [centre - half, centre + half]
    cuts.append(CELL_Y[1])
    ax, az = CELL_AXIS

    def loop_at(y_, r_):
        return [b.verts.new((ax + r_ * math.cos(2 * math.pi * k / CELL_SIDES), y_, az + r_ * math.sin(2 * math.pi * k / CELL_SIDES)))
                for k in range(CELL_SIDES)]

    def tube(a_, c_, uv_c):
        for k in range(CELL_SIDES):
            j = (k + 1) % CELL_SIDES
            f = b.faces.new((a_[k], a_[j], c_[j], c_[k]))
            for l_ in f.loops:
                l_[layer_uv].uv = uv_c

    prev = None
    for i in range(len(cuts) - 1):
        is_ring = i % 2 == 1
        r_ = CELL_R + CELL_PROUD + (CELL_RING_PROUD if is_ring else 0.0)
        a_, c_ = loop_at(cuts[i], r_), loop_at(cuts[i + 1], r_)
        if prev is not None:
            tube(prev, a_, CELL_RING_UV)                # the step between a band and the next
        tube(a_, c_, CELL_RING_UV if is_ring else CELL_DIM_UV)
        if i == 0:
            f = b.faces.new(a_)
            for l_ in f.loops:
                l_[layer_uv].uv = CELL_DIM_UV
        prev = c_
    f = b.faces.new(list(reversed(prev)))
    for l_ in f.loops:
        l_[layer_uv].uv = CELL_DIM_UV
    bmesh.ops.recalc_face_normals(b, faces=b.faces[:])
    bmesh.ops.triangulate(b, faces=b.faces[:])
    for f in b.faces:
        f.smooth = True
    new_me = bpy.data.meshes.new("Shotgun_Cell")
    b.to_mesh(new_me)
    b.free()
    new_me.uv_layers[0].name = UV_NAME
    new_me.materials.append(me.materials[0])
    ob = bpy.data.objects.new("Shotgun_Cell", new_me)
    scene.collection.objects.link(ob)
    new_me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))
    print("MODELLED Shotgun_Cell tris", len(new_me.polygons), "radius %.1f mm" % ((CELL_R + CELL_PROUD) * 1000))
    return ob


CELL = build_cell()      # in the scene before the bakes, so it shades what is around it

bm = bmesh.new()
bm.from_mesh(me)
print("WELDED body verts", len(bm.verts), "tris", len(bm.faces), "open edges", sum(1 for e in bm.edges if len(e.link_faces) == 1),
      "sharp edges", sum(1 for e in bm.edges if not e.smooth), "of", len(bm.edges))
bm.free()

# ---------------------------------------------------------------- bake: normal (rounded edges + details) and AO
mat = me.materials[0]
mat.name = "Shotgun_Atlas"
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
    only(SRC)
    bpy.ops.object.bake(type=kind, use_clear=True, **kwargs)
    nt.nodes.remove(node)
    print("BAKED", name)
    return image


im_bake_n = bake("NORMAL", "Shotgun_NormalBake", 24, normal_space="TANGENT")
im_ao = bake("AO", "Shotgun_AO", 96)

normal = load(im_bake_n)[..., :3].copy()
for block in BLOCKS:
    normal[block] = (0.5, 0.5, 1.0)
im_normal = new_image("T_Shotgun_Normal", normal, False)
ao = np.clip(load(im_ao)[..., 0], 0.0, 1.0)
for block in BLOCKS:
    ao[block] = 1.0
im_orm = new_image("T_Shotgun_ORM", np.stack([ao, rough, metal], axis=2), False)
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
glow.inputs["B"].default_value = GLOW + (1.0,)
nt.links.new(t_mask.outputs["Color"], glow.inputs["A"])
nt.links.new(glow.outputs["Result"], bsdf.inputs["Emission Color"])
bsdf.inputs["Emission Strength"].default_value = EMISSION
bpy.data.images.remove(im_height)
bpy.data.images.remove(color_im)

# ---------------------------------------------------------------- join the cables back, save
only(CABLES)
CELL.select_set(True)
SRC.select_set(True)
bpy.context.view_layer.objects.active = SRC
bpy.ops.object.join()
SRC.name = "Shotgun_Body"
me.name = "Shotgun_Body"
for o in scene.objects:
    if o.type == "MESH":
        print("OBJECT", o.name, "tris", sum(len(p.vertices) - 2 for p in o.data.polygons), "slots",
              [m.name for m in o.data.materials], "custom normals", o.data.has_custom_normals)
for im in (im_orm, im_normal):
    dump(im, im.name, False)
bpy.ops.wm.save_as_mainfile(filepath=out_blend, compress=True)
print("SAVED", out_blend)
