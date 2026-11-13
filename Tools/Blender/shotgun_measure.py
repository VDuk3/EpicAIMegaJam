# Measures the shotgun where parts have to be modelled on it, by casting rays at the built Shotgun.blend (frame:
# muzzle -Y, up +Z, X = 0 on the centre line, metres; printed in millimetres):
#   top      the height of the top surface along the gun (receiver block, stock comb), on three lines
#   bottom   the height of the underside between the cell and the trigger guard (the loading port's place)
#   forend   cross-sections of the lower front (how far out the left side is at each height, and the underside)
#   sight    the depth of the sight's rear, seen from behind (its window and its lower panel)
# Usage: blender -b --factory-startup --python shotgun_measure.py -- <Shotgun.blend>
import bpy, sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.open_mainfile(filepath=argv[0])
ob = bpy.data.objects["Shotgun_Body"]
dg = bpy.context.evaluated_depsgraph_get()
bvh = BVHTree.FromObject(ob, dg)


def hit(origin, direction, axis):
    h = bvh.ray_cast(Vector(origin), Vector(direction))
    return None if h[0] is None else h[0][axis] * 1000


def row(label, values):
    print("%-14s %s" % (label, " ".join("  --- " if v is None else "%6.1f" % v for v in values)))


ys = [y / 1000 for y in range(-60, 481, 15)]
print("TOP: z of the top surface; columns y =", " ".join("%6d" % (y * 1000) for y in ys))
for x in (0.0, 0.012, 0.024):
    row("x=%+.0f" % (x * 1000), [hit((x, y, 1.0), (0, 0, -1), 2) for y in ys])

ys = [y / 1000 for y in range(-260, 121, 10)]
print("BOTTOM: z of the underside; columns y =", " ".join("%6d" % (y * 1000) for y in ys))
for x in (0.0, 0.012, 0.024, 0.036):
    row("x=%+.0f" % (x * 1000), [hit((x, y, -1.0), (0, 0, 1), 2) for y in ys])

zs = [z / 1000 for z in range(20, 171, 10)]
print("FOREND left side: x of the surface seen from +X; columns z =", " ".join("%6d" % (z * 1000) for z in zs))
for y in (-0.48, -0.45, -0.42, -0.38, -0.34, -0.30, -0.26, -0.23):
    row("y=%+.0f" % (y * 1000), [hit((1.0, y, z), (-1, 0, 0), 0) for z in zs])
xs = [x / 1000 for x in range(-60, 61, 10)]
print("FOREND underside: z seen from below; columns x =", " ".join("%6d" % (x * 1000) for x in xs))
for y in (-0.48, -0.45, -0.42, -0.38, -0.34, -0.30, -0.26, -0.23):
    row("y=%+.0f" % (y * 1000), [hit((x, y, -1.0), (0, 0, 1), 2) for x in xs])

xs = [x / 1000 for x in range(-40, 56, 6)]
print("SIGHT rear: y of the surface seen from behind; columns x =", " ".join("%6d" % (x * 1000) for x in xs))
for z in range(395, 225, -8):
    row("z=%d" % z, [hit((x, 1.0, z / 1000), (0, -1, 0), 1) for x in xs])
