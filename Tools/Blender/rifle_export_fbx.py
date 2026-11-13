# Exports the assault rifle (Glyphbound/SourceArt/Weapons/Rifle/Rifle.blend, one Tripo mesh made of ~1000 loose
# panels) as static-mesh FBX files for Unreal, plus its two textures. The game animates the moving parts in code.
#   SM_Rifle_Body            everything that does not move. UV 1 ("ImbueU") drives the energy strip: U = 0 at the
#                            muzzle end of the side neon strip, 1 at its rear end; V = 1 marks glow that is not part of
#                            the strip (always lit). M_RifleAtlas splits the strip into 13 cells, like the pistol bar.
#   SM_Rifle_Trigger         the trigger blade inside the guard, pivot at its top (hinge)
#   SM_Rifle_Magazine        NEW geometry modelled here (the source has no magazine): a curved box magazine under the
#                            magazine well in front of the trigger guard, base plate and a glowing slit on each side
#   SM_Rifle_WeakPointLight  the two oval lenses on the sides of the sight (lit only on weak-point hits)
#   SM_Rifle_AmmoDisplay     a 2:1 quad just behind the sight's eyepiece, facing the player, for the ammo count
# Source frame: muzzle at -Y, up +Z, metres, ~1.0 m long. Export frame: turned +90 deg around Z so the muzzle points
# to +X (Unreal forward), origin at the hand point (top of the pistol grip). Same FBX settings as the pistol.
# Usage: blender -b Rifle.blend --python rifle_export_fbx.py -- <out_dir>
import bpy, bmesh, sys, os, json, math
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
os.makedirs(os.path.join(out_dir, "Export"), exist_ok=True)
os.makedirs(os.path.join(out_dir, "Textures"), exist_ok=True)

scene = bpy.context.scene
SRC = [o for o in scene.objects if o.type == "MESH"][0]
CX = -0.0055                       # the model's centre line in X (its left and right panels mirror around it)
HAND = Vector((CX, 0.19, 0.0))     # top of the pistol grip
TRIGGER_PIVOT = Vector((CX, 0.125, 0.003))
MAG_PIVOT = Vector((CX, 0.032, -0.028))
MAG_OUT = Vector((0.0, -0.02, -0.17))   # slide-out vector of the magazine on reload, source frame
TURN = Matrix.Rotation(math.pi / 2, 4, "Z")
# Neon strip span along Y (source frame): from its muzzle end to its rear end, below the sight.
STRIP_Y0, STRIP_Y1, STRIP_ZMAX = -0.345, 0.085, 0.1


def islands(bm):
    seen, out = set(), []
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, comp = [f], []
        seen.add(f.index)
        while stack:
            a = stack.pop()
            comp.append(a.index)
            for e in a.edges:
                for b in e.link_faces:
                    if b.index not in seen:
                        seen.add(b.index)
                        stack.append(b)
        out.append(comp)
    return out


def inside(vs, lo, hi):
    return all(lo[i] <= min(v.co[i] for v in vs) and max(v.co[i] for v in vs) <= hi[i] for i in range(3))


# Pick the trigger and the lenses by the bounding box of their loose panels.
src_bm = bmesh.new()
src_bm.from_mesh(SRC.data)
src_bm.faces.ensure_lookup_table()
TRIGGER_FACES, LENS_FACES = set(), set()
for comp in islands(src_bm):
    vs = {v for i in comp for v in src_bm.faces[i].verts}
    if inside(vs, (-0.0105, 0.116, -0.043), (-0.0005, 0.134, 0.0035)):
        TRIGGER_FACES.update(comp)
    elif len(comp) >= 15 and inside(vs, (-0.042, 0.104, 0.105), (0.031, 0.170, 0.140)) \
            and max(v.co.x for v in vs) - min(v.co.x for v in vs) < 0.006:
        LENS_FACES.update(comp)
src_bm.free()
print("PICK trigger faces", len(TRIGGER_FACES), "lens faces", len(LENS_FACES))
if not TRIGGER_FACES or not LENS_FACES:
    raise SystemExit("rifle_export_fbx: trigger or lens panels not found")


def set_imbue_uv(bm):
    uv = bm.loops.layers.uv.get("ImbueU") or bm.loops.layers.uv.new("ImbueU")
    for f in bm.faces:
        for loop in f.loops:
            co = loop.vert.co
            u = (co.y - STRIP_Y0) / (STRIP_Y1 - STRIP_Y0)
            strip = STRIP_Y0 <= co.y <= STRIP_Y1 and co.z < STRIP_ZMAX
            # V is flipped by the FBX import (Unreal V = 1 - Blender V): Unreal gets V = 0 on the strip, 1 off it.
            loop[uv].uv = (u, 1.0 if strip else 0.0)


def build(name, pivot, faces=None, material=None, extra=None, imbue=False, slot_materials=None):
    """One mesh: the source faces whose index is in faces (all if None), or only the extra geometry, moved so pivot is
    the origin and turned so the muzzle points to +X."""
    bm = bmesh.new()
    materials = slot_materials or ([material] if material is not None else list(SRC.data.materials))
    if faces is not None or extra is None:
        bm.from_mesh(SRC.data)
        bm.faces.ensure_lookup_table()
        if faces is not None:
            bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in faces], context="FACES")
        if material is not None:
            for f in bm.faces:
                f.material_index = 0
    if extra is not None:
        extra(bm)
    if imbue:
        set_imbue_uv(bm)
    bm.transform(TURN @ Matrix.Translation(-pivot))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in materials:
        me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
    info = {"tris": sum(len(p.vertices) - 2 for p in me.polygons),
            "min": [round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)],
            "max": [round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)],
            "materials": [m.name for m in materials], "uv_maps": [l.name for l in me.uv_layers],
            "offset_from_body": [round(v, 4) for v in (TURN.to_3x3() @ (pivot - HAND))]}
    print("PART", name, json.dumps(info))
    return ob, info


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


def add_box_rings(bm, rings, mat):
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
        f.material_index = mat
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
    slit = []
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
        slit.append(f)
    bmesh.ops.recalc_face_normals(bm, faces=body + plate)
    for f in bm.faces:
        for loop in f.loops:
            loop[uv].uv = (0.5, 0.5)


def add_display_plane(bm):
    """The ammo display: a 2:1 hologram pane 1.5 mm behind the rear face of the sight's eyepiece (Y = 0.211, facing +Y,
    the player), inside that face (X -0.0185 .. 0.0085, half a millimetre in from its edges). The first version spanned the whole sight 4 cm behind its main
    face and seen from the first-person camera it stuck out past the sight (owner: fix the imperfections).
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


# Slot-naming materials for import_rifle.py (not used in Blender renders).
def mat(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


all_faces = set(range(len(SRC.data.polygons)))
report = {"hand_point_source": [round(v, 4) for v in HAND], "parts": {}}
for name, pivot, kwargs in [
        ("SM_Rifle_Body", HAND, {"faces": all_faces - TRIGGER_FACES - LENS_FACES, "imbue": True}),
        ("SM_Rifle_Trigger", TRIGGER_PIVOT, {"faces": TRIGGER_FACES, "imbue": True}),
        ("SM_Rifle_WeakPointLight", HAND, {"faces": LENS_FACES, "material": mat("Rifle_WeakPoint")}),
        ("SM_Rifle_AmmoDisplay", HAND, {"extra": add_display_plane, "material": mat("Holo_AmmoDisplay")}),
]:
    ob, info = build(name, pivot, **kwargs)
    report["parts"][name] = info
    export(ob)

# The magazine has three slots: body, base plate, glowing slit.
bm_mats = [mat("Rifle_Mag_Body"), mat("Rifle_Mag_Plate"), mat("Rifle_Mag_Glow")]
ob, info = build("SM_Rifle_Magazine", MAG_PIVOT, extra=add_magazine, slot_materials=bm_mats)
report["parts"]["SM_Rifle_Magazine"] = info
export(ob)

report["magazine_out_offset"] = [round(v, 4) for v in (TURN.to_3x3() @ MAG_OUT)]
report["strip_y_source"] = [STRIP_Y0, STRIP_Y1]

# Textures: the Tripo albedo and its neon mask, as PNG.
for image_name, out_name in [(next(i.name for i in bpy.data.images if i.name.startswith("Color_")), "T_Rifle_BaseColor"),
                             ("neon_mask", "T_Rifle_EmissiveMask")]:
    im = bpy.data.images[image_name].copy()
    im.filepath_raw = os.path.join(out_dir, "Textures", out_name + ".png")
    im.file_format = "PNG"
    im.save()
    print("TEXTURE", out_name)

with open(os.path.join(out_dir, "export_report.json"), "w") as f:
    json.dump(report, f, indent=2)
print("REPORT", json.dumps(report))
