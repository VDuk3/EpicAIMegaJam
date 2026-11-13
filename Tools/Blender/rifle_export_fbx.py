# Exports the assault rifle (Glyphbound/SourceArt/Weapons/Rifle/Rifle.blend, built by rifle_build_blend.py) as
# static-mesh FBX files for Unreal, plus its four textures. The game animates the moving parts in code.
#   SM_Rifle_Body            everything that does not move: the Tripo shell (slot carabina_roja, which also carries the
#                            glowing strips of the two energy bars) and the modelled metal parts (slot Rifle_Rail: top
#                            rail, energy-bar bezels, ammo-screen housing). UV 1 ("ImbueU") drives the energy strips:
#                            U = 0 at their muzzle end, 1 at their rear end; V = 1 marks glow that is not part of the
#                            strips (always lit). M_RifleAtlas splits the strips into 13 cells, like the pistol bar.
#   SM_Rifle_Trigger         the trigger blade inside the guard, pivot at its top (hinge)
#   SM_Rifle_Magazine        a curved box magazine under the magazine well, base plate and a glowing slit on each side
#   SM_Rifle_WeakPointLight  the two oval lenses on the sides of the sight (lit only on weak-point hits)
#   SM_Rifle_AmmoDisplay     the 2:1 hologram pane in the housing behind the sight's eyepiece, for the ammo count
# Source frame: muzzle at -Y, up +Z, metres, ~1.0 m long. Export frame: turned +90 deg around Z so the muzzle points
# to +X (Unreal forward), origin at each part's pivot (the body's is the hand point, top of the pistol grip). Same FBX
# settings as the pistol.
# Every part is a copy of its mesh in Rifle.blend, moved: nothing is rebuilt here, because a bmesh round trip drops
# custom normals (the first export did, and the gun shaded soft). Pivots and the strip's span are custom properties
# written by rifle_build_blend.py.
# Usage: blender -b Rifle.blend --python rifle_export_fbx.py -- <out_dir>
import bpy, sys, os, json, math
import numpy as np
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
os.makedirs(os.path.join(out_dir, "Export"), exist_ok=True)
os.makedirs(os.path.join(out_dir, "Textures"), exist_ok=True)

scene = bpy.context.scene
# Unreal mesh: (object in Rifle.blend, second UV map for the strip, replacement material or None)
PARTS = {
    "SM_Rifle_Body": ("Rifle_Body", True, None),
    "SM_Rifle_Trigger": ("Rifle_Trigger", True, None),
    "SM_Rifle_WeakPointLight": ("Rifle_Lens", False, "Rifle_WeakPoint"),
    "SM_Rifle_AmmoDisplay": ("Rifle_AmmoDisplay", False, None),
    "SM_Rifle_Magazine": ("Rifle_Magazine", False, None),
}
missing = [src for src, _, _ in PARTS.values() if bpy.data.objects.get(src) is None]
if missing or "strip_y" not in scene:
    raise SystemExit("rifle_export_fbx: Rifle.blend lacks %s (run rifle_build_blend.py)" % (missing or "strip_y"))
HAND = Vector(bpy.data.objects["Rifle_Body"]["pivot"])
STRIP_Y0, STRIP_Y1 = scene["strip_y"]
STRIP_ZMAX = scene["strip_zmax"]
TURN = Matrix.Rotation(math.pi / 2, 4, "Z")


def add_imbue_uv(me):
    """Second UV map from the vertex positions (source frame), see the header."""
    layer = me.uv_layers.new(name="ImbueU", do_init=False)
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    loop_vert = np.empty(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", loop_vert)
    c = co.reshape(-1, 3)[loop_vert]
    u = (c[:, 1] - STRIP_Y0) / (STRIP_Y1 - STRIP_Y0)
    strip = (c[:, 1] >= STRIP_Y0 - 1e-5) & (c[:, 1] <= STRIP_Y1 + 1e-5) & (c[:, 2] < STRIP_ZMAX)
    # V is flipped by the FBX import (Unreal V = 1 - Blender V): Unreal gets V = 0 on the strip, 1 off it.
    uvs = np.stack([u, np.where(strip, 1.0, 0.0)], axis=1).astype(np.float32)
    layer.uv.foreach_set("vector", uvs.ravel())
    me.uv_layers.active_index = 0


def build(name, source, imbue, material):
    """A copy of the part's mesh (normals kept), moved so its pivot is the origin and turned so the muzzle points to +X."""
    pivot = Vector(source["pivot"])
    me = source.data.copy()
    me.name = name
    if imbue:
        add_imbue_uv(me)
    if material is not None:
        me.materials.clear()
        me.materials.append(bpy.data.materials.get(material) or bpy.data.materials.new(material))
    me.transform(TURN @ Matrix.Translation(-pivot))
    me.update()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
    info = {"tris": sum(len(p.vertices) - 2 for p in me.polygons),
            "min": [round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)],
            "max": [round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)],
            "materials": [m.name for m in me.materials], "uv_maps": [l.name for l in me.uv_layers],
            "custom_normals": bool(me.has_custom_normals),
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


report = {"hand_point_source": [round(v, 4) for v in HAND], "parts": {}}
for name, (src, imbue, material) in PARTS.items():
    ob, info = build(name, bpy.data.objects[src], imbue, material)
    report["parts"][name] = info
    export(ob)

mag_out = Vector(bpy.data.objects["Rifle_Magazine"]["out"])
report["magazine_out_offset"] = [round(v, 4) for v in (TURN.to_3x3() @ mag_out)]
report["strip_y_source"] = [round(STRIP_Y0, 4), round(STRIP_Y1, 4)]

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
