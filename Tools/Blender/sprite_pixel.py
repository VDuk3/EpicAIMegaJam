# Pixel-art reduction for the sprite baker (pure numpy, no bpy).
# Input: point-sampled supersampled passes of one view (region id, tone value, depth, specular) at F x the cell size.
# Output: one cell of palette indices (0 = transparent, 1 = outline black, others = ramp tones).
import numpy as np


def hex_rgb(h):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))


class Palette:
    """Ramps -> flat colour table. Index 0 transparent, 1 outline black, 2 specular white; then each ramp's tones."""

    def __init__(self, ramps, specular="#F0F0F0"):
        self.colors = [(0, 0, 0, 0), (0, 0, 0, 255), hex_rgb(specular) + (255,)]
        self.base = {}
        for name, tones in ramps.items():
            self.base[name] = len(self.colors)
            for t in tones:
                self.colors.append(hex_rgb(t) + (255,))
        self.size = {n: len(t) for n, t in ramps.items()}
        self.table = np.array(self.colors, dtype=np.uint8)


def block_reduce(regions, values, depth, spec, F, nreg, weights, min_cov, on_thr):
    """regions: (H*F, W*F) int (0 none). Returns per cell pixel: region, mean value, min depth, spec fraction, coverage."""
    Hs, Ws = regions.shape
    H, W = Hs // F, Ws // F
    r4 = regions.reshape(H, F, W, F).transpose(0, 2, 1, 3).reshape(H, W, F * F)
    v4 = values.reshape(H, F, W, F).transpose(0, 2, 1, 3).reshape(H, W, F * F)
    d4 = depth.reshape(H, F, W, F).transpose(0, 2, 1, 3).reshape(H, W, F * F)
    s4 = spec.reshape(H, F, W, F).transpose(0, 2, 1, 3).reshape(H, W, F * F)
    counts = np.zeros((nreg, H, W), np.float32)
    for r in range(1, nreg):
        counts[r] = (r4 == r).sum(axis=2)
    total = counts[1:].sum(axis=0) / (F * F)
    score = counts * weights[:, None, None]
    score[0] = 0
    reg = np.argmax(score, axis=0)
    on = total >= on_thr
    for r in range(1, nreg):
        if min_cov[r] > 0:
            on |= counts[r] >= min_cov[r] * F * F
    reg[~on] = 0
    sel = (r4 == reg[:, :, None]) & on[:, :, None]
    n = np.maximum(sel.sum(axis=2), 1)
    val = np.where(sel, v4, 0).sum(axis=2) / n
    dep = np.where(sel, d4, 1e9).min(axis=2)
    spc = np.where(sel, s4, 0).sum(axis=2) / n
    return reg, val, dep, spc, total


def neighbours4(a, fill=0):
    p = np.pad(a, 1, constant_values=fill)
    return p[:-2, 1:-1], p[2:, 1:-1], p[1:-1, :-2], p[1:-1, 2:]


def neighbours8(a, fill=0):
    p = np.pad(a, 1, constant_values=fill)
    return [p[:-2, 1:-1], p[2:, 1:-1], p[1:-1, :-2], p[1:-1, 2:], p[:-2, :-2], p[:-2, 2:], p[2:, :-2], p[2:, 2:]]


def label_islands(mask):
    """4-connected components of a bool mask. Returns (labels, sizes)."""
    H, W = mask.shape
    lab = np.zeros((H, W), np.int32)
    sizes = [0]
    cur = 0
    ys, xs = np.nonzero(mask)
    for y0, x0 in zip(ys, xs):
        if lab[y0, x0]:
            continue
        cur += 1
        stack = [(y0, x0)]
        lab[y0, x0] = cur
        n = 0
        while stack:
            y, x = stack.pop()
            n += 1
            for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= yy < H and 0 <= xx < W and mask[yy, xx] and not lab[yy, xx]:
                    lab[yy, xx] = cur
                    stack.append((yy, xx))
        sizes.append(n)
    return lab, np.array(sizes)


def clean_mask(reg, min_island, max_hole):
    """Drops small islands of the silhouette, fills small holes (taking the neighbours' majority region)."""
    on = reg > 0
    lab, sizes = label_islands(on)
    if len(sizes) > 1:
        keep = sizes >= min_island
        keep[np.argmax(sizes)] = True
        keep[0] = False
        reg = np.where(keep[lab], reg, 0)
    off = reg == 0
    lab, sizes = label_islands(off)
    H, W = reg.shape
    border = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]])).tolist())
    for i in range(1, len(sizes)):
        if i in border or sizes[i] > max_hole:
            continue
        ys, xs = np.nonzero(lab == i)
        for y, x in zip(ys, xs):
            cand = [reg[yy, xx] for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1))
                    if 0 <= yy < H and 0 <= xx < W and reg[yy, xx] > 0]
            if cand:
                reg[y, x] = max(set(cand), key=cand.count)
    return reg


def close_region(reg, val, dep, r, passes=1):
    """Morphological closing of region r into empty pixels only (fills the gaps between thin feathers or strands so
    the part reads as one solid shape). Filled pixels take the mean value and the nearest depth of their neighbours."""
    m = reg == r
    d = m.copy()
    for _ in range(passes):
        d = d | np.logical_or.reduce(neighbours8(d, False))
    e = d.copy()
    for _ in range(passes):
        e = e & np.logical_and.reduce(neighbours8(e, True))
    fill = e & (reg == 0)
    if fill.any():
        nv = neighbours8(np.where(m, val, 0.0), 0.0)
        nm = neighbours8(m.astype(np.float32), 0.0)
        nd = neighbours8(np.where(m, dep, 1e9), 1e9)
        cnt = sum(nm)
        mean = sum(nv) / np.maximum(cnt, 1)
        near = np.minimum.reduce(nd)
        reg = reg.copy()
        val = val.copy()
        dep = dep.copy()
        reg[fill] = r
        val[fill] = np.where(cnt[fill] > 0, mean[fill], val[fill])
        dep[fill] = np.where(cnt[fill] > 0, near[fill], dep[fill])
    return reg, val, dep


def _majority(vals):
    return max(set(vals), key=vals.count) if vals else None


def open_region(reg, r):
    """Morphological opening of region r (3x3): removes 1-2 px spurs and specks so the part is made of solid blocks.
    Removed pixels take the majority non-r neighbour region (0 at the silhouette edge)."""
    m = reg == r
    e = m & np.logical_and.reduce(neighbours8(m, False))
    o = e | (np.logical_or.reduce(neighbours8(e, False)) & m)
    gone = m & ~o
    if gone.any():
        reg = reg.copy()
        n8 = neighbours8(reg, 0)
        for y, x in zip(*np.nonzero(gone)):
            cand = [n[y, x] for n in n8 if n[y, x] != r]
            reg[y, x] = _majority(cand) if cand else 0
    return reg


def absorb_into_region(reg, val, dep, r, max_px, protect):
    """Connected components (of empty pixels or of other regions) of at most max_px pixels whose whole border is
    region r become region r: holes and foreign specks inside a wing disappear."""
    H, W = reg.shape
    m = reg == r
    other = ~m
    lab, sizes = label_islands(other)
    nv = None
    for i in range(1, len(sizes)):
        if sizes[i] > max_px:
            continue
        ys, xs = np.nonzero(lab == i)
        if ((ys == 0) | (ys == H - 1) | (xs == 0) | (xs == W - 1)).any():
            continue
        if np.isin(reg[ys, xs], protect).any():
            continue
        border = set()
        for y, x in zip(ys, xs):
            for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if lab[yy, xx] != i:
                    border.add(int(reg[yy, xx]))
        if border == {r}:
            if nv is None:
                reg, val, dep = reg.copy(), val.copy(), dep.copy()
                mv = np.where(m, val, 0.0)
                cnt = sum(neighbours8(m.astype(np.float32), 0.0))
                nv = sum(neighbours8(mv, 0.0)) / np.maximum(cnt, 1)
                nd = np.minimum.reduce(neighbours8(np.where(m, dep, 1e9), 1e9))
            reg[ys, xs] = r
            val[ys, xs] = nv[ys, xs]
            dep[ys, xs] = nd[ys, xs]
    return reg, val, dep


def mode_tones(tone, reg, r, passes=2):
    """Majority filter of tones inside region r (3x3, same-region pixels): flat bands, no salt and pepper."""
    m = reg == r
    for _ in range(passes):
        nt = neighbours8(tone, -1)
        nr = neighbours8(reg, -1)
        new = tone.copy()
        for y, x in zip(*np.nonzero(m)):
            vals = [nt[i][y, x] for i in range(8) if nr[i][y, x] == r] + [tone[y, x]]
            new[y, x] = _majority(vals)
        tone = new
    return tone


def merge_small_bands(tone, reg, r, min_px):
    """Tone islands inside region r smaller than min_px take the majority tone around them."""
    m = reg == r
    for t in np.unique(tone[m]):
        lab, sizes = label_islands(m & (tone == t))
        for i in range(1, len(sizes)):
            if sizes[i] >= min_px:
                continue
            ys, xs = np.nonzero(lab == i)
            around = []
            for y, x in zip(ys, xs):
                for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                    if 0 <= yy < m.shape[0] and 0 <= xx < m.shape[1] and m[yy, xx] and lab[yy, xx] != i:
                        around.append(int(tone[yy, xx]))
            if around:
                tone[ys, xs] = _majority(around)
    return tone


def clean_region_specks(reg, protect):
    """A pixel whose region has no 8-neighbour of the same region takes the majority region of its 4 neighbours.
    Regions in `protect` (thin parts such as weapons) are kept if they have at least one same-region neighbour."""
    for _ in range(2):
        nb8 = neighbours8(reg)
        same = sum((n == reg).astype(np.int32) for n in nb8)
        nb4 = neighbours4(reg)
        lone = (reg > 0) & (same == 0) & ~np.isin(reg, protect)
        if not lone.any():
            break
        ys, xs = np.nonzero(lone)
        for y, x in zip(ys, xs):
            cand = [n[y, x] for n in nb4 if n[y, x] > 0]
            if cand:
                reg[y, x] = max(set(cand), key=cand.count)
    return reg


def smooth_tones(tone, reg, passes=2):
    """Within a region, a tone pixel that differs from all its same-region 4-neighbours, while at least two of them
    agree, takes their tone (kills single-pixel specks and 1-px tone holes)."""
    H, W = tone.shape
    for _ in range(passes):
        changed = False
        nt = neighbours4(tone, -1)
        nr = neighbours4(reg, -1)
        new = tone.copy()
        for y, x in zip(*np.nonzero(reg > 0)):
            r = reg[y, x]
            t = tone[y, x]
            vals = [nt[i][y, x] for i in range(4) if nr[i][y, x] == r]
            if len(vals) < 2 or t in vals:
                continue
            best = max(set(vals), key=vals.count)
            if vals.count(best) >= 2:
                new[y, x] = best
                changed = True
        tone = new
        if not changed:
            break
    return tone


def quantize(val, reg, region_ramp_n, region_range, prev_tone=None, prev_val=None, hyst=0.0):
    """Maps each pixel's tone value to an index in its region's ramp, with optional hysteresis against the previous
    frame (a pixel that stays near a threshold keeps last frame's tone)."""
    H, W = val.shape
    tone = np.zeros((H, W), np.int32)
    for r, n in region_ramp_n.items():
        m = reg == r
        if not m.any():
            continue
        lo, hi = region_range[r]
        x = (val[m] - lo) / max(hi - lo, 1e-6) * n
        t = np.clip(np.floor(x), 0, n - 1).astype(np.int32)
        if prev_tone is not None and hyst > 0:
            pt = prev_tone[m]
            near = (np.abs(t - pt) == 1)
            # distance of x from the boundary between t and pt
            bnd = np.maximum(t, pt).astype(np.float32)
            close = np.abs(x - bnd) < hyst
            t = np.where(near & close, pt, t)
        tone[m] = t
    return tone


def inner_outline(reg, dep, tone, threshold, regions_ok):
    """Pixels whose 4-neighbour is clearly nearer (an overlapping part in front) get their ramp's darkest tone."""
    nd = neighbours4(dep, 1e9)
    nr = neighbours4(reg, 0)
    edge = np.zeros(reg.shape, bool)
    for d, r in zip(nd, nr):
        edge |= (r > 0) & (d < dep - threshold)
    edge &= (reg > 0) & np.isin(reg, regions_ok)
    # thin it: never darken a pixel whose own region is 1 px wide there (both opposite neighbours off-region)
    n = neighbours4(reg, 0)
    thin = ((n[0] != reg) & (n[1] != reg)) | ((n[2] != reg) & (n[3] != reg))
    edge &= ~thin
    return np.where(edge, 0, tone), edge


def despeckle_index(idx, reg, protect):
    """Final pass on palette indices: a colour pixel with no 8-neighbour of its own colour takes the most common
    colour among its 8-neighbours of the same region (protected regions such as eyes keep their single pixels)."""
    for _ in range(2):
        nb = neighbours8(idx, -1)
        nr = neighbours8(reg, -1)
        same = sum((n == idx).astype(np.int32) for n in nb)
        lone = (idx > 2) & (same == 0) & ~np.isin(reg, protect)
        if not lone.any():
            break
        out = idx.copy()
        for y, x in zip(*np.nonzero(lone)):
            cand = [nb[i][y, x] for i in range(8) if nr[i][y, x] == reg[y, x] and nb[i][y, x] > 2]
            if cand:
                out[y, x] = max(set(cand), key=cand.count)
        idx = out
    return idx


def fill_pinholes(idx, bare=None):
    """A transparent pixel closed on all 4 sides becomes outline black (not next to a bare region: gaps between
    feathers stay open)."""
    on = idx > 0
    n = neighbours4(on, False)
    hole = (~on) & n[0] & n[1] & n[2] & n[3]
    if bare is not None:
        nb = neighbours4(bare, False)
        hole &= ~(nb[0] | nb[1] | nb[2] | nb[3])
    out = idx.copy()
    out[hole] = 1
    return out


def outer_outline(idx, bare=None):
    """1 px black outline, 4-connected, outside the silhouette. idx 0 = transparent, 1 = black.
    bare: pixels of regions drawn without an outline (ethereal parts such as feathered wings); the ring is only
    drawn next to the other pixels."""
    on = idx > 0
    drawn = on if bare is None else (on & ~bare)
    n = neighbours4(drawn, False)
    ring = (~on) & (n[0] | n[1] | n[2] | n[3])
    out = idx.copy()
    out[ring] = 1
    return out


def metrics(cells, loop, ramp_of_index):
    """cells: list of (H, W) palette-index frames of one clip and direction. Returns a dict of defect counts."""
    iso = holes = border = 0
    flick = []
    for c in cells:
        on = c > 0
        nb = neighbours8(c, -1)
        same = sum((n == c).astype(np.int32) for n in nb)
        iso += int(((c > 1) & (same == 0)).sum())
        n4 = neighbours4(on, False)
        holes += int(((~on) & n4[0] & n4[1] & n4[2] & n4[3]).sum())
        border += int(on[0].sum() + on[-1].sum() + on[:, 0].sum() + on[:, -1].sum())
    pairs = list(zip(cells[:-1], cells[1:]))
    if loop and len(cells) > 2:
        pairs.append((cells[-1], cells[0]))
    for a, b in pairs:
        both = (a > 1) & (b > 1)
        ra, rb = ramp_of_index[a], ramp_of_index[b]
        ch = both & (ra == rb) & (a != b)
        flick.append(float(ch.sum()) / max(int(both.sum()), 1))
    closure = None
    if loop and len(cells) > 2:
        diffs = [float((x != y).sum()) for x, y in zip(cells[:-1], cells[1:])]
        closure = float((cells[-1] != cells[0]).sum()) / max(np.mean(diffs), 1.0)
    return {"isolated": iso, "holes": holes, "border": border,
            "tone_change_max": round(max(flick), 3) if flick else 0.0, "loop_seam": round(closure, 2) if closure else None}


def scale_nearest(img, k):
    return np.repeat(np.repeat(img, k, axis=0), k, axis=1)


# 3x5 bitmap font for labels (uppercase, digits, a few signs)
_FONT = {
    "A": "010101111101101", "B": "110101110101110", "C": "011100100100011", "D": "110101101101110",
    "E": "111100110100111", "F": "111100110100100", "G": "011100101101011", "H": "101101111101101",
    "I": "111010010010111", "J": "001001001101010", "K": "101101110101101", "L": "100100100100111",
    "M": "101111111101101", "N": "110101101101101", "O": "010101101101010", "P": "110101110100100",
    "Q": "010101101110011", "R": "110101110101101", "S": "011100010001110", "T": "111010010010010",
    "U": "101101101101111", "V": "101101101101010", "W": "101101111111101", "X": "101101010101101",
    "Y": "101101010010010", "Z": "111001010100111", "0": "111101101101111", "1": "010110010010111",
    "2": "110001010100111", "3": "110001010001110", "4": "101101111001001", "5": "111100110001110",
    "6": "011100111101111", "7": "111001010010010", "8": "111101111101111", "9": "111101111001110",
    "=": "000111000111000", "-": "000000111000000", " ": "000000000000000", ":": "000010000010000",
    "_": "000000000000111", "/": "001001010100100",
}


def draw_text(img, x, y, text, color, scale=2):
    for ch in text.upper():
        g = _FONT.get(ch, _FONT[" "])
        for i, b in enumerate(g):
            if b == "1":
                yy = y + (i // 3) * scale
                xx = x + (i % 3) * scale
                img[yy:yy + scale, xx:xx + scale] = color
        x += 4 * scale
    return img
