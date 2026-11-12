# Crops regions of a texture atlas and saves them enlarged, for visual review.
# Usage: blender -b --python atlas_crop.py -- <atlas.png> <out_dir> <name:x0,y0,x1,y1> [...]
# Coordinates are image pixels with the origin at the TOP-LEFT (as an image viewer shows them).
import bpy, sys, os
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
src, out_dir, regions = argv[0], argv[1], argv[2:]
os.makedirs(out_dir, exist_ok=True)
im = bpy.data.images.load(src)
w, h = im.size
px = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]  # flip to top-left origin

for spec in regions:
    # optional channel prefix "R@", "G@", "B@": show that channel as grayscale
    ch = None
    if spec[1:2] == "@":
        ch, spec = "RGB".index(spec[0]), spec[2:]
    name, box = spec.split(":")
    x0, y0, x1, y1 = [int(v) for v in box.split(",")]
    crop = px[y0:y1, x0:x1].copy()
    if ch is not None:
        crop[..., 0:3] = crop[..., ch:ch + 1]
    scale = max(1, int(512 / max(crop.shape[0], crop.shape[1])))
    crop = np.repeat(np.repeat(crop, scale, axis=0), scale, axis=1)
    ch, cw = crop.shape[:2]
    out = bpy.data.images.new(name, cw, ch, alpha=True)
    out.colorspace_settings.name = im.colorspace_settings.name
    out.pixels = crop[::-1].ravel()
    out.filepath_raw = os.path.join(out_dir, name + ".png")
    out.file_format = 'PNG'
    out.save()
    print("CROP", name, x0, y0, x1, y1, "x", scale)
