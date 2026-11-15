# valkyrie_wings_lib.py - which vertices of the Valkyrie (Valkiria.glb, Tripo auto-rig) are her wings, and how to make
# them ride her back rigidly (overnight point 1: "rig the wings so they keep their place, stable, like the back wings of
# any fantasy humanoid"). Diagnosis: the wings are a slab of loose islands behind the T-posed arms, skinned to the
# arms, clavicles and fingers by the auto-rig, so every arm movement bent and tore them.
# Rule (per mesh island, rest pose, GLB frame: face -Y, up +Z):
#   an island is a wing piece when its middle is out from the spine (|x| > X_SPINE) and it is not part of an arm:
#   fewer than ARM_FRAC of its vertices lie within ARM_R of the arm bones' segments (clavicle to finger tips), or its
#   middle is behind the arm line (y > the arm's y + BEHIND).
# The fix: every wing vertex gets one weight, 1.0 on CHEST_BONE, and each wing half is swept back about a vertical
# axis through its root by SWEEP_DEG and lifted LIFT_DEG, so it reads as wings on the back and not a flat cross.
import bmesh, math
from mathutils import Vector, Matrix

X_SPINE = 0.075
ARM_R = 0.055
ARM_FRAC = 0.55
BEHIND = 0.025
CHEST_BONE = "spine_05"
SWEEP_DEG = 22.0
LIFT_DEG = 0.0
ARM_BONES = ("clavicle", "upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky")
LEG_BONES = ("thigh", "calf", "foot", "ball")
LEG_R = 0.09


def islands(me):
    bm = bmesh.new(); bm.from_mesh(me); bm.verts.ensure_lookup_table()
    isl = [-1] * len(bm.verts); groups = []
    for v in bm.verts:
        if isl[v.index] >= 0:
            continue
        st = [v]; isl[v.index] = len(groups); mem = []
        while st:
            x = st.pop(); mem.append(x.index)
            for e in x.link_edges:
                o = e.other_vert(x)
                if isl[o.index] < 0:
                    isl[o.index] = len(groups); st.append(o)
        groups.append(mem)
    bm.free()
    return isl, groups


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
    return (a + ab * t - p).length


def arm_segments(arm, names=ARM_BONES):
    segs = []
    for b in arm.data.bones:
        if any(b.name.startswith(k) for k in names):
            segs.append((arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local))
    return segs


def wing_vertices(body, arm, scale=1.0):
    """scale: the character's scale against the GLB (the distances above are in GLB metres, 0.894 m tall)."""
    me = body.data; mw = body.matrix_world
    x_spine, arm_r, behind, leg_r = X_SPINE * scale, ARM_R * scale, BEHIND * scale, LEG_R * scale
    isl, groups = islands(me)
    segs = arm_segments(arm)
    legs = arm_segments(arm, LEG_BONES)
    arm_y = sum((a.y + b.y) * 0.5 for a, b in segs) / max(len(segs), 1)
    wing = set()
    for mem in groups:
        pts = [mw @ me.vertices[k].co for k in mem]
        mid = sum(pts, Vector()) / len(pts)
        if abs(mid.x) <= x_spine:
            continue
        on_leg = sum(1 for p in pts if min(seg_dist(p, a, b) for a, b in legs) < leg_r)
        if on_leg / len(pts) > 0.5:
            continue
        near = sum(1 for p in pts if min(seg_dist(p, a, b) for a, b in segs) < arm_r)
        if near / len(pts) < ARM_FRAC or mid.y > arm_y + behind:
            wing.update(mem)
    return wing, arm_y


def rigidify(body, arm, wing):
    """One weight, CHEST_BONE 1.0, for every wing vertex; then the sweep about each half's root."""
    me = body.data
    chest = body.vertex_groups.get(CHEST_BONE) or body.vertex_groups.new(name=CHEST_BONE)
    for k in wing:
        for g in list(me.vertices[k].groups):
            body.vertex_groups[g.group].remove([k])
        chest.add([k], 1.0, "REPLACE")
    if SWEEP_DEG == 0.0 and LIFT_DEG == 0.0:
        return
    inv = body.matrix_world.inverted()
    for side in (1, -1):
        idx = [k for k in wing if (body.matrix_world @ me.vertices[k].co).x * side > 0]
        if not idx:
            continue
        pts = [body.matrix_world @ me.vertices[k].co for k in idx]
        # root: the wing vertex nearest the spine, at the half's mean height
        root_x = min(abs(p.x) for p in pts) * side
        root = Vector((root_x, max(p.y for p in pts), sum(p.z for p in pts) / len(pts)))
        rot = Matrix.Rotation(math.radians(SWEEP_DEG * side), 4, "Z") @ Matrix.Rotation(math.radians(LIFT_DEG * -side), 4, "Y")
        for k, p in zip(idx, pts):
            q = root + (rot @ (p - root).to_4d()).to_3d()
            me.vertices[k].co = inv @ q
