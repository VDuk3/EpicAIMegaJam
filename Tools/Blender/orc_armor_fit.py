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
BOOT_TOP = 0.185     # m above the floor: the boots are cut here (ankle); the greaves come down to this height
TUNE = {
    ("A", "Helmet"): {"s": 2.75}, ("B", "Helmet"): {"s": 2.85, "dy": -0.025}, ("C", "Helmet"): {"s": 3.15, "dz": 0.07, "dy": 0.015}, ("D", "Helmet"): {"s": 2.75},
    ("D", "Chest"): {"top": 1.87},
    ("A", "Shin"): {"cut": 0.38}, ("B", "Shin"): {"cut": 0.20, "cap": 2.2}, ("C", "Shin"): {"cut": 0.25}, ("D", "Shin"): {"cut": 0.36},
    ("A", "Knee"): {"kind": "tube"}, ("C", "Knee"): {"kind": "cap"},
    ("B", "Hand"): {"left_glove": True},
    ("A", "Shoulder"): {"keep": 0.55}, ("B", "Shoulder"): {"keep": 0.55}, ("D", "Shoulder"): {"keep": 0.55},
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


def _face_points(piece, n=1500):
    """n fixed points on the piece's faces, as (corner indices, weights): they move with the vertices."""
    me = piece.data
    me.calc_loop_triangles()
    V = np.array([v.co[:] for v in me.vertices], dtype=float)
    T = np.array([tr.vertices[:] for tr in me.loop_triangles])
    ar = np.linalg.norm(np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]]), axis=1)
    rng = np.random.default_rng(11)
    idx = rng.choice(len(T), size=n, p=ar / ar.sum())
    r1, r2 = np.sqrt(rng.random(n)), rng.random(n)
    return T[idx], np.stack([1 - r1, r1 * (1 - r2), r1 * r2], axis=1)


def _at(piece, fp):
    V = np.array([v.co[:] for v in piece.data.vertices], dtype=float)
    return (V[fp[0]] * fp[1][:, :, None]).sum(axis=1)


def _sunk(piece, tree, fp, mask=None, depth=0.004):
    """Share of the piece's surface that lies under the skin of that part of the body (deeper than depth), measured
    on the face points fp. Measured on vertices it missed everything between them: set B's thigh tube has 38
    vertices, all of them clear, and the thigh came through the middle of its plates."""
    P = _at(piece, fp)
    if mask is not None:
        P = P[mask]
    n = 0
    for p in P:
        loc, nrm, idx, dist = tree.find_nearest(Vector(p), 0.12)
        if loc is not None and (Vector(p) - loc).dot(nrm) < -depth:
            n += 1
    return n / max(1, len(P))


def _inside_body(tree, p):
    loc, nrm, idx, dist = tree.find_nearest(p)
    return loc is not None and (p - loc).dot(nrm) < 0


def _cut(piece, z, keep_above):
    """Cuts the piece with a horizontal plane at height z and keeps one side. A real cut through the faces: deleting
    the vertices on the other side left the edge as ragged as the mesh is coarse."""
    bm = bmesh.new()
    bm.from_mesh(piece.data)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z), plane_no=(0, 0, 1),
                           clear_inner=keep_above, clear_outer=not keep_above)
    bm.to_mesh(piece.data)
    bm.free()
    piece.data.update()


def _cut_below(piece, frac):
    """Removes the part of a piece below `frac` of its height (the foot of a greave that is really a tall boot)."""
    zs = [v.co.z for v in piece.data.vertices]
    _cut(piece, min(zs) + (max(zs) - min(zs)) * frac, keep_above=True)


def _sym_yaw(piece):
    """The turn about the vertical that makes the piece most alike to its own mirror image, in degrees. Set B's
    helmet stands a few degrees off the front in the line-up; fitted as it stood, it sat crooked on the head."""
    from mathutils.kdtree import KDTree
    S = _surface_samples(piece, 3000)
    S = S - S.mean(0)

    def score(deg):
        c, sn = math.cos(math.radians(deg)), math.sin(math.radians(deg))
        P = np.stack([S[:, 0] * c - S[:, 1] * sn, S[:, 0] * sn + S[:, 1] * c, S[:, 2]], axis=1)
        x0 = (np.percentile(P[:, 0], 2) + np.percentile(P[:, 0], 98)) / 2
        kd = KDTree(len(P))
        for i, p in enumerate(P):
            kd.insert(p, i)
        kd.balance()
        return sum(kd.find((2 * x0 - p[0], p[1], p[2]))[2] for p in P[::4]) / len(P[::4])

    best = min(range(-40, 41, 2), key=score)
    return min((best + d / 2 for d in range(-4, 5)), key=score)


# ------------------------------------------------------------------------------------------------------ tubes
def fit_tube(piece, body, arm, sd, bone_a, bone_b, pts, tree, top_t, max_len, margin=1.10, bind=None, part=(0.0, 1.0),
             test=None, force=None, rounds=5, cap=1.7, bottom_z=None, stretch=1.0):
    """A closed tube round a limb. The piece stands as in the line-up (top up); it is turned so that "down" runs
    along the bone (shortest rotation: nothing is turned upside down), its top put top_t metres past the bone's head,
    and scaled: cross-section to the limb's own cross-section over the stretch `part` of the tube's length times
    `margin`, length to at most max_len.
    Then it is widened only where the limb still comes through: the length is cut into five stations; in each one the
    depth of the deepest points under the skin is measured (on points spread over the faces, not on the vertices:
    set B's thigh tube has 38 vertices) and the station is widened about the tube's centre line by just that much,
    the amounts blended from station to station. So a greave keeps its width at the knee and opens at the ankle,
    where this orc's leg is thicker than the leg the greave was drawn for.
    What did not work: growing the whole tube until every point of the limb saw it around (the slanted tops made
    it grow to the limit: barrels); growing it whole until its vertices were clear (the same, less); letting it sink
    12 mm into the suit (ragged patches of suit through the plates).
    `test`: the stretch that must be clear (default `part`). `force`: the result of the other side's piece, applied
    as it is, so that left and right are the same size. The limb is the armored body's (make_armored)."""
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

    NS = 5
    if force:
        s1, s2, st, wide = force
        u0, u1, w0, w1 = section(top_t + part[0] * psize.z * st, top_t + part[1] * psize.z * st)
    else:
        wide = [1.0] * NS
        st = max_len / psize.z
        for _ in range(3):
            u0, u1, w0, w1 = section(top_t + part[0] * psize.z * st, top_t + part[1] * psize.z * st)
            s1, s2 = (u1 - u0) * margin / psize.x, (w1 - w0) * margin / psize.y
            g = (s1 * s2) ** 0.5
            s1, s2 = max(g * 0.85, min(g * 1.18, s1)), max(g * 0.85, min(g * 1.18, s2))
            st = min(g * stretch, max_len / psize.z)
    L = psize.z * st
    if bottom_z is not None:
        # the tube's lower edge at a given height (the top of the cut boots): it slides down the limb as far as needed
        top_t = max(top_t, (A.z - bottom_z) / max(0.2, -ax.z) - L)
        u0, u1, w0, w1 = section(top_t + part[0] * L, top_t + part[1] * L)
    cu, cw = (u0 + u1) / 2, (w0 + w1) / 2
    test = test or part
    fp = _face_points(piece)
    frac_v = [(pmax.z - p.z) / psize.z for p in src_v]
    frac_p = (pmax.z - _at(piece, fp)[:, 2]) / psize.z
    tested = (frac_p >= test[0]) & (frac_p <= test[1])
    stn = np.clip((frac_p * NS).astype(int), 0, NS - 1)

    def place():
        for v, p, fr in zip(piece.data.vertices, src_v, frac_v):
            x = min(NS - 1.0, max(0.0, fr * NS - 0.5))
            j = int(x)
            w = wide[j] * (1 - (x - j)) + wide[min(NS - 1, j + 1)] * (x - j)
            v.co = A + e1 * (cu + (p.x - pc.x) * s1 * w) + e2 * (cw + (p.y - pc.y) * s2 * w) + ax * (top_t + (pmax.z - p.z) * st)
        piece.data.update()

    place()
    out = _sunk(piece, tree, fp, tested)
    for _ in range(0 if force else rounds):
        if out < 0.02:
            break
        P = _at(piece, fp)
        depth = np.zeros(len(P))
        rad = np.zeros(len(P))
        for i, p in enumerate(P):
            v = Vector(p)
            near = tree.find_nearest(v, 0.12)
            if near[0] is not None:
                depth[i] = max(0.0, -(v - near[0]).dot(near[1]))
            d = v - A
            rad[i] = math.hypot(d.dot(e1) - cu, d.dot(e2) - cw)
        for j in range(NS):
            m = (stn == j) & tested
            if m.sum() < 5:
                continue
            deep = float(np.percentile(depth[m], 97))
            if deep > 0.002:
                wide[j] = min(cap, wide[j] * (1 + (deep + 0.005) / max(0.03, float(np.median(rad[m])))))
        for j in range(NS):                                    # no step between neighbours
            nb = [wide[i] for i in (j - 1, j + 1) if 0 <= i < NS]
            wide[j] = max(wide[j], 0.85 * max(nb))
        place()
        out = _sunk(piece, tree, fp, tested)
    if bind is None:
        _rigid(piece, bone_a + sd)
    else:
        _set_weights(piece, [dict(bind) for _ in piece.data.vertices])
    return {"scale": [round(s1, 2), round(s2, 2), round(st, 2)], "widened": [round(w, 2) for w in wide], "len_cm": round(L * 100, 1),
            "sunk": round(out, 3), "force": (s1, s2, st, list(wide))}


# ------------------------------------------------------------------------------------------------ shoulder caps
def fit_shoulder(piece, body, arm, sd, width=0.30, down=0.09, lift=0.12, force=None, keep=None):
    """The round pad: its strap clasps the top of the upper arm, its dome looks straight out of the deltoid. Moved
    out along that direction until the dome is clear of the body.
    `keep`: the front share of the piece's depth that is kept. The strap behind the dome, at four times its size,
    stood out of the orc's back like a hoop; it is cut off."""
    if keep:
        ys = [v.co.y for v in piece.data.vertices]
        ycut = min(ys) + (max(ys) - min(ys)) * keep
        bm = bmesh.new()
        bm.from_mesh(piece.data)
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.y > ycut], context="VERTS")
        bm.to_mesh(piece.data)
        bm.free()
        piece.data.update()
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
    off = force or 0.02
    while off < 0.30:
        c = S + ax * down + out_dir_ * off
        inside = sum(1 for i in dome if _inside_body(tree, c + R @ (src_v[i] * s)))
        if force or inside <= 0.03 * len(dome):
            break
        off += 0.01
    for v, p in zip(piece.data.vertices, src_v):
        v.co = c + R @ (p * s)
    piece.data.update()
    _rigid(piece, "upperarm" + sd)
    return {"scale": round(s, 2), "out_cm": round(off * 100, 1), "force": off}


# ------------------------------------------------------------------------------------------------------ cuirass
def fit_chest(piece, body, arm, dom, top=1.80, bottom=1.16, mx=1.08, my=1.08, band=(0.25, 0.60), tuck=0.40):
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
    fp = _face_points(piece)
    low = _at(piece, fp)[:, 2] < pz0 + 0.75 * (pz1 - pz0)
    # Front to back the cuirass follows the torso's own profile: in four bands of its height its depth is mapped
    # onto the torso's depth in the same band plus 3 cm each way (a band where the piece is not a whole shell, like
    # the hanging belly plate of set A, keeps the one scale). With one depth scale for the whole piece the back
    # plate stood 10 cm off the shoulder blades and its arm-hole rim showed behind the arm like a hoop.
    NS = 4
    plain = (sy, cy - sy * (py0 + py1) / 2)
    ymap = []
    for j in range(NS):
        f0, f1 = j / NS, (j + 1) / NS
        bp = [p.y for p in tor if bottom + f0 * (top - bottom) <= p.z <= bottom + f1 * (top - bottom) and abs(p.x - cx) < 0.20]
        pp = [v.y for v in src_v if pz0 + f0 * (pz1 - pz0) <= v.z <= pz0 + f1 * (pz1 - pz0)
              and abs(v.x - (px0 + px1) / 2) < 0.33 * (px1 - px0)]
        if len(bp) < 6 or len(pp) < 6 or _pct(pp, 0.95) - _pct(pp, 0.05) < 0.6 * (py1 - py0):
            ymap.append(plain)
            continue
        b0, b1 = _pct(bp, 0.02) - 0.03, _pct(bp, 0.98) + 0.03
        q0, q1 = _pct(pp, 0.05), _pct(pp, 0.95)
        a_ = max(0.6 * sy, min(1.4 * sy, (b1 - b0) / (q1 - q0)))
        ymap.append((a_, (b0 + b1) / 2 - a_ * (q0 + q1) / 2))

    def depth(p):
        x = min(NS - 1.0, max(0.0, (p.z - pz0) / (pz1 - pz0) * NS - 0.5))
        j = int(x)
        u = x - j
        a_ = ymap[j][0] * (1 - u) + ymap[min(NS - 1, j + 1)][0] * u
        b_ = ymap[j][1] * (1 - u) + ymap[min(NS - 1, j + 1)][1] * u
        y = a_ * p.y + b_
        # the back corners are tucked in: beyond 60 % of the half width the back half comes forward, by up to
        # `tuck` of its depth at the edge. The arm holes were drawn for arms that hang; behind this orc's shoulders
        # their rims had nothing to lie on.
        r = (abs(p.x - (px0 + px1) / 2) / ((px1 - px0) / 2) - 0.6) / 0.4
        if y > cy and r > 0:
            r = min(1.0, r)
            y = cy + (y - cy) * (1 - tuck * r * r * (3 - 2 * r))
        return y

    k = 1.0
    for it in range(10):
        if it:
            k *= 1.025
        for v, s in zip(piece.data.vertices, src_v):
            v.co = Vector((cx + (s.x - (px0 + px1) / 2) * sx * k, cy + (depth(s) - cy) * k, bottom + (s.z - pz0) * sz))
        piece.data.update()
        out = _sunk(piece, tree, fp, low)
        if out < 0.03:
            break
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
def fit_helmet(piece, body, arm, dom, s=2.75, dy=0.0, dz=0.0, yaw=None):
    """Upright on the skull (the first fit laid it along the neck bone, which leans forward: every helmet looked
    at the floor), one scale, bound to the head bone. The dome is centred over the cranium; crests, spikes and the
    halo are left out of the measure."""
    yaw = _sym_yaw(piece) if yaw is None else yaw
    piece.data.transform(Matrix.Rotation(math.radians(yaw), 4, "Z"))
    piece.data.update()
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
    return {"scale": s, "top_cm": round((top + 0.02 + dz) * 100, 1), "yaw_deg": round(yaw, 1)}


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
def fit_knee_cap(piece, body, arm, dom, sd, width=0.125, sink=0.08):
    """A cap on the front of the knee (set C's skull)."""
    K = _bh(arm, "calf" + sd)
    near = [p for p in _pts(body, dom, lambda b: b.startswith(("thigh", "calf")) and b.endswith(sd)) if abs(p.z - K.z) < 0.05]
    cx = (min(p.x for p in near) + max(p.x for p in near)) / 2
    # the front of the knee itself, found by a ray from in front of it (the frontmost point of the whole slice
    # belonged to the thigh's bulge and left the skull hanging in the air)
    hit = _body_tree(body).ray_cast(Vector((cx, K.y - 0.6, K.z)), Vector((0, 1, 0)), 1.0)[0]
    yf = hit.y if hit is not None else min(p.y for p in near)
    src_v = [v.co.copy() for v in piece.data.vertices]
    s = width / (max(v.x for v in src_v) - min(v.x for v in src_v))
    yb = max(v.y for v in src_v)
    for v, p in zip(piece.data.vertices, src_v):
        v.co = Vector((cx + p.x * s, yf + sink + (p.y - yb) * s, K.z + p.z * s))
    piece.data.update()
    _set_weights(piece, [{"thigh" + sd: 0.5, "calf" + sd: 0.5} for _ in piece.data.vertices])
    return {"scale": round(s, 2)}


# ----------------------------------------------------------------------------------------------------- gauntlet
def _surface_samples(piece, n=20000):
    """n points spread evenly over the piece's surface (the same ones every run)."""
    me = piece.data
    me.calc_loop_triangles()
    V = np.array([v.co[:] for v in me.vertices], dtype=float)
    T = np.array([tr.vertices[:] for tr in me.loop_triangles])
    a, b, c = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    ar = np.linalg.norm(np.cross(b - a, c - a), axis=1)
    rng = np.random.default_rng(7)
    idx = rng.choice(len(T), size=n, p=ar / ar.sum())
    r1, r2 = np.sqrt(rng.random(n)), rng.random(n)
    return a[idx] * (1 - r1)[:, None] + b[idx] * (r1 * (1 - r2))[:, None] + c[idx] * (r1 * r2)[:, None]


def _frame(a, n):
    a = a.normalized()
    n = (n - a * n.dot(a)).normalized()
    return Matrix((a, n, a.cross(n))).transposed()


def fit_gauntlet(piece, body, arm, dom, sd, whole, whole_dom, s=None, hand_margin=1.05, cuff_share=0.80, left_glove=False):
    """The gauntlet is a glove with a long cuff. The first fit boxed it onto forearm and hand along the bone and so
    put it on backwards: fingers up the forearm, the cuff's mouth open round the hand.
    Here it is read as two parts joined at its narrowest section (the wrist): the cuff is laid along the forearm
    (shortest rotation; the line-up gauntlet already lies like a right forearm) and widened station by station until
    the forearm is inside; the hand part is laid on the hand's own frame (wrist to knuckles, palm plane). The wrists
    coincide. Skin: cuff to the forearm bone, the hand part to the hand and finger bones nearest to each vertex, so
    the glove's fingers close with the orc's. The body is worn WITHOUT its hands under a gauntlet (SK_Orc_Armored)."""
    X = np.array([v.co[:] for v in piece.data.vertices], dtype=float)
    # Sections are measured on 20,000 points spread over the glove's SURFACE: the gloves have under a thousand
    # vertices, a slice may hold five of them, and a profile taken from vertices found the "wrist" anywhere.
    S = _surface_samples(piece)
    c = S.mean(0)
    ax = np.linalg.svd(S - c, full_matrices=False)[2][0]
    if ax[2] > 0:
        ax = -ax                                     # from the cuff (up) to the fingers (down)
    t = (X - c) @ ax
    ts = (S - c) @ ax
    tmin, Lg = ts.min(), ts.max() - ts.min()
    e1 = np.cross(ax, [0.0, 1.0, 0.0])
    e1 /= np.linalg.norm(e1)
    e2 = np.cross(ax, e1)
    NB = 24
    bins = []
    for k in range(NB):
        q = S[(ts >= tmin + Lg * k / NB) & (ts <= tmin + Lg * (k + 1) / NB)] - c
        u, w = q @ e1, q @ e2
        lo_u, hi_u, lo_w, hi_w = np.percentile(u, 1), np.percentile(u, 99), np.percentile(w, 1), np.percentile(w, 99)
        bins.append((hi_u - lo_u, hi_w - lo_w, (hi_u + lo_u) / 2, (hi_w + lo_w) / 2))
    area = [b[0] * b[1] for b in bins]
    rng_k = range(int(NB * 0.36), int(NB * 0.66))
    # the wrist: the narrowest section; where the narrow stretch is long (set C), its end on the hand's side
    kw = max(k for k in rng_k if area[k] <= 1.08 * min(area[j] for j in rng_k))
    tw = tmin + Lg * (kw + 0.5) / NB

    def centre(k):
        return Vector(c + ax * (tmin + Lg * (k + 0.5) / NB) + e1 * bins[k][2] + e2 * bins[k][3])

    Wg = centre(kw)
    k0 = 1
    cg = (Wg - centre(k0)).normalized()
    hand_len_g = tmin + Lg - tw
    wrist_g = (bins[kw][0] + bins[kw][1]) / 2
    # the orc's arm
    W, E = _bh(arm, "hand" + sd), _bh(arm, "lowerarm" + sd)
    co = (W - E).normalized()
    f1 = (Vector((0, 0, 1)) - co * co.z).normalized()
    f2 = co.cross(f1)
    fore = _pts(body, dom, lambda b: b.startswith("lowerarm") and b.endswith(sd))
    tip = max(_pts(whole, whole_dom, lambda b: b.startswith("middle") and b.endswith(sd)), key=lambda p: (p - W).length)
    if s is None:                                   # the glove's hand as long as the orc's (claws make it longer)
        s = max(2.3, min(3.2, hand_margin * (tip - W).length / hand_len_g))
    s = float(s)
    # The glove's hand is laid ON the orc's hand: both are hands, so the glove's hand part is registered to the
    # hand's surface (start: principal axes, long axis away from the wrist, both ways up tried; then closest points
    # and the best rigid motion, the farthest 30 % left out each round: claws, cuffs' rims). Taking the hand's
    # direction from the glove's own shape instead (wrist to palm centre) was off by 40 to 60 degrees.
    is_hand = lambda b: b.startswith(HANDY) and b.endswith(sd)
    hand_tree = _part_tree(whole, whole_dom, is_hand)
    Hp = np.array([p[:] for p in _pts(whole, whole_dom, is_hand)])
    G = (S[ts > tw][::6] - np.array(Wg)) * s
    gc, oc = G.mean(0), Hp.mean(0)
    gv = np.linalg.svd(G - gc, full_matrices=False)[2]
    ov = np.linalg.svd(Hp - oc, full_matrices=False)[2]
    a_g = gv[0] if gv[0] @ gc > 0 else -gv[0]
    a_o = ov[0] if ov[0] @ (oc - np.array(W)) > 0 else -ov[0]
    from mathutils.kdtree import KDTree
    thumb = [p for p in _pts(whole, whole_dom, lambda b: b.startswith("thumb") and b.endswith(sd))]
    best = None
    for sign, turn in ((1.0, 0.0), (-1.0, 0.0), (1.0, 90.0), (-1.0, 90.0)):
        # four starts: the two ways up, each also a quarter turn round the hand's length (a mitten is nearly round)
        Fo = np.array(_frame(Vector(a_o), Vector(ov[2] * sign)))
        if turn:
            Fo = np.array(Matrix.Rotation(math.radians(turn), 3, Vector(a_o))) @ Fo
        R = Fo @ np.array(_frame(Vector(a_g), Vector(gv[2]))).T
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
        # a good registration also leaves the glove's wrist at the orc's wrist and its cuff along the forearm;
        # judged on the surface distance alone, set B's mitten was laid across the hand
        swing = (Matrix(R.tolist()) @ cg).angle(co)
        # ... and the glove's thumb on the orc's thumb: how far the orc's thumb is from the glove's surface. Without
        # this term set B's mitten was registered palm outwards, its thumb on the little finger's side.
        P = G @ R.T + tv
        kd = KDTree(len(P))
        for i, p in enumerate(P):
            kd.insert(p, i)
        kd.balance()
        d_thumb = sum(kd.find(p)[2] for p in thumb) / len(thumb)
        cost = err / 0.006 + (Vector(tv) - W).length / 0.06 + swing / math.radians(25) + d_thumb / 0.012
        if best is None or cost < best[3]:
            best = (err, R.copy(), tv.copy(), cost, d_thumb, (sign, turn))
    Rh, Wh = Matrix(best[1].tolist()), Vector(best[2])          # Wh: where the glove's wrist ends up
    if left_glove:
        # The line-up has one glove per set and nothing says for which hand. Sets A, C and D have a right one; set
        # B's is a LEFT one (its thumb is on the other side on the contact sheet, and the owner saw it on the fitted
        # orc: "the thumb is on the wrong side"). Registered as it is, it lies well (back of the hand out, cuff along
        # the forearm) with the thumb on the little finger's side; so it is registered as it is and then reflected
        # side to side in the hand's own frame (the plane of the hand's length and the palm's normal), which moves
        # the thumb across and nothing else. Tried first: mirroring the piece before registering it (it was then laid
        # palm outwards, cuff twisted 46 degrees), and letting the registration choose between the piece and its
        # mirror (right for A, C and D by 3.5 against 7.5, wrong for B by 5.7 against 7.1).
        n_hand = Vector(gv[2]) - Vector(a_g) * float(np.dot(gv[2], a_g))
        lat = Vector(a_g).cross(n_hand).normalized()
        for v in piece.data.vertices:
            d = v.co - Wg
            v.co = Wg + d - lat * (2 * d.dot(lat))
        piece.data.flip_normals()
        piece.data.update()
        cg = (cg - lat * (2 * cg.dot(lat))).normalized()
        X = np.array([v.co[:] for v in piece.data.vertices], dtype=float)
        t = (X - c) @ ax
    # the cuff takes the hand's turn and is then swung onto the forearm (no twist between the two parts)
    Rc = (Rh @ cg).rotation_difference(co).to_matrix() @ Rh
    info = {"scale": round(s, 2), "wrist_cm": round(float(wrist_g * s) * 100, 1),
            "hand_len_cm": [round(float(hand_len_g * s) * 100, 1), round((tip - W).length * 100, 1)],
            "hand_fit_mm": round(best[0] * 1000, 1), "thumb_mm": round(best[4] * 1000, 1), "cost": float(best[3]), "wrist_off_cm": round((Wh - W).length * 100, 1),
            "cuff_swing_deg": round(math.degrees((Rh @ cg).angle(co)), 1)}
    if os.environ.get("GAUNTLET_DEBUG"):
        print("GAUNTLET", sd, "kw", kw, "Lg_cm", round(Lg * 100, 1), "area_cm2", [round(float(a) * 1e4) for a in area])
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
    NS = 3            # few stations: a station of a low-poly cuff with five vertices in it measured too narrow
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
            row += [min(1.2, max(1.0, be * 1.04 / (ce * 0.90))), (max(cv) + min(cv)) / 2, (max(bv) + min(bv)) / 2]
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
def fit_piece(piece, body, arm, dom, st, slot, side, mirror, whole, whole_dom):
    sd = SIDES[side] if side else ""
    tn = TUNE.get((st, slot), {})
    if mirror:
        piece.data.transform(Matrix.Scale(-1, 4, (1, 0, 0)))
        piece.data.flip_normals()
    if slot == "Pelvis":
        return fit_belt(piece, body, dom)
    if slot == "Foot":
        info = fit_boot(piece, body, arm, dom, sd)
        _cut(piece, BOOT_TOP, keep_above=False)        # owner, 2026-11-14: "take the upper part of the boots off"
        transfer_weights(piece, body)
        return info
    if slot == "Helmet":
        return fit_helmet(piece, body, arm, dom, **tn)
    if slot == "Chest":
        return fit_chest(piece, body, arm, dom, **tn)
    if slot == "Hand":
        return fit_gauntlet(piece, body, arm, dom, sd, whole, whole_dom, **tn)
    limb = lambda *pre: _pts(body, dom, lambda b: b.startswith(pre) and b.endswith(sd))
    tree = lambda *pre: _part_tree(body, dom, lambda b: b.startswith(pre) and b.endswith(sd))
    force = PAIR.get((st, slot))                   # the right side is fitted first; the left takes its sizes
    if slot == "Shoulder":
        info = fit_shoulder(piece, body, arm, sd, force=force, **tn)
    elif slot == "Thigh":
        info = fit_tube(piece, body, arm, sd, "thigh", "calf", limb("thigh"), tree("thigh"), tn.get("top", 0.03), tn.get("len", 0.40),
                        part=(0.30, 0.88), test=(0.12, 0.88), force=force)
    elif slot == "UpperArm":
        info = fit_tube(piece, body, arm, sd, "upperarm", "lowerarm", limb("upperarm"), tree("upperarm"), tn.get("top", 0.07),
                        tn.get("len", 0.26), part=(0.35, 0.85), force=force)
    elif slot == "Shin":
        _cut_below(piece, tn.get("cut", 0.3))
        info = fit_tube(piece, body, arm, sd, "calf", "foot", limb("calf"), tree("calf"), tn.get("top", 0.05), tn.get("len", 0.33),
                        test=(0.0, 0.85), force=force, cap=tn.get("cap", 1.7), bottom_z=BOOT_TOP - 0.015, stretch=1.2)
    elif slot == "Knee" and tn.get("kind") == "cap":
        return fit_knee_cap(piece, body, arm, dom, sd)
    elif slot == "Knee":
        info = fit_tube(piece, body, arm, sd, "calf", "foot", limb("thigh", "calf"), tree("thigh", "calf"), tn.get("top", -0.10),
                        tn.get("len", 0.19), bind={"thigh" + sd: 0.5, "calf" + sd: 0.5}, force=force)
    else:
        raise RuntimeError("no fit for slot " + slot)
    PAIR.setdefault((st, slot), info.pop("force"))
    return info


PAIR = {}


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
SUIT_RGB = (0.165, 0.150, 0.140)     # sRGB values: warm charcoal (a cool one merged with set B's blue steel)
TRIM_RGB = (0.300, 0.300, 0.300)     # the collar of the suit
GLOW_RGB = (1.000, 0.330, 0.080)     # the suit's light lines; the orc's material glows where the texture is this orange


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
    glow = np.zeros((Hd, Wd), dtype=np.float32)
    zc = np.zeros((Hd, Wd), dtype=np.float32)
    # light lines, 7 mm wide: a ring inside the collar band, a ring round each elbow and each knee, and one line down
    # the spine. They sit where no set covers the suit (neck, elbows, knees, the small of the back).
    rings = []
    for sd in ("_r", "_l"):
        for a_, b_, lim in (("upperarm", "lowerarm", 0.16), ("thigh", "calf", 0.20)):
            J = _bh(arm, b_ + sd)
            rings.append((np.array(J), np.array((J - _bh(arm, a_ + sd)).normalized()), lim))
    spine_top, spine_bot = _bh(arm, "neck_01").z, _bh(arm, "spine_01").z
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
        gl = ((dn < -0.008) & (dn > -0.015)).astype(np.float32)
        for J, jn, lim in rings:
            rel = pos - J
            gl = np.maximum(gl, ((np.abs(rel @ jn) < 0.0035) & (np.linalg.norm(rel, axis=-1) < lim)).astype(np.float32))
        gl = np.maximum(gl, ((np.abs(pos[..., 0] - n1.x) < 0.0035) & (pos[..., 1] > n1.y + 0.05) & (pos[..., 2] < spine_top)
                             & (pos[..., 2] > spine_bot)).astype(np.float32))
        sl = (slice(y0, y1 + 1), slice(x0, x1 + 1))
        suit[sl][ins] = m[ins]
        trim[sl][ins] = tr[ins]
        glow[sl][ins] = gl[ins]
        zc[sl][ins] = pos[..., 2][ins]
        cover[sl][ins] = True
    for _ in range(8):                                       # into the padding round each UV island
        acc = [np.zeros_like(suit) for _ in range(4)]
        cnt = np.zeros_like(suit)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            cs = np.roll(cover, (dy, dx), (0, 1))
            cnt += cs
            for k, arr in enumerate((suit, trim, zc, glow)):
                acc[k] += np.roll(arr * cover, (dy, dx), (0, 1))
        new = (~cover) & (cnt > 0)
        for k, arr in enumerate((suit, trim, zc, glow)):
            arr[new] = acc[k][new] / cnt[new]
        cover |= new
    px = np.empty(Wd * Hd * 4, dtype=np.float32)
    base.pixels.foreach_get(px)
    px = px.reshape(Hd, Wd, 4)
    lum = px[..., :3] @ np.array((0.30, 0.59, 0.11), dtype=np.float32)
    ref = float(np.median(lum[suit > 0.5]))
    shade = np.clip(lum / ref, 0.45, 1.7) * (1 + 0.07 * np.sin(zc * (2 * math.pi / 0.022)))
    isT = (trim > 0.5) & (suit > 0.5)
    isG = (glow > 0.5) & (suit > 0.5)
    for k in range(3):
        col = np.where(isG, GLOW_RGB[k], np.where(isT, TRIM_RGB[k] * (0.8 + 0.2 * shade), SUIT_RGB[k] * shade))
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


SLIM = (("thigh", 0.025), ("calf", 0.025), ("upperarm", 0.025), ("lowerarm", 0.025), ("spine_02", 0.015), ("spine_03", 0.015),
        ("spine_04", 0.015), ("spine_05", 0.015), ("clavicle", 0.010))


def make_armored(body, arm, dom):
    """The body worn under armor, SK_Orc_Armored: the same mesh and skin,
      - 2.5 cm slimmer on the limbs and 1.5 cm on the chest (each vertex moved in along its normal by the amounts of
        SLIM weighted by its own bone weights, so the change fades out by itself at hips, neck and wrists): rigid
        plates sized to the bare body either stood off it like barrels or let it through in patches;
      - without hands, from 3 cm above the wrist: the gauntlets are the hands;
      - with the ears laid flat: their tips came through the helmets.
    The armor is fitted against this body."""
    o = body.copy()
    o.data = body.data.copy()
    o.name = body.name + "_Armored"
    bpy.context.scene.collection.objects.link(o)
    o.parent = body.parent
    o.matrix_parent_inverse = Matrix.Identity(4)
    names = {g.index: g.name for g in body.vertex_groups}
    head = _pts(body, dom, lambda b: b == "head")
    hx = (min(p.x for p in head) + max(p.x for p in head)) / 2
    top = max(p.z for p in head)
    for v in o.data.vertices:
        tot = sum(ge.weight for ge in v.groups) or 1.0
        d = sum(ge.weight * amt for ge in v.groups for pre, amt in SLIM if names[ge.group].startswith(pre)) / tot
        if d > 0:
            v.co = v.co - v.normal * d
        if dom.get(v.index) == "head" and top - 0.24 < v.co.z < top - 0.05 and abs(v.co.x - hx) > 0.112:
            v.co.x = hx + math.copysign(0.112, v.co.x - hx)
    wr = {sd: (_bh(arm, "hand" + sd), (_bh(arm, "hand" + sd) - _bh(arm, "lowerarm" + sd)).normalized()) for sd in ("_r", "_l")}
    bm = bmesh.new()
    bm.from_mesh(o.data)
    dead = []
    for fc in bm.faces:
        bones = [dom.get(v.index, "") for v in fc.verts]
        if any(b.startswith(HANDY) for b in bones):
            dead.append(fc)
            continue
        for sd, (W, ax) in wr.items():
            if all(b.endswith(sd) and b.startswith("lowerarm") for b in bones) and (fc.calc_center_median() - W).dot(ax) > -0.03:
                dead.append(fc)
    bmesh.ops.delete(bm, geom=dead, context="FACES")
    bm.to_mesh(o.data)
    bm.free()
    o.data.update()
    return o


def do_armor():
    d = out_dir("OrcArmor")
    arm, body, tex, s, bones = humanoid(src("Enemies", "Orc", "Base_Rigged_Orc.glb"), "Orc", 2.10, d)
    dom = body_regions(body, arm)
    paint_suit(body, arm, dom)                     # for the review renders; do_orc writes the game's textures
    worn = make_armored(body, arm, dom)
    worn_dom = body_regions(worn, arm)
    report = {"pieces": {}, "textures": {}}
    for fn in (1, 2):
        meshes = append_glb(src("Enemies", "Orc", "Orc_Armor_%d.glb" % fn))
        src_ob = meshes[0]
        flatten([src_ob])
        report["textures"]["Armor%d" % fn] = save_textures(src_ob.data.materials[0], "OrcArmor%d" % fn, d)
        src_ob.data.materials[0].name = "M_OrcArmor%d" % fn
        groups = piece_groups(src_ob, fn)
        for (st, slot, side), verts in sorted(groups.items(), key=lambda kv: (kv[0][0], kv[0][1], kv[0][2] != "r")):
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
                info = fit_piece(p, worn, arm, worn_dom, st, slot, sd, fn == 1 and sd == "l", body, dom)
                report["pieces"]["%s_%s_%s" % (st, slot, sd or "c")] = info
                print("FIT", st, slot, sd, info)
        bpy.data.objects.remove(src_ob)
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
