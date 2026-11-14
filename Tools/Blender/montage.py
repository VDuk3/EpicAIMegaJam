# Tiles PNG files in a row: blender -b --python montage.py -- <out.png> <in1.png> <in2.png> ...  (all the same size)
import sys, bpy, numpy as np
a = sys.argv[sys.argv.index("--") + 1:]
out, ins = a[0], a[1:]
arrs = []
for p in ins:
    im = bpy.data.images.load(p)
    w, h = im.size
    x = np.empty(w * h * 4, dtype=np.float32)
    im.pixels.foreach_get(x)
    arrs.append(x.reshape(h, w, 4))
sheet = np.concatenate(arrs, axis=1)
h, w = sheet.shape[:2]
o = bpy.data.images.new("m", w, h)
o.pixels.foreach_set(sheet.ravel())
o.filepath_raw = out
o.file_format = "PNG"
o.save()
