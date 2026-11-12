# Removes inherited marks (text, digits, QR-like patterns) and earlier flat patches from the pistol atlas.
# Method: seamless (Poisson-style) cloning. For each region a donor block of the same size is chosen nearby by
# matching the ring of pixels around it; the donor is pasted and a harmonic membrane corrects it so the edges
# match the surroundings. The same donor offset is used for Color, ORM and NormalGL so albedo, roughness/metal
# and relief stay consistent. Regions flagged "fill" use only the harmonic fill (smooth, no donor), used inside
# the glowing cyan label where any donor would bring a different pattern.
#
# Usage: blender -b <in.blend> --python pistol_clean_atlas.py -- <out.blend> <review_dir>
import bpy, sys, os
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
out_blend, review_dir = argv[0], argv[1]
os.makedirs(review_dir, exist_ok=True)

# (name, x0, y0, x1, y1, mode) in atlas pixels, origin TOP-LEFT, x1/y1 exclusive.
REGIONS = [
    # a symbol that exists only in the ORM roughness channel (seen as a smudge on the +Y silver panel); first, so
    # no later region can use it as a donor
    ("orm_symbol",       1088, 1210, 1134, 1296, "clone"),
    ("v4_matrix_patch",   518,  894,  610,  994, "clone"),
    ("v4_drawing_patch", 1194, 1068, 1310, 1158, "clone"),
    ("old_flat_patch_a",  238,  950,  324, 1017, "clone"),
    ("symbol_label",      282,  978,  352, 1044, "clone"),
    ("tiny_text",         156,  938,  174,  948, "clone"),
    ("old_flat_patch_b",  860,  826,  968,  902, "grain"),
    ("edge_text_left",      0,  316,   15,  404, "clone"),
    ("qr_like_block",     148,  296,  180,  332, "clone"),
    ("vertical_text",    1739, 1564, 1753, 1638, "clone"),
    ("cyan_label_digits", 305,  425,  345,  500, "fill"),
    # rear face (the one the player sees): thin dotted columns that stretched faces blow up into text-like smears,
    # and an engraved dotted arc under the dial
    ("rear_column_a",    1194, 1232, 1204, 1334, "fill"),
    ("rear_column_b",    1221, 1278, 1228, 1338, "fill"),
    ("rear_top_column",  1240, 1915, 1252, 1996, "fill"),
    ("dial_arc",          638,  846,  704,  866, "fill"),
]
RING = 3          # px of surroundings compared when choosing a donor
SEARCH = 260      # px search radius for the donor
STEP = 2
GRAIN_RADIUS = 6   # box-blur radius whose residual is the "grain" used by mode "grain"

def box_blur(a, r):
    """Mean over a (2r+1)^2 window, edges clamped. a: (h, w, c)."""
    p = np.pad(a, ((r + 1, r), (r + 1, r), (0, 0)), mode="edge")
    s = p.cumsum(0).cumsum(1)
    k = 2 * r + 1
    return (s[k:, k:] - s[:-k, k:] - s[k:, :-k] + s[:-k, :-k]) / (k * k)

def load(prefix):
    im = next(i for i in bpy.data.images if i.name.startswith(prefix))
    w, h = im.size
    a = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1].copy()
    return im, a

def store(im, a):
    im.pixels.foreach_set(np.ascontiguousarray(a[::-1]).ravel())
    im.update()
    im.pack()

def overlaps(x0, y0, x1, y1, margin):
    for _, a0, b0, a1, b1, _ in REGIONS:
        if x0 < a1 + margin and x1 > a0 - margin and y0 < b1 + margin and y1 > b0 - margin:
            return True
    return False

def ring_mask(h, w):
    m = np.ones((h + 2 * RING, w + 2 * RING), bool)
    m[RING:-RING, RING:-RING] = False
    return m

def membrane(boundary_img, h, w, iters=2500):
    """Harmonic interpolation inside an h x w box whose 1-px outer frame is given by boundary_img ((h+2) x (w+2) x C)."""
    u = boundary_img.copy()
    # Coons-patch initial guess (bilinear blend of the four edges) speeds convergence.
    top, bot = u[0:1, 1:-1], u[-1:, 1:-1]
    lef, rig = u[1:-1, 0:1], u[1:-1, -1:]
    ty = np.linspace(0, 1, h + 2)[1:-1, None, None]
    tx = np.linspace(0, 1, w + 2)[None, 1:-1, None]
    guess = (1 - ty) * top + ty * bot + (1 - tx) * lef + tx * rig
    corners = ((1 - ty) * (1 - tx) * u[0, 0] + (1 - ty) * tx * u[0, -1] + ty * (1 - tx) * u[-1, 0] + ty * tx * u[-1, -1])
    u[1:-1, 1:-1] = guess - corners
    for _ in range(iters):
        u[1:-1, 1:-1] = 0.25 * (u[:-2, 1:-1] + u[2:, 1:-1] + u[1:-1, :-2] + u[1:-1, 2:])
    return u[1:-1, 1:-1]

im_c, C = load("Color_")
im_o, O = load("ORM_")
im_n, N = load("NormalGL_")
H, W = C.shape[:2]
mask_ring = None
log = []

for name, x0, y0, x1, y1, mode in REGIONS:
    h, w = y1 - y0, x1 - x0
    # frame = 1px ring just outside the region, clamped to the image
    fy0, fy1, fx0, fx1 = max(y0 - 1, 0), min(y1 + 1, H), max(x0 - 1, 0), min(x1 + 1, W)
    if mode in ("clone", "grain"):
        rm = ring_mask(h, w)
        ry0, rx0 = y0 - RING, x0 - RING
        def ring_pixels(img, oy, ox):
            sy0, sx0 = ry0 + oy, rx0 + ox
            blk = img[max(sy0, 0):sy0 + h + 2 * RING, max(sx0, 0):sx0 + w + 2 * RING]
            return blk
        CO = np.concatenate([C[..., :3], O[..., :3]], axis=2)  # match the ring in Color and ORM together
        tgt = ring_pixels(CO, 0, 0)
        valid = None
        if tgt.shape[:2] != rm.shape:  # region touches the border: compare only the part inside
            valid = np.zeros_like(rm)
            valid[:tgt.shape[0], :tgt.shape[1]] = True
        best = None
        for oy in range(-SEARCH, SEARCH + 1, STEP):
            for ox in range(-SEARCH, SEARCH + 1, STEP):
                if abs(oy) < h + 2 and abs(ox) < w + 2:
                    continue
                dy0, dx0, dy1, dx1 = y0 + oy, x0 + ox, y1 + oy, x1 + ox
                if dy0 - RING < 0 or dx0 - RING < 0 or dy1 + RING > H or dx1 + RING > W:
                    continue
                if overlaps(dx0, dy0, dx1, dy1, RING + 2):
                    continue
                d = CO[dy0 - RING:dy1 + RING, dx0 - RING:dx1 + RING]
                if valid is not None:
                    d = d[:tgt.shape[0], :tgt.shape[1]]
                    m = rm[:tgt.shape[0], :tgt.shape[1]]
                else:
                    m = rm
                if mode == "clone":
                    score = float(((d[m] - tgt[m]) ** 2).mean())
                else:
                    # "grain": only the donor's fine texture is used, so pick the smoothest block (largest local
                    # step small) whose mean brightness matches the surroundings.
                    lum = C[dy0:dy1, dx0:dx1, :3].mean(axis=2)
                    steps = max(float(np.abs(np.diff(lum, axis=0)).max()), float(np.abs(np.diff(lum, axis=1)).max()))
                    score = steps + abs(float(lum.mean()) - float(tgt[m][:, :3].mean()))
                if best is None or score < best[0]:
                    best = (score, oy, ox)
        score, oy, ox = best
        log.append(f"{name}: {mode} from offset dy={oy} dx={ox} ring_mse={score:.5f}")
    else:
        oy = ox = 0
        log.append(f"{name}: harmonic fill")

    for img, is_normal in ((C, False), (O, False), (N, True)):
        frame = img[fy0:fy1, fx0:fx1, :3].copy()
        # pad the frame to (h+2, w+2) when the region touches the image border
        frame = np.pad(frame, ((1 if y0 - 1 < 0 else 0, 1 if y1 + 1 > H else 0),
                               (1 if x0 - 1 < 0 else 0, 1 if x1 + 1 > W else 0), (0, 0)), mode="edge")
        if mode == "clone":
            donor = img[y0 + oy - 1:y1 + oy + 1, x0 + ox - 1:x1 + ox + 1, :3].copy()
            diff = frame - donor
            diff[1:-1, 1:-1] = 0
            corr = membrane(diff, h, w)
            new = donor[1:-1, 1:-1] + corr
        elif mode == "grain":
            # smooth fill from the surroundings plus soft isotropic noise whose strength matches the fine grain
            # measured in a 12 px band around the region (no donor shapes or streaks)
            B = 12
            band = img[max(y0 - B, 0):y1 + B, max(x0 - B, 0):x1 + B, :3]
            hp_band = band - box_blur(band, GRAIN_RADIUS)
            inner = np.ones(band.shape[:2], bool)
            inner[min(B, y0):min(B, y0) + h, min(B, x0):min(B, x0) + w] = False
            # robust (median-based) so UV-island edges in the band do not inflate it; one value for all channels
            sigma = np.full(3, 0.35 * 1.4826 * float(np.median(np.abs(hp_band[inner]))), np.float32)
            rng = np.random.default_rng(1234 + x0 * 7 + y0)
            noise = box_blur(rng.standard_normal((h, w, 1)).astype(np.float32), 1)
            noise /= max(float(noise.std()), 1e-6)
            new = membrane(frame, h, w) + noise * sigma[None, None, :]
        else:
            new = membrane(frame, h, w)
        if is_normal:
            v = new * 2 - 1
            v /= np.maximum(np.linalg.norm(v, axis=2, keepdims=True), 1e-6)
            new = v * 0.5 + 0.5
        img[y0:y1, x0:x1, :3] = np.clip(new, 0, 1)

store(im_c, C)
store(im_o, O)
store(im_n, N)

# Review images: each region with context, before/after is produced by running atlas_crop on both files.
for img, tag in ((C, "Color"), (O, "ORM"), (N, "NormalGL")):
    out = bpy.data.images.new("review_" + tag, W, H, alpha=True)
    out.colorspace_settings.name = "sRGB" if tag == "Color" else "Non-Color"
    out.pixels.foreach_set(np.ascontiguousarray(img[::-1]).ravel())
    out.filepath_raw = os.path.join(review_dir, f"{tag}_clean.png")
    out.file_format = "PNG"
    out.save()

bpy.ops.wm.save_as_mainfile(filepath=out_blend, compress=True)
with open(os.path.join(review_dir, "clean_log.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(log))
print("\n".join(log))
print("SAVED", out_blend)
