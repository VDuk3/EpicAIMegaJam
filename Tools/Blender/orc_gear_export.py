# orc_gear_export.py - the horde chief's shield and heavy pistol (ASSETS delivery of 2026-11-14, Tripo FBX + JPG maps
# in Glyphbound/SourceArt/Enemies/OrcShield and Weapons/OrcPistol) prepared for Unreal.
#   blender -b --python Tools/Blender/orc_gear_export.py -- <stage_dir> inspect      views of both as delivered
#   blender -b --python Tools/Blender/orc_gear_export.py -- <stage_dir> export       SM_OrcShield.fbx, SM_OrcPistol.fbx, report.json
# Export frame (the same as the enemy rifle): metres, Z up, front to +X, FBX with axis_forward -Z, axis_up Y.
#   Shield: its face looks to +X, origin at the middle of its back (where the forearm lies), sized SHIELD_HEIGHT.
#   Pistol: barrel along +X, origin at the top of the grip, sized PISTOL_LENGTH, a Muzzle point in the report.
# The turns below were read off the inspect views (written next to each constant).
import bpy, bmesh, sys, os, json, math
from mathutils import Matrix, Vector

ARGS = sys.argv[sys.argv.index("--") + 1:]
STAGE, MODE = ARGS[0], ARGS[1]
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "Glyphbound", "SourceArt"))
ASSETS = {
    "OrcShield": os.path.join(ROOT, "Enemies", "OrcShield"),
    "OrcPistol": os.path.join(ROOT, "Weapons", "OrcPistol"),
}
SHIELD_HEIGHT = 1.15      # m, on the 2.10 m orc (the chief is scaled 1.65 with everything he carries)
PISTOL_LENGTH = 0.52      # m: a heavy hand cannon
# Filled in after looking at the inspect views: rotation (axis, degrees) steps applied in order, then the grip point.
# As delivered: the shield's face looks to -Y and its handle is on the +Y side, 46 % up; the pistol's muzzle points to -X
# and its grip is at the +X end. Fractions below are measured on the views (ortho, 720 px per metre) after the turn.
TURN = {"OrcShield": [("Z", 90)], "OrcPistol": [("Z", 180)]}
GRIP = {"OrcPistol": (0.278, 0.275)}      # the hand's place on the grip: along the gun from its back end, up from its lowest point
MUZZLE_UP = 0.634                         # the bore's height as a fraction of the gun's height (a tusk under the barrel reaches as far forward)
HANDLE_UP = 0.465                         # the shield's handle: this far up its height, on its back


def load(name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(ASSETS[name], name + ".fbx"))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    others = [o.type + ":" + o.name for o in bpy.context.scene.objects if o.type != "MESH"]
    for o in meshes:
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # one material with the delivered base colour, so the views show what it is
    mat = bpy.data.materials.new("M_" + name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(ASSETS[name], name + "_BaseColor.jpg"))
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    o.data.materials.clear()
    o.data.materials.append(mat)
    for p in o.data.polygons:
        p.material_index = 0
    print("LOADED %s: %d mesh objects joined, %d verts, %d polys, uv layers %s, other objects %s" % (
        name, len(meshes), len(o.data.vertices), len(o.data.polygons), [u.name for u in o.data.uv_layers], others))
    return o


def bounds(o):
    vs = [v.co for v in o.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    return lo, hi


def render_views(o, name, out, tag):
    lo, hi = bounds(o)
    c = (lo + hi) / 2
    size = max(hi - lo)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x = 900
    scene.render.resolution_y = 900
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    scene.world = world
    world.color = (0.25, 0.25, 0.27)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = size * 1.25
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    views = {"front_from_minusY": (Vector((0, -1, 0)), (math.pi / 2, 0, 0)),
             "back_from_plusY": (Vector((0, 1, 0)), (math.pi / 2, 0, math.pi)),
             "right_from_plusX": (Vector((1, 0, 0)), (math.pi / 2, 0, math.pi / 2)),
             "left_from_minusX": (Vector((-1, 0, 0)), (math.pi / 2, 0, -math.pi / 2)),
             "top_from_plusZ": (Vector((0, 0, 1)), (0, 0, 0))}
    for vname, (d, rot) in views.items():
        cam.location = c + d * size * 3
        cam.rotation_euler = rot
        scene.render.filepath = os.path.join(out, "%s_%s_%s.png" % (name, tag, vname))
        bpy.ops.render.render(write_still=True)
    print("VIEWS %s %s: bounds lo (%.3f, %.3f, %.3f) hi (%.3f, %.3f, %.3f)" % (name, tag, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z))


def islands(o):
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table()
    seen = set()
    out = []
    for v in bm.verts:
        if v.index in seen:
            continue
        stack = [v]
        seen.add(v.index)
        n = 0
        lo = Vector(v.co)
        hi = Vector(v.co)
        while stack:
            x = stack.pop()
            n += 1
            for i in range(3):
                lo[i] = min(lo[i], x.co[i])
                hi[i] = max(hi[i], x.co[i])
            for e in x.link_edges:
                w = e.other_vert(x)
                if w.index not in seen:
                    seen.add(w.index)
                    stack.append(w)
        out.append((n, lo, hi))
    bm.free()
    return sorted(out, key=lambda t: -t[0])


def apply_turns(o, name):
    for axis, deg in TURN[name]:
        o.data.transform(Matrix.Rotation(math.radians(deg), 4, axis))
    o.data.update()


def export_static(o, path):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", use_mesh_modifiers=False,
                             add_leaf_bones=False, bake_anim=False, use_custom_props=False, path_mode="STRIP",
                             axis_forward="-Z", axis_up="Y")
    print("WROTE", path)


def v3(v, scale=100.0):
    return [round(v.x * scale, 2), round(-v.y * scale, 2), round(v.z * scale, 2)]


os.makedirs(STAGE, exist_ok=True)
if MODE == "inspect":
    out = os.path.join(STAGE, "inspect")
    os.makedirs(out, exist_ok=True)
    for name in ASSETS:
        o = load(name)
        isl = islands(o)
        print("ISLANDS %s: %d; largest %s" % (name, len(isl), [(n, tuple(round(c, 3) for c in lo), tuple(round(c, 3) for c in hi)) for n, lo, hi in isl[:6]]))
        render_views(o, name, out, "raw")
        if TURN[name]:
            apply_turns(o, name)
            render_views(o, name, out, "turned")
elif MODE == "export":
    report = {}
    for name in ASSETS:
        o = load(name)
        apply_turns(o, name)
        lo, hi = bounds(o)
        if name == "OrcShield":
            s = SHIELD_HEIGHT / (hi.z - lo.z)
            # origin: the handle on its back (lowest X after the turn), which the left hand holds
            origin = Vector((lo.x + 0.03, (lo.y + hi.y) / 2, lo.z + (hi.z - lo.z) * HANDLE_UP))
            o.data.transform(Matrix.Scale(s, 4) @ Matrix.Translation(-origin))
            lo, hi = bounds(o)
            report[name] = {"scale": s, "size_cm": v3(hi - lo), "box_lo_cm": v3(lo), "box_hi_cm": v3(hi)}
        else:
            s = PISTOL_LENGTH / (hi.x - lo.x)
            gx, gz = GRIP[name]
            origin = Vector((lo.x + (hi.x - lo.x) * gx, (lo.y + hi.y) / 2, lo.z + (hi.z - lo.z) * gz))
            o.data.transform(Matrix.Scale(s, 4) @ Matrix.Translation(-origin))
            lo, hi = bounds(o)
            muzzle = Vector((hi.x, 0.0, lo.z + (hi.z - lo.z) * MUZZLE_UP))
            report[name] = {"scale": s, "size_cm": v3(hi - lo), "sockets": {"Muzzle": v3(muzzle)}}
        o.data.update()
        o.name = "SM_" + name
        o.data.name = "SM_" + name
        export_static(o, os.path.join(STAGE, "SM_%s.fbx" % name))
        render_views(o, name, os.path.join(STAGE, "inspect"), "export")
    with open(os.path.join(STAGE, "report.json"), "w") as f:
        json.dump(report, f, indent=1)
    print("REPORT", json.dumps(report))


def pack_mr(name):
    """T_<name>_MR.png for the shared enemy material (glTF layout: G roughness, B metallic) from the two delivered maps."""
    import numpy as np
    maps = {}
    for kind in ("Roughness", "Metallic"):
        img = bpy.data.images.load(os.path.join(ASSETS[name], "%s_%s.jpg" % (name, kind)))
        img.colorspace_settings.name = "Non-Color"
        w, h = img.size
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        maps[kind] = px.reshape(h, w, 4)[..., 0]
    h, w = maps["Roughness"].shape
    out = np.ones((h, w, 4), dtype=np.float32)
    out[..., 1] = maps["Roughness"]
    out[..., 2] = maps["Metallic"]
    img = bpy.data.images.new("T_%s_MR" % name, w, h, alpha=False)
    img.colorspace_settings.name = "Non-Color"
    img.pixels.foreach_set(out.ravel())
    img.filepath_raw = os.path.join(STAGE, "T_%s_MR.png" % name)
    img.file_format = "PNG"
    img.save()
    print("PACKED %s MR %dx%d: roughness mean %.2f, metallic mean %.2f" % (name, w, h, float(maps["Roughness"].mean()), float(maps["Metallic"].mean())))


if MODE == "export":
    for name in ASSETS:
        pack_mr(name)
