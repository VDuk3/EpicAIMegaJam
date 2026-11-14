# Orc armor: the fit. Executed by enemies_export_fbx.py (it uses that file's helpers: humanoid, islands,
# split_by_verts, fit_boot, wrap_outside, to_cm, export_skeletal ...). Everything here works in metres.
#
# What the line-ups really are (read off contact sheets of every piece, Tools/Blender/orc_armor_pieces.py):
#   - the pieces stand in the pose of the body's RIGHT side: a tube's top is up, its high edge is on the outer side,
#     and the gauntlet lies along the direction of a right forearm with the back of the hand to the front;
#   - thigh plates, upper-arm plates and greaves are closed TUBES, not open shells;
#   - the "greaves" of Armor 2 are tall boots (they have a foot); the foot is cut off, the boot of Armor 1 has it;
#   - the round pads have a U-shaped strap behind the dome: they clasp the limb they sit on;
#   - set A and set C carry a knee piece under each thigh tube.
# The fit, therefore: every piece keeps its shape. A tube is turned from "pointing down" to its bone by the shortest
# rotation (nothing is turned upside down), scaled as a whole until the limb is inside it, and bound to ONE bone, so
# it moves like a plate and does not stretch. The first version scaled each piece per axis onto a box of the body,
# pushed every vertex out of the skin one by one and skinned it like cloth: torn edges, skirts, funnels.
import numpy as np
from mathutils.bvhtree import BVHTree

HANDY = ("hand", "index", "middle", "ring", "pinky", "thumb")
# per set and slot: hand-set numbers, each one read off a review render (armor_review_render.py)
TUNE = {
    ("A", "Helmet"): {"s": 2.75}, ("B", "Helmet"): {"s": 2.75}, ("C", "Helmet"): {"s": 3.15}, ("D", "Helmet"): {"s": 2.75},
    ("D", "Chest"): {"top": 1.87},
    ("A", "Shin"): {"cut": 0.38}, ("B", "Shin"): {"cut": 0.20}, ("C", "Shin"): {"cut": 0.25}, ("D", "Shin"): {"cut": 0.36},
    ("A", "Knee"): {"kind": "tube"}, ("C", "Knee"): {"kind": "cap"},
}


def _bh(arm, name):
    return arm.matrix_world @ arm.data.bones[name].head_local


def _bt(arm, name):
    return arm.matrix_world @ arm.data.bones[name].tail_local


def _pts(body, dom, pred):
    wm = body.matrix_world
    return [wm @ body.data.vertices[i].co for i, b in dom.items() if pred(b)]


def _rigid(piece, bone):
    piece.vertex_groups.clear()
    piece.vertex_groups.new(name=bone).add(list(range(len(piece.data.vertices))), 1.0, "REPLACE")


def _set_weights(piece, per_vert):
    piece.vertex_groups.clear()
    groups = {}
    for i, ws in enumerate(per_vert):
        tot = sum(ws.values())
        for b, w in ws.items():
            if w / tot > 0.01:
                if b not in groups:
                    groups[b] = piece.vertex_groups.new(name=b)
                groups[b].add([i], w / tot, "REPLACE")


def _piece_tree(piece):
    return BVHTree.FromPolygons([v.co.copy() for v in piece.data.vertices], [tuple(p.vertices) for p in piece.data.polygons])


def _part_tree(body, dom, pred):
    """BVH of the body's faces whose corners all belong to bones accepted by pred (a limb, the torso)."""
    wm = body.matrix_world
    faces = [p.vertices for p in body.data.polygons if all(pred(dom.get(i, "")) for i in p.vertices)]
    idx = sorted({i for f in faces for i in f})
    remap = {i: k for k, i in enumerate(idx)}
    return BVHTree.FromPolygons([wm @ body.data.vertices[i].co for i in idx], [tuple(remap[i] for i in f) for f in faces])


def _sunk(piece, tree, depth=0.004, sel=None):
    """Share of the piece's vertices that lie under the surface of that part of the body (deeper than depth)."""
    vs = [v.co for v in piece.data.vertices] if sel is None else [piece.data.vertices[i].co for i in sel]
    n = 0
    for p in vs:
        loc, nrm, idx, dist = tree.find_nearest(p, 0.12)
        if loc is not None and (p - loc).dot(nrm) < -depth:
            n += 1
    return n / max(1, len(vs))


def _inside_body(tree, p):
    loc, nrm, idx, dist = tree.find_nearest(p)
    return loc is not None and (p - loc).dot(nrm) < 0


def _cut_below(piece, frac):
    """Removes the part of a piece below `frac` of its height (the foot of a greave that is really a tall boot)."""
    zs = [v.co.z for v in piece.data.vertices]
    zc = min(zs) + (max(zs) - min(zs)) * frac
    bm = bmesh.new()
    bm.from_mesh(piece.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < zc], context="VERTS")
    bm.to_mesh(piece.data)
    bm.free()
    piece.data.update()


# ------------------------------------------------------------------------------------------------------ tubes
def fit_tube(piece, body, arm, sd, bone_a, bone_b, pts, tree, top_t, max_len, margin=1.10, bind=None, part=(0.0, 1.0)):
    """A closed tube round a limb. The piece stands as in the line-up (top up); it is turned so that "down" runs
    along the bone, its top put top_t metres past the bone's head, and scaled (cross-section to the limb's own
    cross-section over the covered stretch times `margin`, length to at most max_len) and then grown in steps of 4 %
    until no more than 2 % of its vertices are under the limb's skin. (Asking instead that every point of the limb
    see the tube around it failed: the tubes' tops are cut on a slant and the test blew them up to 1.4 times.)
    `part`: the stretch of the tube's length, from its top, that is measured and tested. A thigh tube sized on the
    buttock at its top, or an upper-arm tube on the deltoid, came out as a barrel; they are sized below it."""
    A, Bn = _bh(arm, bone_a + sd), _bh(arm, bone_b + sd)
    ax = (Bn - A).normalized()
    R = Vector((0, 0, -1)).rotation_difference(ax).to_matrix()
    e1, e2 = R @ Vector((1, 0, 0)), R @ Vector((0, 1, 0))
    src_v = [v.co.copy() for v in piece.data.vertices]
    pmin = Vector([min(v[k] for v in src_v) for k in range(3)])
    pmax = Vector([max(v[k] for v in src_v) for k in range(3)])
    psize, pc = pmax - pmin, (pmin + pmax) / 2
    loc = [((p - A).dot(e1), (p - A).dot(e2), (p - A).dot(ax)) for p in pts]

    def section(t0, t1):
        sel = [q for q in loc if t0 <= q[2] <= t1]
        if len(sel) < 8:
            sel = loc
        us, ws = [q[0] for q in sel], [q[1] for q in sel]
        return _pct(us, 0.02), _pct(us, 0.98), _pct(ws, 0.02), _pct(ws, 0.98)

    st = max_len / psize.z
    for _ in range(3):
        u0, u1, w0, w1 = section(top_t + part[0] * psize.z * st, top_t + part[1] * psize.z * st)
        s1, s2 = (u1 - u0) * margin / psize.x, (w1 - w0) * margin / psize.y
        g = (s1 * s2) ** 0.5
        s1, s2 = max(g * 0.85, min(g * 1.18, s1)), max(g * 0.85, min(g * 1.18, s2))
        st = min(g, max_len / psize.z)
    cu, cw = (u0 + u1) / 2, (w0 + w1) / 2
    L = psize.z * st
    tested = [i for i, s in enumerate(src_v) if part[0] <= (pmax.z - s.z) / psize.z <= part[1]]
    k = 1.0
    for _ in range(12):
        for v, s in zip(piece.data.vertices, src_v):
            v.co = (A + e1 * (cu + (s.x - pc.x) * s1 * k) + e2 * (cw + (s.y - pc.y) * s2 * k)
                    + ax * (top_t + (pmax.z - s.z) * st))
        piece.data.update()
        out = _sunk(piece, tree, sel=tested)
        if out < 0.02:
            break
        k *= 1.03
    if bind is None:
        _rigid(piece, bone_a + sd)
    else:
        _set_weights(piece, [dict(bind) for _ in piece.data.vertices])
    return {"scale": [round(s1 * k, 2), round(s2 * k, 2), round(st, 2)], "len_cm": round(L * 100, 1), "sunk": round(out, 3)}


# ------------------------------------------------------------------------------------------------ shoulder caps
def fit_shoulder(piece, body, arm, sd, width=0.30, down=0.09, lift=0.12):
    """The round pad: its strap clasps the top of the upper arm, its dome looks straight out of the deltoid. Moved
    out along that direction until the dome is clear of the body."""
    S, E = _bh(arm, "upperarm" + sd), _bh(arm, "lowerarm" + sd)
    ax = (E - S).normalized()
    ex = Vector((-1.0 if sd == "_r" else 1.0, 0, 0))
    out_dir_ = (ex + Vector((0, 0, lift))).normalized()           # out of the side of the deltoid, a little up
    out_dir_ = (out_dir_ - ax * out_dir_.dot(ax) * 0.5).normalized()
    R1 = Vector((0, 0, -1)).rotation_difference(ax).to_matrix()
    f = R1 @ Vector((0, -1, 0))
    ang = math.atan2(ax.dot(f.cross(out_dir_)), f.dot(out_dir_))
    R = Matrix.Rotation(ang, 3, ax) @ R1
    src_v = [v.co.copy() for v in piece.data.vertices]
    xs = [v.x for v in src_v]
    s = width / (max(xs) - min(xs))
    tree = _body_tree(body)
    dome = [i for i, v in enumerate(src_v) if v.y < 0]
    off = 0.02
    while off < 0.30:
        c = S + ax * down + out_dir_ * off
        inside = sum(1 for i in dome if _inside_body(tree, c + R @ (src_v[i] * s)))
        if inside <= 0.03 * len(dome):
            break
        off += 0.01
    for v, p in zip(piece.data.vertices, src_v):
        v.co = c + R @ (p * s)
    piece.data.update()
    _rigid(piece, "upperarm" + sd)
    return {"scale": round(s, 2), "out_cm": round(off * 100, 1)}


# ------------------------------------------------------------------------------------------------------ cuirass
def fit_chest(piece, body, arm, dom, top=1.80, bottom=1.16, mx=1.08, my=1.08, band=(0.25, 0.60)):
    """The cuirass: one scale per axis (this orc's torso is 60 cm wide, 50 deep and only 50 tall from belt to
    shoulder; the cuirasses were drawn for a narrower, taller chest), bottom edge on the belt, grown until the chest
    is inside. Bound to the spine bones by height only, never to the arms."""
    tor = _pts(body, dom, lambda b: b.split("_")[0] in ("pelvis", "spine", "neck") or b.startswith("clavicle"))
    src_v = [v.co.copy() for v in piece.data.vertices]
    pz0, pz1 = min(v.z for v in src_v), max(v.z for v in src_v)
    # the same band on both: 25 % to 60 % of the height, under the arm holes (shoulder spikes and wings are above)
    lo, hi = band
    bnd = [p for p in tor if bottom + lo * (top - bottom) <= p.z <= bottom + hi * (top - bottom) and abs(p.x) < 0.34]
    pb = [v for v in src_v if pz0 + lo * (pz1 - pz0) <= v.z <= pz0 + hi * (pz1 - pz0)]
    xs, ys = [p.x for p in bnd], [p.y for p in bnd]
    x0, x1, y0, y1 = _pct(xs, 0.01), _pct(xs, 0.99), _pct(ys, 0.01), _pct(ys, 0.99)
    px0, px1 = _pct([v.x for v in pb], 0.01), _pct([v.x for v in pb], 0.99)
    py0, py1 = _pct([v.y for v in pb], 0.03), _pct([v.y for v in pb], 0.97)
    sx, sy, sz = (x1 - x0) * mx / (px1 - px0), (y1 - y0) * my / (py1 - py0), (top - bottom) / (pz1 - pz0)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    tree = _part_tree(body, dom, lambda b: b.split("_")[0] in ("pelvis", "spine", "neck") or b.startswith("clavicle"))
    k = 1.0
    for _ in range(10):
        for v, s in zip(piece.data.vertices, src_v):
            v.co = Vector((cx + (s.x - (px0 + px1) / 2) * sx * k, cy + (s.y - (py0 + py1) / 2) * sy * k, bottom + (s.z - pz0) * sz))
        piece.data.update()
        out = _sunk(piece, tree, sel=[i for i, v in enumerate(piece.data.vertices) if v.co.z < bottom + 0.75 * (top - bottom)])
        if out < 0.03:
            break
        k *= 1.025
    sp = ["spine_01", "spine_02", "spine_03", "spine_04", "spine_05"]
    heads = [_bh(arm, b).z for b in sp]
    mids = [(heads[i] + heads[i + 1]) / 2 for i in range(4)] + [heads[4] + 0.04]
    per = []
    for v in piece.data.vertices:
        z = v.co.z
        if z <= mids[0]:
            per.append({sp[0]: 1.0})
        elif z >= mids[4]:
            per.append({sp[4]: 1.0})
        else:
            i = max(j for j in range(4) if mids[j] <= z)
            u = (z - mids[i]) / (mids[i + 1] - mids[i])
            per.append({sp[i]: 1 - u, sp[i + 1]: u})
    _set_weights(piece, per)
    return {"scale": [round(sx * k, 2), round(sy * k, 2), round(sz, 2)], "sunk": round(out, 3)}


# ------------------------------------------------------------------------------------------------------- helmet
def fit_helmet(piece, body, arm, dom, s=2.75, dy=0.0, dz=0.0):
    """Upright on the skull (the first fit laid it along the neck bone, which leans forward: every helmet looked
    at the floor), one scale, bound to the head bone. The dome is centred over the cranium; crests, spikes and the
    halo are left out of the measure."""
    head = _pts(body, dom, lambda b: b == "head")
    top = max(p.z for p in head)
    cran = [p for p in head if p.z > top - 0.20]
    hx = (_pct([p.x for p in cran], 0.02) + _pct([p.x for p in cran], 0.98)) / 2
    hy = (_pct([p.y for p in cran], 0.02) + _pct([p.y for p in cran], 0.98)) / 2
    src_v = [v.co.copy() for v in piece.data.vertices]
    zt = _pct([v.z for v in src_v], 0.97)
    up = [v for v in src_v if zt - 0.20 / s < v.z <= zt]
    px = (_pct([v.x for v in up], 0.05) + _pct([v.x for v in up], 0.95)) / 2
    py = (_pct([v.y for v in up], 0.05) + _pct([v.y for v in up], 0.95)) / 2
    for v, p in zip(piece.data.vertices, src_v):
        v.co = Vector((hx + (p.x - px) * s, hy + dy + (p.y - py) * s, top + 0.02 + dz + (p.z - zt) * s))
    piece.data.update()
    _rigid(piece, "head")
    return {"scale": s, "top_cm": round((top + 0.02 + dz) * 100, 1)}


# --------------------------------------------------------------------------------------------------------- belt
BELT_TOP = 1.20     # m: top edge of the belt (the orc's waist; its pelvis joint is at 1.12)


def fit_belt(piece, body, dom):
    """Belt + codpiece: the ring is sized to the orc's waist (a slab of the torso bones, arms excluded) and hung from
    BELT_TOP; the codpiece follows the same transform. Bound to the pelvis alone: the legs move under it."""
    wm = body.matrix_world
    pts = [wm @ body.data.vertices[i].co for i, b in dom.items()
           if b.split("_")[0] in ("pelvis", "spine", "thigh") and BELT_TOP - 0.20 < (wm @ body.data.vertices[i].co).z < BELT_TOP
           and abs((wm @ body.data.vertices[i].co).x) < 0.40]
    xs = [p.x for p in pts]
    ys = [p.y for p in pts]
    bx0, bx1, by0, by1 = _pct(xs, 0.02), _pct(xs, 0.98), min(ys), max(ys)
    pv = [v.co for v in piece.data.vertices]
    pmin = Vector((min(v.x for v in pv), min(v.y for v in pv), min(v.z for v in pv)))
    pmax = Vector((max(v.x for v in pv), max(v.y for v in pv), max(v.z for v in pv)))
    sx = (bx1 - bx0) * 1.12 / (pmax.x - pmin.x)
    sy = (by1 - by0) * 1.14 / (pmax.y - pmin.y)
    sz = (sx * sy) ** 0.5
    for v in piece.data.vertices:
        c = v.co
        v.co = Vector(((c.x - (pmin.x + pmax.x) / 2) * sx + (bx0 + bx1) / 2,
                       (c.y - (pmin.y + pmax.y) / 2) * sy + (by0 + by1) / 2,
                       (c.z - pmax.z) * sz + BELT_TOP))
    piece.data.update()
    left = wrap_outside(piece, body, pts, max_depth=0.04)
    _rigid(piece, "pelvis")
    return {"scale": [round(sx, 3), round(sy, 3), round(sz, 3)], "verts_pushed_out": left}


# --------------------------------------------------------------------------------------------------------- knee
def fit_knee_cap(piece, body, arm, dom, sd, width=0.16):
    """A cap on the front of the knee (set C's skull)."""
    K = _bh(arm, "calf" + sd)
    near = [p for p in _pts(body, dom, lambda b: b.startswith(("thigh", "calf")) and b.endswith(sd)) if abs(p.z - K.z) < 0.05]
    yf = min(p.y for p in near)
    cx = (min(p.x for p in near) + max(p.x for p in near)) / 2
    src_v = [v.co.copy() for v in piece.data.vertices]
    s = width / (max(v.x for v in src_v) - min(v.x for v in src_v))
    yb = max(v.y for v in src_v)
    for v, p in zip(piece.data.vertices, src_v):
        v.co = Vector((cx + p.x * s, yf + 0.012 + (p.y - yb) * s, K.z + p.z * s))
    piece.data.update()
    _set_weights(piece, [{"thigh" + sd: 0.5, "calf" + sd: 0.5} for _ in piece.data.vertices])
    return {"scale": round(s, 2)}


# ----------------------------------------------------------------------------------------------------- gauntlet
def _frame(a, n):
    a = a.normalized()
    n = (n - a * n.dot(a)).normalized()
    return Matrix((a, n, a.cross(n))).transposed()


def fit_gauntlet(piece, body, arm, dom, sd, s=None, hand_margin=1.05, cuff_share=0.80):
    """The gauntlet is a glove with a long cuff. The first fit boxed it onto forearm and hand along the bone and so
    put it on backwards: fingers up the forearm, the cuff's mouth open round the hand.
    Here it is read as two parts joined at its narrowest section (the wrist): the cuff is laid along the forearm
    (shortest rotation; the line-up gauntlet already lies like a right forearm) and widened station by station until
    the forearm is inside; the hand part is laid on the hand's own frame (wrist to knuckles, palm plane). The wrists
    coincide. Skin: cuff to the forearm bone, the hand part to the hand and finger bones nearest to each vertex, so
    the glove's fingers close with the orc's. The body is worn WITHOUT its hands under a gauntlet (SK_Orc_NoHands)."""
    X = np.array([v.co[:] for v in piece.data.vertices], dtype=float)
    c = X.mean(0)
    ax = np.linalg.svd(X - c, full_matrices=False)[2][0]
    if ax[2] > 0:
        ax = -ax                                     # from the cuff (up) to the fingers (down)
    t = (X - c) @ ax
    tmin, Lg = t.min(), t.max() - t.min()
    e1 = np.cross(ax, [0.0, 1.0, 0.0])
    e1 /= np.linalg.norm(e1)
    e2 = np.cross(ax, e1)
    NB = 24
    bins = []
    for k in range(NB):
        sel = (t >= tmin + Lg * k / NB) & (t <= tmin + Lg * (k + 1) / NB)
        if sel.sum() < 4:
            bins.append(None)
            continue
        q = X[sel] - c
        u, w = q @ e1, q @ e2
        bins.append((u.max() - u.min(), w.max() - w.min(), (u.max() + u.min()) / 2, (w.max() + w.min()) / 2))
    cand = [k for k in range(int(NB * 0.40), int(NB * 0.66)) if bins[k]]
    kw = min(cand, key=lambda k: bins[k][0] * bins[k][1])
    tw = tmin + Lg * (kw + 0.5) / NB

    def centre(k):
        return Vector(c + ax * (tmin + Lg * (k + 0.5) / NB) + e1 * bins[k][2] + e2 * bins[k][3])

    Wg = centre(kw)
    k0 = next(k for k in range(1, NB) if bins[k])
    cg = (Wg - centre(k0)).normalized()
    hand_len_g = tmin + Lg - tw
    wrist_g = (bins[kw][0] + bins[kw][1]) / 2
    # the orc's arm
    W, E = _bh(arm, "hand" + sd), _bh(arm, "lowerarm" + sd)
    co = (W - E).normalized()
    f1 = (Vector((0, 0, 1)) - co * co.z).normalized()
    f2 = co.cross(f1)
    fore = _pts(body, dom, lambda b: b.startswith("lowerarm") and b.endswith(sd))
    tip = max(_pts(body, dom, lambda b: b.startswith("middle") and b.endswith(sd)), key=lambda p: (p - W).length)
    if s is None:                                   # the glove's hand as long as the orc's (claws make it longer)
        s = max(2.3, min(3.1, hand_margin * (tip - W).length / hand_len_g))
    s = float(s)
    # The glove's hand is laid ON the orc's hand: both are hands, so the glove's hand part is registered to the
    # hand's surface (start: principal axes, long axis away from the wrist, both ways up tried; then closest points
    # and the best rigid motion, the farthest 30 % left out each round: claws, cuffs' rims). Taking the hand's
    # direction from the glove's own shape instead (wrist to palm centre) was off by 40 to 60 degrees.
    is_hand = lambda b: b.startswith(HANDY) and b.endswith(sd)
    hand_tree = _part_tree(body, dom, is_hand)
    Hp = np.array([p[:] for p in _pts(body, dom, is_hand)])
    G = (X[t > tw] - np.array(Wg)) * s
    gc, oc = G.mean(0), Hp.mean(0)
    gv = np.linalg.svd(G - gc, full_matrices=False)[2]
    ov = np.linalg.svd(Hp - oc, full_matrices=False)[2]
    a_g = gv[0] if gv[0] @ gc > 0 else -gv[0]
    a_o = ov[0] if ov[0] @ (oc - np.array(W)) > 0 else -ov[0]
    best = None
    for sign in (1.0, -1.0):
        R = np.array(_frame(Vector(a_o), Vector(ov[2] * sign))) @ np.array(_frame(Vector(a_g), Vector(gv[2]))).T
        tv = oc - R @ gc
        for _ in range(40):
            P = G @ R.T + tv
            Q = np.array([hand_tree.find_nearest(Vector(p))[0][:] for p in P])
            dist = np.linalg.norm(P - Q, axis=1)
            keep = dist <= np.percentile(dist, 70)
            mg, mq = G[keep].mean(0), Q[keep].mean(0)
            U, _, Vt = np.linalg.svd((G[keep] - mg).T @ (Q[keep] - mq))
            Dm = np.diag([1.0, 1.0, np.sign(np.linalg.det(Vt.T @ U.T))])
            R = Vt.T @ Dm @ U.T
            tv = mq - R @ mg
        err = float(dist[keep].mean())
        if best is None or err < best[0]:
            best = (err, R.copy(), tv.copy())
    Rh, Wh = Matrix(best[1].tolist()), Vector(best[2])          # Wh: where the glove's wrist ends up
    # the cuff takes the hand's turn and is then swung onto the forearm (no twist between the two parts)
    Rc = (Rh @ cg).rotation_difference(co).to_matrix() @ Rh
    info = {"scale": round(s, 2), "wrist_cm": round(float(wrist_g * s) * 100, 1),
            "hand_len_cm": [round(float(hand_len_g * s) * 100, 1), round((tip - W).length * 100, 1)],
            "hand_fit_mm": round(best[0] * 1000, 1), "wrist_off_cm": round((Wh - W).length * 100, 1),
            "cuff_swing_deg": round(math.degrees((Rh @ cg).angle(co)), 1)}
    blend0, blend1 = tw - 0.08 * Lg, tw + 0.08 * Lg
    out = []
    for i, v in enumerate(piece.data.vertices):
        d = v.co - Wg
        u = min(1.0, max(0.0, (t[i] - blend0) / (blend1 - blend0)))
        u = u * u * (3 - 2 * u)
        out.append(Wh + ((Rc @ d) * (1 - u) + (Rh @ d) * u) * s)
    # the cuff, station by station along the forearm: widened and centred on the forearm
    cuff = [i for i in range(len(out)) if t[i] < tw]
    tau = {i: (out[i] - W).dot(co) for i in cuff}
    t_end = min(tau.values())
    # the cuffs were drawn elbow-long for a hand much smaller than this one: shortened to a share of the forearm
    fa = min(1.0, cuff_share * (W - E).length / -t_end)
    for i in cuff:
        if tau[i] < 0:
            out[i] = out[i] - co * (tau[i] * (1 - fa))
            tau[i] *= fa
    t_end *= fa
    info["cuff_cm"] = round(-t_end * 100, 1)
    NS = 8
    st_k = []
    for k in range(NS):
        a, b = t_end * (k + 1) / NS, t_end * k / NS          # b is nearer the wrist (tau runs from t_end < 0 to 0)
        sel = [i for i in cuff if a <= tau[i] <= b]
        bod = [p for p in fore if a - 0.01 <= (p - W).dot(co) <= b + 0.01]
        if len(sel) < 4 or len(bod) < 4:
            st_k.append(None)
            continue
        row = []
        for f in (f1, f2):
            cv = [(out[i] - W).dot(f) for i in sel]
            bv = [(p - W).dot(f) for p in bod]
            ce, be = max(cv) - min(cv), max(bv) - min(bv)
            row += [min(1.3, max(1.0, be * 1.04 / (ce * 0.90))), (max(cv) + min(cv)) / 2, (max(bv) + min(bv)) / 2]
        st_k.append(row)
    last = [1.0, 0.0, 0.0, 1.0, 0.0, 0.0]
    for k in range(NS):
        if st_k[k] is None:
            st_k[k] = list(last)
        last = st_k[k]
    for k in range(1, NS):                                   # never narrower towards the elbow
        st_k[k][0] = max(st_k[k][0], st_k[k - 1][0])
        st_k[k][3] = max(st_k[k][3], st_k[k - 1][3])
    for i in cuff:
        x = min(NS - 1e-6, max(0.0, tau[i] / t_end * NS - 0.5))
        k = int(x)
        fr = x - k
        row = [st_k[k][j] * (1 - fr) + st_k[min(NS - 1, k + 1)][j] * fr for j in range(6)]
        ramp = max(0.0, min(1.0, (tau[i] / t_end) / 0.25))
        ramp = ramp * ramp * (3 - 2 * ramp)
        p = out[i]
        for f, j in ((f1, 0), (f2, 3)):
            q = (p - W).dot(f)
            kk = 1 + (row[j] - 1) * ramp
            nq = row[j + 1] + (q - row[j + 1]) * kk + (row[j + 2] - row[j + 1]) * ramp
            p = p + f * (nq - q)
        out[i] = p
    info["cuff_widen"] = [round(max(r[0], r[3]), 2) for r in st_k]
    for v, p in zip(piece.data.vertices, out):
        v.co = p
    piece.data.update()
    # skin
    segs = [("lowerarm" + sd, W + co * t_end, W)]
    for f in ("index", "middle", "ring", "pinky", "thumb"):
        segs.append(("hand" + sd, W, _bh(arm, f + "_01" + sd)))
        for j in (1, 2, 3):
            b = "%s_0%d%s" % (f, j, sd)
            h, tl = _bh(arm, b), _bt(arm, b)
            if j == 3:
                tl = h + (tl - h) * 3.0                      # claws and finger tips reach past the last joint
            segs.append((b, h, tl))

    def seg_d(p, a, b):
        ab = b - a
        u = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
        return (p - (a + ab * u)).length

    n = len(out)
    hard = []
    for i in range(n):
        if t[i] < blend0:
            hard.append("lowerarm" + sd)
        else:
            hard.append(min(segs[1:], key=lambda sg: seg_d(out[i], sg[1], sg[2]))[0])
    adj = [[] for _ in range(n)]
    for e in piece.data.edges:
        a, b = e.vertices
        adj[a].append(b)
        adj[b].append(a)
    per = [{hard[i]: 1.0} for i in range(n)]
    for _ in range(2):
        new = []
        for i in range(n):
            acc = dict(per[i])
            for j in adj[i]:
                for b, w in per[j].items():
                    acc[b] = acc.get(b, 0.0) + w / max(1, len(adj[i]))
            tot = sum(acc.values())
            new.append({b: w / tot for b, w in acc.items()})
        per = new
    _set_weights(piece, per)
    return info


# ------------------------------------------------------------------------------------------------------ dispatch
def fit_piece(piece, body, arm, dom, st, slot, side, mirror):
    sd = SIDES[side] if side else ""
    tn = TUNE.get((st, slot), {})
    if mirror:
        piece.data.transform(Matrix.Scale(-1, 4, (1, 0, 0)))
        piece.data.flip_normals()
    if slot == "Pelvis":
        return fit_belt(piece, body, dom)
    if slot == "Foot":
        info = fit_boot(piece, body, arm, dom, sd)
        transfer_weights(piece, body)
        return info
    if slot == "Helmet":
        return fit_helmet(piece, body, arm, dom, **tn)
    if slot == "Chest":
        return fit_chest(piece, body, arm, dom, **tn)
    if slot == "Hand":
        return fit_gauntlet(piece, body, arm, dom, sd, **tn)
    if slot == "Shoulder":
        return fit_shoulder(piece, body, arm, sd, **tn)
    limb = lambda *pre: _pts(body, dom, lambda b: b.startswith(pre) and b.endswith(sd))
    tree = lambda *pre: _part_tree(body, dom, lambda b: b.startswith(pre) and b.endswith(sd))
    if slot == "Thigh":
        return fit_tube(piece, body, arm, sd, "thigh", "calf", limb("thigh"), tree("thigh"), tn.get("top", 0.03), tn.get("len", 0.40),
                        part=(0.40, 1.0))
    if slot == "UpperArm":
        return fit_tube(piece, body, arm, sd, "upperarm", "lowerarm", limb("upperarm"), tree("upperarm"), tn.get("top", 0.07), tn.get("len", 0.26),
                        part=(0.35, 1.0))
    if slot == "Shin":
        _cut_below(piece, tn.get("cut", 0.3))
        return fit_tube(piece, body, arm, sd, "calf", "foot", limb("calf"), tree("calf"), tn.get("top", 0.05), tn.get("len", 0.30))
    if slot == "Knee":
        if tn.get("kind") == "cap":
            return fit_knee_cap(piece, body, arm, dom, sd)
        return fit_tube(piece, body, arm, sd, "calf", "foot", limb("thigh", "calf"), tree("thigh", "calf"), tn.get("top", -0.10),
                        tn.get("len", 0.19), bind={"thigh" + sd: 0.5, "calf" + sd: 0.5})
    raise RuntimeError("no fit for slot " + slot)


def transfer_weights(piece, body):
    """Cloth-like skin copied from the nearest body vertices: only for the boots, which bend with the foot."""
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


def clusters(isl, gap):
    """Islands whose boxes are closer than `gap` form one piece (a helmet is 18 islands: shell, visor, teeth, halo)."""
    n = len(isl)
    par = list(range(n))

    def find(a):
        while par[a] != a:
            par[a] = par[par[a]]
            a = par[a]
        return a

    for a in range(n):
        for b in range(a + 1, n):
            A, B = isl[a], isl[b]
            if max(max(A["min"][k] - B["max"][k], B["min"][k] - A["max"][k], 0.0) for k in range(3)) < gap:
                par[find(a)] = find(b)
    out = {}
    for i in range(n):
        out.setdefault(find(i), []).append(i)
    return list(out.values())


def piece_groups(src_ob, fn):
    """{(set, slot, side): vertex indices}. Classifying whole pieces, not single islands: classifying by island
    centre sent a helmet's jaw to the chest and a breastplate's lower plate to the forearm. Armor 2's belt, codpiece
    and thigh plates touch each other in the line-up; that cluster is split island by island."""
    isl = islands(src_ob)
    groups = {}
    for m in clusters(isl, 0.012 if fn == 1 else 0.003):
        mn = Vector([min(isl[i]["min"][a] for i in m) for a in range(3)])
        mx = Vector([max(isl[i]["max"][a] for i in m) for a in range(3)])
        c, size = (mn + mx) / 2, mx - mn
        if fn == 2 and c.z > 0.6 and size.z > 0.2:
            for i in m:
                groups.setdefault(armor_slot(fn, isl[i]["c"], isl[i]["s"]), set()).update(isl[i]["verts"])
        else:
            groups.setdefault(armor_slot(fn, c, size), set()).update(v for i in m for v in isl[i]["verts"])
    return groups


# ---------------------------------------------------------------------------------------------- the body under it
SUIT_RGB = (0.170, 0.175, 0.190)     # sRGB values: charcoal, a little cool
TRIM_RGB = (0.300, 0.310, 0.330)     # the collar and the cuffs of the suit


def _image_of(mat, sock):
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    stack = [bsdf.inputs[sock]]
    while stack:
        s = stack.pop()
        for l in s.links:
            if l.from_node.type == "TEX_IMAGE":
                return l.from_node.image
            stack.extend(i for i in l.from_node.inputs if i.is_linked)
    return None


def paint_suit(body, arm, dom):
    """Paints an undersuit into the body's own textures: everything below a collar line across the neck (a plane
    square to the neck, so it is high at the nape and low at the throat; the jaw stays above it) becomes dark ribbed
    fabric, hands included (gloves), with a trim band at the collar. The skin's shading is kept as the fabric's
    shading, so the muscles still read. Only the head stays skin. The bare belly, back, elbows and the shorts painted
    into the delivered texture no longer show between the armor pieces. (A first version stopped at the wrists: a
    ring of green skin showed between cuff and glove.)
    Done per texel: every triangle is drawn into UV space with its world position, so the collar is a clean line
    whatever the mesh's resolution (4,147 triangles)."""
    me = body.data
    wm = body.matrix_world
    base, mr = _image_of(me.materials[0], "Base Color"), _image_of(me.materials[0], "Roughness")
    Wd, Hd = base.size
    me.calc_loop_triangles()
    uvl = me.uv_layers.active.data
    cover = np.zeros((Hd, Wd), dtype=bool)
    suit = np.zeros((Hd, Wd), dtype=np.float32)
    trim = np.zeros((Hd, Wd), dtype=np.float32)
    zc = np.zeros((Hd, Wd), dtype=np.float32)
    n1, hd = _bh(arm, "neck_01"), _bh(arm, "head")
    nax = np.array((hd - n1).normalized())
    C = np.array(n1 + (hd - n1) * 0.50)
    co = [np.array(wm @ v.co) for v in me.vertices]
    for tri in me.loop_triangles:
        P = [co[i] for i in tri.vertices]
        uv = np.array([uvl[l].uv[:] for l in tri.loops]) * (Wd, Hd)
        x0, x1 = int(max(0, np.floor(uv[:, 0].min()) - 1)), int(min(Wd - 1, np.ceil(uv[:, 0].max()) + 1))
        y0, y1 = int(max(0, np.floor(uv[:, 1].min()) - 1)), int(min(Hd - 1, np.ceil(uv[:, 1].max()) + 1))
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        den = (uv[1, 1] - uv[2, 1]) * (uv[0, 0] - uv[2, 0]) + (uv[2, 0] - uv[1, 0]) * (uv[0, 1] - uv[2, 1])
        if abs(den) < 1e-9:
            continue
        a = ((uv[1, 1] - uv[2, 1]) * (gx - uv[2, 0]) + (uv[2, 0] - uv[1, 0]) * (gy - uv[2, 1])) / den
        b = ((uv[2, 1] - uv[0, 1]) * (gx - uv[2, 0]) + (uv[0, 0] - uv[2, 0]) * (gy - uv[2, 1])) / den
        c = 1 - a - b
        ins = (a >= -0.03) & (b >= -0.03) & (c >= -0.03)
        if not ins.any():
            continue
        pos = a[..., None] * P[0] + b[..., None] * P[1] + c[..., None] * P[2]
        dn = (pos - C) @ nax                               # above the collar line: > 0
        m = np.clip(-dn / 0.004, 0, 1)
        tr = ((dn < 0) & (dn > -0.022)).astype(np.float32)
        sl =(slice(y0, y1 + 1), slice(x0, x1 + 1))
        suit[sl][ins] = m[ins]
        trim[sl][ins] = tr[ins]
        zc[sl][ins] = pos[..., 2][ins]
        cover[sl][ins] = True
    for _ in range(8):                                       # into the padding round each UV island
        acc = [np.zeros_like(suit) for _ in range(3)]
        cnt = np.zeros_like(suit)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            cs = np.roll(cover, (dy, dx), (0, 1))
            cnt += cs
            for k, arr in enumerate((suit, trim, zc)):
                acc[k] += np.roll(arr * cover, (dy, dx), (0, 1))
        new = (~cover) & (cnt > 0)
        for k, arr in enumerate((suit, trim, zc)):
            arr[new] = acc[k][new] / cnt[new]
        cover |= new
    px = np.empty(Wd * Hd * 4, dtype=np.float32)
    base.pixels.foreach_get(px)
    px = px.reshape(Hd, Wd, 4)
    lum = px[..., :3] @ np.array((0.30, 0.59, 0.11), dtype=np.float32)
    ref = float(np.median(lum[suit > 0.5]))
    shade = np.clip(lum / ref, 0.45, 1.7) * (1 + 0.07 * np.sin(zc * (2 * math.pi / 0.022)))
    isT = (trim > 0.5) & (suit > 0.5)
    for k in range(3):
        col = np.where(isT, TRIM_RGB[k] * (0.8 + 0.2 * shade), SUIT_RGB[k] * shade)
        px[..., k] = px[..., k] * (1 - suit) + col * suit
    base.pixels.foreach_set(px.ravel())
    base.save()
    if base.packed_file:                              # packed images come back as delivered when the blend is reopened
        base.unpack(method="REMOVE")
    if mr is not None and tuple(mr.size) == (Wd, Hd):
        q = np.empty(Wd * Hd * 4, dtype=np.float32)
        mr.pixels.foreach_get(q)
        q = q.reshape(Hd, Wd, 4)
        q[..., 1] = q[..., 1] * (1 - suit) + np.where(isT, 0.45, 0.80) * suit     # G roughness
        q[..., 2] = q[..., 2] * (1 - suit) + np.where(isT, 0.60, 0.0) * suit      # B metal
        mr.pixels.foreach_set(q.ravel())
        mr.save()
        if mr.packed_file:
            mr.unpack(method="REMOVE")
    print("SUIT texels", int((suit > 0.5).sum()), "of", int(cover.sum()), "skin lum ref", round(ref, 3))


def make_nohands(body, dom):
    """The body worn under gauntlets: the same mesh without its hands (the gauntlets are the hands)."""
    o = body.copy()
    o.data = body.data.copy()
    o.name = body.name + "_NoHands"
    bpy.context.scene.collection.objects.link(o)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    dead = [f for f in bm.faces if all(dom.get(v.index, "").startswith(HANDY) for v in f.verts)]
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    bm.to_mesh(o.data)
    bm.free()
    o.parent = body.parent
    o.matrix_parent_inverse = Matrix.Identity(4)
    return o


def do_armor():
    d = out_dir("OrcArmor")
    arm, body, tex, s, bones = humanoid(src("Enemies", "Orc", "Base_Rigged_Orc.glb"), "Orc", 2.10, d)
    dom = body_regions(body, arm)
    paint_suit(body, arm, dom)                     # for the review renders; do_orc writes the game's textures
    report = {"pieces": {}, "textures": {}}
    for fn in (1, 2):
        meshes = append_glb(src("Enemies", "Orc", "Orc_Armor_%d.glb" % fn))
        src_ob = meshes[0]
        flatten([src_ob])
        report["textures"]["Armor%d" % fn] = save_textures(src_ob.data.materials[0], "OrcArmor%d" % fn, d)
        src_ob.data.materials[0].name = "M_OrcArmor%d" % fn
        groups = piece_groups(src_ob, fn)
        for (st, slot, side), verts in sorted(groups.items(), key=lambda kv: str(kv[0])):
            sides = [side] if side else (["r", "l"] if slot in ("Hand", "Foot") else [None])
            for sd in sides:
                # a knee piece is exported inside its set's thigh mesh
                name = "SK_OrcArmor_%s_%s_%s%s" % (st, "Thigh" if slot == "Knee" else slot, "k" if slot == "Knee" else "", sd or "c")
                p = split_by_verts(src_ob, verts, name)
                vs = [v.co for v in p.data.vertices]
                c = Vector(((min(v.x for v in vs) + max(v.x for v in vs)) / 2, (min(v.y for v in vs) + max(v.y for v in vs)) / 2,
                            (min(v.z for v in vs) + max(v.z for v in vs)) / 2))
                p.data.transform(Matrix.Translation(-c))
                # a single gauntlet / boot in Armor 1 is the right one; it is mirrored for the left
                info = fit_piece(p, body, arm, dom, st, slot, sd, mirror=(fn == 1 and sd == "l"))
                report["pieces"]["%s_%s_%s" % (st, slot, sd or "c")] = info
                print("FIT", st, slot, sd, info)
        bpy.data.objects.remove(src_ob)
    make_nohands(body, dom)
    to_cm(arm, [o for o in bpy.context.scene.objects if o.type == "MESH"])
    # merge sides: one mesh per set and slot, then export each with the orc's armature
    by_slot = {}
    for o in [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith("SK_OrcArmor_")]:
        _, _, st, slot, _ = o.name.split("_")[:5]
        by_slot.setdefault((st, slot), []).append(o)
    tris = {}
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
        tris[st] = tris.get(st, 0) + sum(len(p.vertices) - 2 for p in m.data.polygons)
        export_skeletal(arm, [m], os.path.join(d, m.name + ".fbx"))
    report["triangles_per_set"] = tris
    body.hide_set(True)
    write_report(d, report)
    if "--blend" in ARGS:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(d, "armor_fit_debug.blend"))
