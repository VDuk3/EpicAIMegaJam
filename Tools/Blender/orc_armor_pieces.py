# Contact sheets of the orc armor pieces AS DELIVERED (before any fit), one sheet per slot: four sets x four views
# (front, right side, back, top), each piece alone and filling its tile. Also prints each piece's size and islands.
#   blender -b --python orc_armor_pieces.py -- <out_dir>
# Uses the piece classification of enemies_export_fbx.py (piece_groups), so a sheet shows exactly what the fit gets.
import sys, os, json, bpy, numpy as np
from mathutils import Vector

OUT = sys.argv[sys.argv.index("--") + 1]
os.makedirs(OUT, exist_ok=True)
HERE = os.path.dirname(os.path.abspath(__file__))
_argv = sys.argv
sys.argv = ["blender", "--", OUT]                       # no assets: only the helpers are defined
ns = {"__file__": os.path.join(HERE, "enemies_export_fbx.py")}
exec(compile(open(ns["__file__"]).read(), ns["__file__"], "exec"), ns)
sys.argv = _argv

ns["reset"]()
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.light = "STUDIO"
sc.display.shading.color_type = "TEXTURE"
sc.display.shading.show_shadows = False
TILE = 340
sc.render.resolution_x = TILE
sc.render.resolution_y = TILE
sc.render.image_settings.file_format = "PNG"
w = bpy.data.worlds.new("w")
sc.world = w
w.color = (0.42, 0.43, 0.46)
cam_d = bpy.data.cameras.new("c")
cam_d.type = "ORTHO"
cam = bpy.data.objects.new("c", cam_d)
sc.collection.objects.link(cam)
sc.camera = cam
VIEWS = [("front", (0, -1, 0)), ("side", (1, 0, 0)), ("back", (0, 1, 0)), ("top", (0, -0.001, 1))]

pieces = {}
info = {}
for fn in (1, 2):
    src_ob = ns["append_glb"](ns["src"]("Enemies", "Orc", "Orc_Armor_%d.glb" % fn))[0]
    ns["flatten"]([src_ob])
    isl = ns["islands"](src_ob)
    for (st, slot, side), verts in ns["piece_groups"](src_ob, fn).items():
        o = ns["split_by_verts"](src_ob, verts, "P_%s_%s_%s" % (st, slot, side or "c"))
        vs = [v.co for v in o.data.vertices]
        mn = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
        mx = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
        n_isl = len(ns["islands"](o))
        pieces[(st, slot, side or "c")] = (o, (mn + mx) / 2, mx - mn)
        info["%s_%s_%s" % (st, slot, side or "c")] = {
            "size_cm": [round(x * 100, 1) for x in (mx - mn)], "centre_cm": [round(x * 100, 1) for x in (mn + mx) / 2],
            "verts": len(vs), "islands": n_isl}
    src_ob.hide_render = True
json.dump(info, open(os.path.join(OUT, "pieces.json"), "w"), indent=1)
for k in sorted(info):
    print("PIECE", k, info[k])


def tile(o, c, size, d):
    for p, _, _ in pieces.values():
        p.hide_render = p is not o
    d = Vector(d).normalized()
    cam.location = c + d * 5
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam_d.ortho_scale = max(size) * 1.15
    sc.render.filepath = os.path.join(OUT, "_tile.png")
    bpy.ops.render.render(write_still=True)
    im = bpy.data.images.load(sc.render.filepath)
    a = np.empty(TILE * TILE * 4, dtype=np.float32)
    im.pixels.foreach_get(a)
    bpy.data.images.remove(im)
    return a.reshape(TILE, TILE, 4)


for slot, side in (("Helmet", "c"), ("Chest", "c"), ("Hand", "c"), ("Foot", "c"), ("Pelvis", "c"), ("Thigh", "r"),
                   ("Thigh", "l"), ("UpperArm", "r"), ("Shoulder", "r"), ("Shin", "r")):
    rows = []
    for vname, d in VIEWS:
        row = []
        for st in "ABCD":
            if (st, slot, side) in pieces:
                o, c, size = pieces[(st, slot, side)]
                row.append(tile(o, c, size, d))
            else:
                row.append(np.zeros((TILE, TILE, 4), dtype=np.float32))
        rows.append(np.concatenate(row, axis=1))
    sheet = np.concatenate(rows[::-1], axis=0)          # image rows are bottom-up
    h, wd = sheet.shape[:2]
    img = bpy.data.images.new("s", wd, h)
    img.pixels.foreach_set(sheet.ravel())
    img.filepath_raw = os.path.join(OUT, "pieces_%s_%s.png" % (slot, side))
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)
os.remove(os.path.join(OUT, "_tile.png"))
print("PIECES DONE", OUT)
