# Finds what lies under given pixels of a review render: object, material and atlas pixel (top-left origin).
# Rebuilds the same camera as pistol_render_views.py for the named view.
# Usage: blender -b <file.blend> --python pistol_pick.py -- <view> <res> <px,py> [...]
import bpy, sys, math
from mathutils import Vector
from bpy_extras.view3d_utils import region_2d_to_vector_3d  # noqa (not used in background)

argv = sys.argv[sys.argv.index("--") + 1:]
view, res, pix = argv[0], int(argv[1]), argv[2:]
VIEWS = {
    "side_minusY": ((0, -1, 0.05), 1.25, 50), "side_plusY": ((0, 1, 0.05), 1.25, 50),
    "top": ((0.0, -0.05, 1), 1.25, 50), "front34": ((-1, -0.7, 0.35), 1.25, 50),
    "fps_approx": ((1, -0.35, 0.45), 0.75, 35),
}
sc = bpy.context.scene
sc.frame_set(1)
W, H = res, int(res * 0.75)
dg = bpy.context.evaluated_depsgraph_get()
pts = []
for ob in sc.objects:
    if ob.type == "MESH" and not ob.hide_render:
        pts += [ob.matrix_world @ Vector(c) for c in ob.bound_box]
mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
center, size = (mn + mx) / 2, (mx - mn).length
d, mul, lens = VIEWS[view]
loc = center + Vector(d).normalized() * size * mul
rot = (center - loc).to_track_quat("-Z", "Y").to_matrix()
sensor = 36.0
for p in pix:
    px, py = [float(v) for v in p.split(",")]
    # sensor fit AUTO: the larger dimension (width) uses the 36 mm sensor
    sx = (px + 0.5 - W / 2) / W * sensor
    sy = -(py + 0.5 - H / 2) / W * sensor
    dirc = rot @ Vector((sx, sy, -lens)).normalized()
    hit, pos, nrm, idx, ob, mat = sc.ray_cast(dg, loc, dirc)
    if not hit:
        print("PICK", p, "nothing"); continue
    ev = ob.evaluated_get(dg)
    me = ev.data
    poly = me.polygons[idx]
    slot = ob.material_slots[poly.material_index].material.name if ob.material_slots else "-"
    uv = None
    if me.uv_layers.active:
        # barycentric-free approximation: average UV of the polygon's loops weighted by distance to hit
        local = ob.matrix_world.inverted() @ pos
        best = None
        ws, acc = 0.0, Vector((0, 0))
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            w = 1.0 / max((v - local).length, 1e-6)
            acc += Vector(me.uv_layers.active.data[li].uv) * w
            ws += w
        uv = acc / ws
    ap = (round(uv.x * 2048), round((1 - uv.y) * 2048)) if uv else None
    print("PICK", p, ob.name, "face", idx, "mat", slot, "uv", tuple(round(c, 4) for c in uv) if uv else None, "atlas_px", ap,
          "world", tuple(round(c, 4) for c in pos))
