# Imports the rifle GLB into an empty scene, packs its images and saves it as the one source .blend.
# Usage: blender -b --factory-startup --python rifle_import_glb.py -- <model.glb> <out.blend>
import bpy, sys
argv = sys.argv[sys.argv.index("--") + 1:]
glb, out = argv[0], argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
for im in bpy.data.images:
    if im.packed_file is None and im.size[0] > 0:
        im.pack()
bpy.ops.wm.save_as_mainfile(filepath=out)
print("SAVED", out)
