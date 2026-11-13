# Exports the assault rifle (Glyphbound/SourceArt/Weapons/Rifle/Rifle.blend, built by rifle_build_blend.py) as
# static-mesh FBX files for Unreal, plus its four textures. The game animates the moving parts in code.
#   SM_Rifle_Body            everything that does not move: the Tripo shell (slot carabina_roja) and the modelled top
#                            rail (slot Rifle_Rail). UV 1 ("ImbueU") drives the energy strip: U = 0 at the muzzle end
#                            of the side neon strip, 1 at its rear end; V = 1 marks glow that is not part of the strip
#                            (always lit). M_RifleAtlas splits the strip into 13 cells, like the pistol bar.
#   SM_Rifle_Trigger         the trigger blade inside the guard, pivot at its top (hinge)
#   SM_Rifle_Magazine        geometry modelled here (the source has no magazine): a curved box magazine under the
#                            magazine well in front of the trigger guard, base plate and a glowing slit on each side
#   SM_Rifle_WeakPointLight  the two oval lenses on the sides of the sight (lit only on weak-point hits)
#   SM_Rifle_AmmoDisplay     a 2:1 quad just behind the sight's eyepiece, facing the player, for the ammo count
# Source frame: muzzle at -Y, up +Z, metres, ~1.0 m long. Export frame: turned +90 deg around Z so the muzzle points
# to +X (Unreal forward), origin at the hand point (top of the pistol grip). Same FBX settings as the pistol.
# The body, trigger and lenses keep the normals of Rifle.blend (hard edges and weighted normals): their mesh data is
# copied and moved, never rebuilt, because a bmesh round trip drops custom normals (the first export did, and the gun
# shaded soft).
# Usage: blender -b Rifle.blend --python rifle_export_fbx.py -- <out_dir>
import bpy, bmesh, sys, os, json, math
import numpy as np
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
os.makedirs(os.path.join(out_dir, "Export"), exist_ok=True)
os.makedirs(os.path.join(out_dir, "Textures"), exist_ok=True)

scene = bpy.context.scene
PARTS = {name: bpy.data.objects.get(name) for name in ("Rifle_Body", "Rifle_Trigger", "Rifle_Lens")}
if not all(PARTS.values()):
    raise SystemExit("rifle_export_fbx: Rifle.blend has no Rifle_Body/Rifle_Trigger/Rifle_Lens (run rifle_build_blend.py)")
CX = -0.0055                       # the model's centre line in X (its left and right panels mirror around it)
HAND = Vector((CX, 0.19, 0.0))     # top of the pistol grip
TRIGGER_PIVOT = Vector((CX, 0.125, 0.003))
MAG_PIVOT = Vector((CX, 0.032, -0.028))
MAG_OUT = Vector((0.0, -0.02, -0.17))   # slide-out vector of the magazine on reload, source frame
TURN = Matrix.Rotation(math.pi / 2, 4, "Z")
SHARP_ANGLE = 30.0   # degrees, for the parts modelled here
# Neon strip span along Y (source frame): from its muzzle end to its rear end, below the sight.
STRIP_Y0, STRIP_Y1, STRIP_ZMAX = -0.345, 0.085, 0.1


def mat(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def add_imbue_uv(me):
    """Second UV map from the vertex positions (source frame), see the header."""
    layer = me.uv_layers.new(name="ImbueU", do_init=False)
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    loop_vert = np.empty(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", loop_vert)
    c = co.reshape(-1, 3)[loop_vert]
    u = (c[:, 1] - STRIP_Y0) / (STRIP_Y1 - STRIP_Y0)
    strip = (c[:, 1] >= STRIP_Y0) & (c[:, 1] <= STRIP_Y1) & (c[:, 2] < STRIP_ZMAX)
    # V is flipped by the FBX import (Unreal V = 1 - Blender V): Unreal gets V = 0 on the strip, 1 off it.
    uvs = np.stack([u, np.where(strip, 1.0, 0.0)], axis=1).astype(np.float32)
    layer.uv.foreach_set("vector", uvs.ravel())
    me.uv_layers.active_index = 0


def describe(name, me, materials, pivot):
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
    info = {"tris": sum(len(p.vertices) - 2 for p in me.polygons),
            "min": [round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)],
            "max": [round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)],
            "materials": [m.name for m in materials], "uv_maps": [l.name for l in me.uv_layers],
            "custom_normals": bool(me.has_custom_normals),
            "offset_from_body": [round(v, 4) for v in (TURN.to_3x3() @ (pivot - HAND))]}
    print("PART", name, json.dumps(info))
    return info


def from_blend(name, source, pivot, imbue=False, material=None):
    """A part that exists in Rifle.blend: a copy of its mesh (normals kept), moved so pivot is the origin and turned
    so the muzzle points to +X."""
    me = source.data.copy()
    me.name = name
    if imbue:
        add_imbue_uv(me)
    if material is not None:
        me.materials.clear()
        me.materials.append(material)
    me.transform(TURN @ Matrix.Translation(-pivot))
    me.update()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    return ob, describe(name, me, list(me.materials), pivot)


def modelled(name, pivot, extra, slot_materials):
    """A part modelled here (magazine, display pane)."""
    bm = bmesh.new()
    extra(bm)
    bm.transform(TURN @ Matrix.Translation(-pivot))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))
    for m in slot_materials:
        me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    return ob, describe(name, me, slot_materials, pivot)


def export(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(out_dir, "Export", ob.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", use_mesh_modifiers=False,
                             add_leaf_bones=False, bake_anim=False, use_custom_props=False, path_mode="STRIP",
                             axis_forward="-Z", axis_up="Y")
    print("WROTE", path)


# ---- the magazine (new geometry) ----
MAG_HALF_W = 0.0125
MAG_TOP_Z, MAG_BOTTOM_Z = -0.028, -0.163
MAG_TOP_Y = (0.006, 0.050)          # back and front faces at the top (front = smaller Y, towards the muzzle)
MAG_FORWARD = 0.04                  # how far the bottom curves towards the muzzle
MAG_RINGS = 8


def mag_section(t):
    """Front and back Y and the Z of the magazine at t (0 top, 1 bottom): a gentle forward curve."""
    z = MAG_TOP_Z + (MAG_BOTTOM_Z - MAG_TOP_Z) * t
    shift = MAG_FORWARD * t * t
    return MAG_TOP_Y[0] - shift, MAG_TOP_Y[1] - shift, z


def add_box_rings(bm, rings, mat_index):
    """rings: list of (y_front, y_back, z, half_w), top to bottom; a closed tube with caps."""
    loops = []
    for yf, yb, z, hw in rings:
        loops.append([bm.verts.new((CX - hw, yf, z)), bm.verts.new((CX + hw, yf, z)),
                      bm.verts.new((CX + hw, yb, z)), bm.verts.new((CX - hw, yb, z))])
    faces = []
    for a, b in zip(loops, loops[1:]):
        for i in range(4):
            j = (i + 1) % 4
            faces.append(bm.faces.new((a[i], a[j], b[j], b[i])))
    faces.append(bm.faces.new(loops[0]))
    faces.append(bm.faces.new(list(reversed(loops[-1]))))
    for f in faces:
        f.material_index = mat_index
    return faces


def add_magazine(bm):
    uv = bm.loops.layers.uv.get("UVMap") or bm.loops.layers.uv.new("UVMap")
    rings = []
    for k in range(MAG_RINGS + 1):
        yf, yb, z = mag_section(k / MAG_RINGS)
        rings.append((yf, yb, z, MAG_HALF_W))
    body = add_box_rings(bm, rings, 0)
    # base plate: a little wider and longer, 9 mm tall
    yf, yb, z = mag_section(1.0)
    plate = add_box_rings(bm, [(yf - 0.004, yb + 0.004, z + 0.002, MAG_HALF_W + 0.0015),
                               (yf - 0.004, yb + 0.004, z - 0.007, MAG_HALF_W + 0.0015)], 1)
    # glowing round-counter slit on each side: a thin quad 0.4 mm proud of the side, from 25% to 80% of the length
    for side in (-1, 1):
        x = CX + side * (MAG_HALF_W + 0.0004)
        pts = []
        for t in (0.25, 0.8):
            yf, yb, z = mag_section(t)
            mid = (yf + yb) / 2
            pts.append((mid - 0.003, z)); pts.append((mid + 0.003, z))
        quad = [bm.verts.new((x, pts[0][0], pts[0][1])), bm.verts.new((x, pts[1][0], pts[1][1])),
                bm.verts.new((x, pts[3][0], pts[3][1])), bm.verts.new((x, pts[2][0], pts[2][1]))]
        f = bm.faces.new(quad)
        f.material_index = 2
        f.normal_update()
        if (f.normal.x > 0) != (side > 0):
            f.normal_flip()
    bmesh.ops.recalc_face_normals(bm, faces=body + plate)
    for f in bm.faces:
        for loop in f.loops:
            loop[uv].uv = (0.5, 0.5)


def add_display_plane(bm):
    """The ammo display: a 2:1 hologram pane 1.5 mm behind the rear face of the sight's eyepiece (Y = 0.211, facing +Y,
    the player), inside that face (X -0.0185 .. 0.0085, half a millimetre in from its edges). The first version spanned
    the whole sight 4 cm behind its main face and seen from the first-person camera it stuck out past the sight.
    UV: U to the viewer's right (-X seen from behind), V up in Blender (down in Unreal), so a texture reads upright."""
    y, x0, x1, z0, z1 = 0.2125, -0.0185, 0.0085, 0.12175, 0.13525
    uv = bm.loops.layers.uv.get("UVMap") or bm.loops.layers.uv.new("UVMap")
    vs = [bm.verts.new((x, y, z)) for x, z in [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]]
    face = bm.faces.new(vs)
    face.normal_update()
    if face.normal.y < 0:
        face.normal_flip()
    for loop in face.loops:
        co = loop.vert.co
        loop[uv].uv = ((x1 - co.x) / (x1 - x0), (co.z - z0) / (z1 - z0))


report = {"hand_point_source": [round(v, 4) for v in HAND], "parts": {}}
jobs = [
    ("SM_Rifle_Body", lambda: from_blend("SM_Rifle_Body", PARTS["Rifle_Body"], HAND, imbue=True)),
    ("SM_Rifle_Trigger", lambda: from_blend("SM_Rifle_Trigger", PARTS["Rifle_Trigger"], TRIGGER_PIVOT, imbue=True)),
    ("SM_Rifle_WeakPointLight", lambda: from_blend("SM_Rifle_WeakPointLight", PARTS["Rifle_Lens"], HAND,
                                                   material=mat("Rifle_WeakPoint"))),
    ("SM_Rifle_AmmoDisplay", lambda: modelled("SM_Rifle_AmmoDisplay", HAND, add_display_plane, [mat("Holo_AmmoDisplay")])),
    # The magazine has three slots: body, base plate, glowing slit.
    ("SM_Rifle_Magazine", lambda: modelled("SM_Rifle_Magazine", MAG_PIVOT, add_magazine,
                                           [mat("Rifle_Mag_Body"), mat("Rifle_Mag_Plate"), mat("Rifle_Mag_Glow")])),
]
for name, job in jobs:
    ob, info = job()
    report["parts"][name] = info
    export(ob)

report["magazine_out_offset"] = [round(v, 4) for v in (TURN.to_3x3() @ MAG_OUT)]
report["strip_y_source"] = [STRIP_Y0, STRIP_Y1]

# Textures, as PNG: albedo, ORM (AO, roughness, metallic), normal (OpenGL, +Y) and the neon mask.
for name in ("T_Rifle_BaseColor", "T_Rifle_ORM", "T_Rifle_Normal", "T_Rifle_EmissiveMask"):
    im = bpy.data.images[name].copy()
    im.filepath_raw = os.path.join(out_dir, "Textures", name + ".png")
    im.file_format = "PNG"
    im.save()
    print("TEXTURE", name)

with open(os.path.join(out_dir, "export_report.json"), "w") as f:
    json.dump(report, f, indent=2)
print("REPORT", json.dumps(report))
