# Exports the shotgun (Glyphbound/SourceArt/Weapons/Shotgun/Shotgun.blend, built by shotgun_build_blend.py) as
# static-mesh FBX files for Unreal, plus its four textures. The game animates the moving parts in code.
#   SM_Shotgun_Body            everything that does not move, one material slot (Shotgun_Atlas): the rebuilt Tripo
#                              shell with the cables, the cell, the loading port and the energy bar joined in.
#                              UV 1 ("ImbueU") drives the energy bar's strip: U = 0 at its muzzle end, 1 at its rear
#                              end; V = 1 marks glow that is not part of the strip (always lit). The material splits
#                              the strip into 13 cells, like the pistol's and the rifle's bars.
#   SM_Shotgun_Trigger         the trigger, pivot at its top (hinge)
#   SM_Shotgun_Pump            the ribbed sleeve around the forend, pivot at its rest place; it slides back along
#                              "pump_slide_offset" and returns
#   SM_Shotgun_Magazine        one shell, pivot at its rest place inside the loading port; "magazine_out_offset" is its
#                              way out of the port (the shotgun loads shell by shell)
#   SM_Shotgun_WeakPointLight  the lens on the sight's rear lower panel (lit only on weak-point hits)
#   SM_Shotgun_AmmoDisplay     the 2:1 hologram pane in the sight's rear window, for the ammo count
# Source frame: muzzle at -Y, up +Z, metres, 1.03 m long. Export frame: turned +90 deg around Z so the muzzle points
# to +X (Unreal forward), origin at each part's pivot (the body's is the hand point, top of the pistol grip). Same FBX
# settings as the pistol and the rifle.
# Every part is a copy of its mesh in Shotgun.blend, moved: nothing is rebuilt here, because a bmesh round trip drops
# custom normals (the rifle's first export did, and the gun shaded soft). Pivots, the pump's slide, the shell's way
# out and the strip's place are custom properties written by shotgun_build_blend.py.
# Usage: blender -b Shotgun.blend --python shotgun_export_fbx.py -- <out_dir>
import bpy, sys, os, json, math
import numpy as np
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
os.makedirs(os.path.join(out_dir, "Export"), exist_ok=True)
os.makedirs(os.path.join(out_dir, "Textures"), exist_ok=True)

scene = bpy.context.scene
# Unreal mesh: (object in Shotgun.blend, second UV map for the strip)
PARTS = {
    "SM_Shotgun_Body": ("Shotgun_Body", True),
    "SM_Shotgun_Trigger": ("Shotgun_Trigger", True),
    "SM_Shotgun_Pump": ("Shotgun_Pump", True),
    "SM_Shotgun_WeakPointLight": ("Shotgun_WeakPointLight", False),
    "SM_Shotgun_AmmoDisplay": ("Shotgun_AmmoDisplay", False),
    "SM_Shotgun_Magazine": ("Shotgun_Magazine", False),
}
missing = [src for src, _ in PARTS.values() if bpy.data.objects.get(src) is None]
if missing or "strip_y" not in scene:
    raise SystemExit("shotgun_export_fbx: Shotgun.blend lacks %s (run shotgun_build_blend.py)" % (missing or "strip_y"))
HAND = Vector(bpy.data.objects["Shotgun_Body"]["pivot"])
STRIP_Y0, STRIP_Y1 = scene["strip_y"]
STRIP_HALF_W, STRIP_ZMIN = scene["strip_half_w"], scene["strip_zmin"]
TURN = Matrix.Rotation(math.pi / 2, 4, "Z")
TURN3 = TURN.to_3x3()


def cm(v):
    """A source-frame vector (metres) in the export frame, in centimetres: what the game's art table holds."""
    return [round(c * 100, 2) for c in (TURN3 @ Vector(v))]


def add_imbue_uv(me):
    """Second UV map from the vertex positions (source frame), see the header."""
    layer = me.uv_layers.new(name="ImbueU", do_init=False)
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    loop_vert = np.empty(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", loop_vert)
    c = co.reshape(-1, 3)[loop_vert]
    u = (c[:, 1] - STRIP_Y0) / (STRIP_Y1 - STRIP_Y0)
    strip = (c[:, 1] >= STRIP_Y0 - 1e-4) & (c[:, 1] <= STRIP_Y1 + 1e-4) & (c[:, 2] > STRIP_ZMIN) & \
            (np.abs(c[:, 0]) < STRIP_HALF_W + 0.0005)
    # V is flipped by the FBX import (Unreal V = 1 - Blender V): Unreal gets V = 0 on the strip, 1 off it.
    uvs = np.stack([u, np.where(strip, 1.0, 0.0)], axis=1).astype(np.float32)
    layer.uv.foreach_set("vector", uvs.ravel())
    me.uv_layers.active_index = 0
    return int(strip.sum())


def build(name, source, imbue):
    """A copy of the part's mesh (normals kept), moved so its pivot is the origin and turned so the muzzle points to +X."""
    pivot = Vector(source["pivot"])
    me = source.data.copy()
    me.name = name
    strip_loops = add_imbue_uv(me) if imbue else 0
    me.transform(TURN @ Matrix.Translation(-pivot))
    me.update()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    info = {"tris": sum(len(p.vertices) - 2 for p in me.polygons),
            "min_cm": [round(float(v) * 100, 2) for v in co.min(axis=0)],
            "max_cm": [round(float(v) * 100, 2) for v in co.max(axis=0)],
            "materials": [m.name for m in me.materials], "uv_maps": [l.name for l in me.uv_layers],
            "custom_normals": bool(me.has_custom_normals), "strip_loops": strip_loops,
            "offset_from_body_cm": cm(pivot - HAND)}
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


# The muzzle: the front end of the upper barrel (the lower tube is the magazine tube).
body = bpy.data.objects["Shotgun_Body"].data
co = np.empty(len(body.vertices) * 3, np.float32)
body.vertices.foreach_get("co", co)
co = co.reshape(-1, 3)
upper = co[co[:, 2] > 0.105]
front = upper[upper[:, 1] < upper[:, 1].min() + 0.004]
muzzle = Vector((float((front[:, 0].min() + front[:, 0].max()) / 2), float(front[:, 1].min()),
                 float((front[:, 2].min() + front[:, 2].max()) / 2)))
pane = bpy.data.objects["Shotgun_AmmoDisplay"].data
pane_co = [v.co.copy() for v in pane.vertices]
pane_centre = sum(pane_co, Vector()) / len(pane_co)

report = {"hand_point_source": [round(v, 4) for v in HAND], "length_cm": round(float(np.ptp(co[:, 1])) * 100, 1), "parts": {}}
for name, (src, imbue) in PARTS.items():
    ob, info = build(name, bpy.data.objects[src], imbue)
    report["parts"][name] = info
    export(ob)
if report["parts"]["SM_Shotgun_Body"]["strip_loops"] == 0:
    raise SystemExit("shotgun_export_fbx: no face corner of the body is on the energy strip")

report["trigger_offset_cm"] = report["parts"]["SM_Shotgun_Trigger"]["offset_from_body_cm"]
report["pump_rest_offset_cm"] = report["parts"]["SM_Shotgun_Pump"]["offset_from_body_cm"]
report["pump_slide_offset_cm"] = cm(bpy.data.objects["Shotgun_Pump"]["slide"])
report["magazine_rest_offset_cm"] = report["parts"]["SM_Shotgun_Magazine"]["offset_from_body_cm"]
report["magazine_out_offset_cm"] = cm(bpy.data.objects["Shotgun_Magazine"]["out"])
report["muzzle_tip_cm"] = cm(muzzle - HAND)
report["ammo_display_centre_cm"] = cm(pane_centre - HAND)
report["strip_y_source"] = [round(STRIP_Y0, 4), round(STRIP_Y1, 4)]

# Textures, as PNG: albedo, ORM (AO, roughness, metallic), normal (OpenGL, +Y) and the glow mask.
for name in ("T_Shotgun_BaseColor", "T_Shotgun_ORM", "T_Shotgun_Normal", "T_Shotgun_EmissiveMask"):
    im = bpy.data.images[name].copy()
    im.filepath_raw = os.path.join(out_dir, "Textures", name + ".png")
    im.file_format = "PNG"
    im.save()
    print("TEXTURE", name)

with open(os.path.join(out_dir, "export_report.json"), "w") as f:
    json.dump(report, f, indent=2)
print("REPORT", json.dumps(report))
