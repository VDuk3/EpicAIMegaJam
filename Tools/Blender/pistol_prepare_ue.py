# Prepares the cleaned pistol for Unreal 5.9 without touching the UE project:
#  1. Bakes the Blender-only shader logic of the atlas material into plain textures:
#     - T_Pistol_BaseColor (sRGB): the Color Ramp of the luminance that the Blender material used as albedo
#       (so UE does not depend on the ramp),
#     - T_Pistol_ORM (linear): R = AO, G = roughness, B = metallic (UE's usual packing; AO now gets used),
#     - T_Pistol_Normal (linear, DirectX/UE convention: green flipped from the OpenGL source),
#     - T_Pistol_EmissiveMask (linear, grayscale): where the atlas glows in the element color
#       (Blender rule: m = min(G, B); glow = 0.65 if m > 1.35 R and m > 0.12).
#  2. Adds a second UV map "ImbueU" to the energy bar: U goes 0..1 along X like the Generated X the Blender
#     material used, so the UE material can light cells where U < ImbueRemaining.
#  3. Saves the .blend in place, only when it had to add the UV map.
# Usage: blender -b <cleaned.blend> --python pistol_prepare_ue.py -- <export_dir>
import bpy, sys, os
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
os.makedirs(out_dir, exist_ok=True)

def to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)

def to_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)

def img(prefix):
    im = next(i for i in bpy.data.images if i.name.startswith(prefix))
    w, h = im.size
    return im, np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)

def save(name, arr, colorspace):
    h, w = arr.shape[:2]
    out = bpy.data.images.new(name, w, h, alpha=False)
    out.colorspace_settings.name = colorspace
    out.pixels.foreach_set(np.ascontiguousarray(arr).ravel())
    out.filepath_raw = os.path.join(out_dir, name + ".png")
    out.file_format = "PNG"
    out.save()
    print("WROTE", out.filepath_raw)

mat = next(m for m in bpy.data.materials if m.name.startswith("tripo_material_"))
ramp = next(n for n in mat.node_tree.nodes if n.bl_idname == "ShaderNodeValToRGB").color_ramp
(p0, c0), (p1, c1) = [(e.position, np.array(e.color[:3])) for e in ramp.elements]

_, C = img("Color_")
_, O = img("ORM_")
_, N = img("NormalGL_")
lin = to_linear(C[..., :3])                       # the shader samples the sRGB atlas as linear
lum = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
t = np.clip((lum - p0) / (p1 - p0), 0, 1)[..., None]
albedo_lin = c0 + (c1 - c0) * t                   # linear ramp between the two stops
base = np.ones_like(C)
base[..., :3] = to_srgb(albedo_lin)
save("T_Pistol_BaseColor", base, "sRGB")

m = np.minimum(lin[..., 1], lin[..., 2])
glow = ((m > 1.35 * lin[..., 0]) & (m > 0.12)).astype(np.float32)
mask = np.ones_like(C)
mask[..., 0] = mask[..., 1] = mask[..., 2] = glow
save("T_Pistol_EmissiveMask", mask, "Non-Color")
print("EMISSIVE_PIXELS", int(glow.sum()))

orm = O.copy()
orm[..., 3] = 1
save("T_Pistol_ORM", orm, "Non-Color")

nrm = N.copy()
nrm[..., 1] = 1.0 - nrm[..., 1]                   # OpenGL -> DirectX
nrm[..., 3] = 1
save("T_Pistol_Normal", nrm, "Non-Color")

# second UV map on the energy bar: U = position along X (0 = muzzle end, 1 = grip end), V = 0.5
bar = bpy.data.objects["V4_energy_cells"]
me = bar.data
created = me.uv_layers.get("ImbueU") is None
uv = me.uv_layers.get("ImbueU") or me.uv_layers.new(name="ImbueU")
xs = [v.co.x for v in me.vertices]
x0, x1 = min(xs), max(xs)
for loop in me.loops:
    x = me.vertices[loop.vertex_index].co.x
    uv.data[loop.index].uv = ((x - x0) / (x1 - x0), 0.5)
print("IMBUE_UV", bar.name, "x range", round(x0, 4), round(x1, 4), "uv layers", [l.name for l in me.uv_layers])

if created:  # the master only changes the first time; later runs just bake the textures again
    bpy.ops.wm.save_mainfile(compress=True)
    print("SAVED", bpy.data.filepath)
