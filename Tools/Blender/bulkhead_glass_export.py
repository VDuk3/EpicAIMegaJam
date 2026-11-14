# bulkhead_glass_export.py - models the glass bulkhead at the east end of the transfer bay (the wall that breaks).
#   blender -b --python Tools/Blender/bulkhead_glass_export.py -- <out_dir>
# Owner, 2026-11-14: "the glass that breaks before the stairs looks cheap; there are windows of thick dirty glass in the
# map already, take them as the model". Those windows are the deep wing's: heavy worn steel, cut corners, thick glass
# with the deep water's glow behind it. This is the same thing as a whole bulkhead:
#   SM_GB_Bulkhead_Frame      stays: jambs on plinths, lintel, corner gussets, threshold, bolt heads      (slot Steel)
#   SM_GB_Bulkhead_Bars       stays: the glazing bars, their hub plates and every pane's corner plates    (slot Steel)
#   SM_GB_Bulkhead_Pane_<c><r>  breaks, each on its own: 4 x 2 panes of glass 10 cm thick (c 0..3 from -Y, r L or U) (slot Glass)
# (owner, after seeing the first version in the game: "each pane on its own, and what holds the glass stays when
# only the glass breaks"). Every mesh keeps the wall's origin, so all of them are placed at the same point.
# Metres, Z up, the wall across the X axis (its thickness), centred on Y, standing on Z = 0. Opening: 10.24 x 5.75 m.
# Each pane's two big faces are unwrapped 0..1 (the material puts grime along a pane's edges with it).
import bpy, bmesh, sys, os, math
from mathutils import Matrix, Vector

OUT = sys.argv[sys.argv.index("--") + 1]
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)

HALF_W, TOP = 5.12, 5.75          # the opening in the kit's wall
JAMB, LINTEL = 0.42, 0.43         # frame member sizes
IN_Y, IN_TOP = HALF_W - JAMB, TOP - LINTEL
SILL = 0.04
BAR = 0.14                        # glazing bar width
COLS = [-IN_Y, -IN_Y / 2.0, 0.0, IN_Y / 2.0, IN_Y]
TRANSOM = 3.0

mats = {}


def mat(name):
    if name not in mats:
        mats[name] = bpy.data.materials.new(name)
    return mats[name]


def new_obj(name, bm, material):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(mat(material))
    return ob


def box(x, y, z, material, bevel=0.012, pane_uv=False):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    sx, sy, sz = x[1] - x[0], y[1] - y[0], z[1] - z[0]
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    bmesh.ops.translate(bm, vec=((x[0] + x[1]) / 2, (y[0] + y[1]) / 2, (z[0] + z[1]) / 2), verts=bm.verts)
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for l in f.loops:
            c = l.vert.co
            if pane_uv:
                l[uv].uv = ((c.y - y[0]) / sy, (c.z - z[0]) / sz)
            else:
                l[uv].uv = (c.y + c.x, c.z)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(bevel, 0.45 * min(sx, sy, sz)), segments=1, affect='EDGES', profile=0.5)
    return new_obj("box", bm, material)


def prism(tri, x, material):
    """A triangle in (y, z) extruded from x[0] to x[1]."""
    bm = bmesh.new()
    a = [bm.verts.new((x[0], p[0], p[1])) for p in tri]
    b = [bm.verts.new((x[1], p[0], p[1])) for p in tri]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new((a[j], a[i], b[i], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for l in f.loops:
            l[uv].uv = (l.vert.co.y, l.vert.co.z)
    return new_obj("prism", bm, material)


def bolt(y, z, x_face, material="Steel", r=0.028, h=0.022):
    """A hexagonal bolt head on the face at x_face (its sign gives the side it stands on)."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=r, radius2=r * 0.9, depth=h)
    bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi / 2 * (1 if x_face > 0 else -1), 3, 'Y'), verts=bm.verts)
    bmesh.ops.translate(bm, vec=(x_face + math.copysign(h / 2, x_face), y, z), verts=bm.verts)
    bm.loops.layers.uv.new("UVMap")
    return new_obj("bolt", bm, material)


def disc(y, z, x, r, material="Steel"):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=10, radius1=r, radius2=r, depth=x[1] - x[0])
    bmesh.ops.rotate(bm, cent=(0, 0, 0), matrix=Matrix.Rotation(math.pi / 2, 3, 'Y'), verts=bm.verts)
    bmesh.ops.translate(bm, vec=((x[0] + x[1]) / 2, y, z), verts=bm.verts)
    bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.01, segments=1, affect='EDGES')
    bm.loops.layers.uv.new("UVMap")
    return new_obj("disc", bm, material)


def join(name, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name
    ob.data.name = name
    # flat faces, hard edges: it is plate steel
    for p in ob.data.polygons:
        p.use_smooth = False
    return ob


def export(ob):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, ob.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, path_mode='STRIP')
    me = ob.data
    lo = Vector((min(v.co.x for v in me.vertices), min(v.co.y for v in me.vertices), min(v.co.z for v in me.vertices)))
    hi = Vector((max(v.co.x for v in me.vertices), max(v.co.y for v in me.vertices), max(v.co.z for v in me.vertices)))
    print('EXPORTED %s: %d polygons, slots %s, from %s to %s' % (ob.name, len(me.polygons), [s.material.name for s in ob.material_slots], tuple(round(c, 2) for c in lo), tuple(round(c, 2) for c in hi)))


# ---- the frame (stays)
parts = []
D = 0.20                                             # half depth of the frame
for s in (-1, 1):
    y0, y1 = sorted((s * HALF_W, s * IN_Y))
    parts.append(box((-D, D), (y0, y1), (0.0, TOP), "Steel", 0.02))
    parts.append(box((-D - 0.07, D + 0.07), (y0 - 0.0, y1 + (0.06 if s < 0 else 0.0) - (0.06 if s > 0 else 0.0)), (0.0, 0.38), "Steel", 0.025))      # plinth
    parts.append(box((-D - 0.035, D + 0.035), (y0, y1), (2.92, 3.08), "Steel", 0.012))                                                           # a band at the transom's height
    # top corner gusset: the cut corner of the deep wing's windows
    c = s * IN_Y
    parts.append(prism([(c, IN_TOP), (c, IN_TOP - 0.85), (c - s * 0.85, IN_TOP)], (-D + 0.04, D - 0.04), "Steel"))
    for z in [0.62 + 0.46 * i for i in range(11)]:
        for face in (-D, D):
            parts.append(bolt(s * (HALF_W - JAMB / 2.0), z, face))
parts.append(box((-D, D), (-IN_Y, IN_Y), (IN_TOP, TOP), "Steel", 0.02))
parts.append(box((-D - 0.035, D + 0.035), (-IN_Y, IN_Y), (IN_TOP + 0.02, IN_TOP + 0.12), "Steel", 0.012))                                         # the lintel's lower flange
parts.append(box((-D, D), (-IN_Y, IN_Y), (0.0, SILL), "Steel", 0.008))                                                                           # threshold
n = 15
for i in range(n):
    y = -IN_Y + 0.35 + (2 * IN_Y - 0.7) * i / (n - 1)
    for face in (-D, D):
        parts.append(bolt(y, IN_TOP + 0.27, face))
frame = join("SM_GB_Bulkhead_Frame", parts)

# ---- the bars (stay) and the panes (break one by one)
parts = []
panes = []
G = 0.05                                             # half thickness of the glass
B = 0.09                                             # half depth of a glazing bar
rows = [(SILL + BAR * 0.8, TRANSOM - BAR / 2.0), (TRANSOM + BAR / 2.0, IN_TOP)]
parts.append(box((-B, B), (-IN_Y, IN_Y), (SILL, SILL + BAR * 0.8), "Steel", 0.012))                 # bottom rail
parts.append(box((-B, B), (-IN_Y, IN_Y), (TRANSOM - BAR / 2.0, TRANSOM + BAR / 2.0), "Steel", 0.012))
for y in COLS[1:-1]:
    parts.append(box((-B, B), (y - BAR / 2.0, y + BAR / 2.0), (SILL, IN_TOP), "Steel", 0.012))
    parts.append(disc(y, TRANSOM, (-B - 0.025, B + 0.025), 0.17))                                   # a hub plate where the bars cross
    for k in range(6):
        a = math.pi / 3 * k
        for face in (-B - 0.025, B + 0.025):
            parts.append(bolt(y + 0.115 * math.cos(a), TRANSOM + 0.115 * math.sin(a), face, r=0.018, h=0.014))
CUT = 0.26                                           # leg of a pane's corner plate
for ci in range(4):
    ya = COLS[ci] + (BAR / 2.0 if ci > 0 else 0.0)
    yb = COLS[ci + 1] - (BAR / 2.0 if ci < 3 else 0.0)
    for (za, zb) in rows:
        pane = box((-G, G), (ya, yb), (za, zb), "Glass", 0.006, pane_uv=True)
        pane.name = "SM_GB_Bulkhead_Pane_%d%s" % (ci, "L" if za < TRANSOM else "U")
        pane.data.name = pane.name
        for p in pane.data.polygons:
            p.use_smooth = False
        panes.append(pane)
        for (cy, sy) in ((ya, 1), (yb, -1)):
            for (cz, sz) in ((za, 1), (zb, -1)):
                parts.append(prism([(cy, cz), (cy + sy * CUT, cz), (cy, cz + sz * CUT)], (-G - 0.018, G + 0.018), "Steel"))
                for face in (-G - 0.018, G + 0.018):
                    parts.append(bolt(cy + sy * CUT * 0.3, cz + sz * CUT * 0.3, face, r=0.016, h=0.012))
bars = join("SM_GB_Bulkhead_Bars", parts)

for ob in [frame, bars] + panes:
    export(ob)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'bulkhead.blend'))
