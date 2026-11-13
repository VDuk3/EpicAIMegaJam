# Imports the Entity's Tripo FBX (variant #26) into an empty scene and saves it as the one source .blend.
# The zip stored the texture file names with broken accents, so the FBX cannot find them: each image is relinked by its
# suffix (_basecolor, _normal) from the .fbm folder next to the FBX, then packed. Rotation (Tripo is Y-up) is applied.
# Usage: blender -b --factory-startup --python entity_import_fbx.py -- <model.fbx> <out.blend>
import bpy, sys, os, glob
argv = sys.argv[sys.argv.index("--") + 1:]
fbx, out = argv[0], argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
for im in bpy.data.images:
    if im.size[0] == 0:
        suffix = im.name.rsplit("_", 1)[-1]
        hits = glob.glob(os.path.join(os.path.dirname(fbx), "*.fbm", "*_%s.*" % suffix))
        if not hits:
            raise SystemExit("entity_import_fbx: no file for image %s" % im.name)
        im.filepath = hits[0]
        im.reload()
    im.pack()
    print("IMAGE", im.name, tuple(im.size))
ob = [o for o in bpy.data.objects if o.type == "MESH"][0]
ob.name = "Entity_Tripo26"
bpy.context.view_layer.objects.active = ob
ob.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.wm.save_as_mainfile(filepath=out)
print("SAVED", out, "tris", sum(len(p.vertices) - 2 for p in ob.data.polygons))
