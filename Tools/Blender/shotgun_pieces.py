# Lists the shotgun's loose pieces in the straightened frame (muzzle -Y, up +Z, gun centred on X = 0, metres), says
# how much texture chosen groups of pieces really have, and renders those groups to find and judge parts (trigger,
# sight, cables...). Piece numbers are stable: pieces are sorted by vertex count, then by their lowest vertex index.
# Modes for the group renders:
#   red   the group painted red on a grey gun (which piece is which);
#   lit   the gun's own texture under the review lights, the group framed close up from its side, from above, from
#         behind and from the player's eye;
#   flat  the same frames unlit, texels as they are (nearest-texel sampling).
# Per group it prints: surface, texels, texels per cm, UV islands, and the colour of its texels (mean, spread, and the
# shares that are copper/orange, glowing cyan, or black).
# Usage: blender -b --factory-startup --python shotgun_pieces.py -- <model.fbx|model.glb> <out_dir>
#        ["3,4;7;12,13" groups] [red|lit|flat]
import bpy, bmesh, sys, os, math, glob
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src, out_dir = argv[0], argv[1]
groups = [[int(i) for i in g.split(",")] for g in argv[2].split(";")] if len(argv) > 2 and argv[2] else []
mode = argv[3] if len(argv) > 3 else "red"
os.makedirs(out_dir, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
if src.lower().endswith(".fbx"):
    bpy.ops.import_scene.fbx(filepath=src)
    for im in bpy.data.images:
        if im.size[0] == 0:
            hits = glob.glob(os.path.join(os.path.dirname(src), "*.fbm", "*"))
            if len(hits) == 1:
                im.filepath = hits[0]
                im.reload()
else:
    bpy.ops.import_scene.gltf(filepath=src)
ob = [o for o in bpy.data.objects if o.type == "MESH"][0]
bpy.context.view_layer.objects.active = ob
ob.select_set(True)
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
me = ob.data
# The GLB splits vertices along UV seams: weld them. UVs live on face corners, so they are kept.
wb = bmesh.new()
wb.from_mesh(me)
bmesh.ops.remove_doubles(wb, verts=wb.verts, dist=0.00001)
wb.to_mesh(me)
wb.free()
xy = np.array([v.co[:2] for v in me.vertices])
width, yaw = min((np.ptp(xy[:, 0] * math.cos(a) - xy[:, 1] * math.sin(a)), a) for a in np.radians(np.arange(-25, 25, 0.05)))
ob.rotation_mode = "XYZ"                                # the glTF importer leaves quaternions
ob.rotation_euler = (0.0, 0.0, yaw)
bpy.ops.object.transform_apply(rotation=True)
xs = [v.co.x for v in me.vertices]
ob.location.x = -(min(xs) + max(xs)) / 2
bpy.ops.object.transform_apply(location=True)
print("STRAIGHTEN yaw_deg", round(math.degrees(yaw), 2), "width", round(float(width), 4))

bm = bmesh.new()
bm.from_mesh(me)
bm.verts.index_update()
bm.faces.ensure_lookup_table()
seen, pieces = set(), []
for v in bm.verts:
    if v.index in seen:
        continue
    stack, vs = [v], []
    seen.add(v.index)
    while stack:
        a = stack.pop()
        vs.append(a)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b.index not in seen:
                seen.add(b.index)
                stack.append(b)
    pieces.append(vs)
pieces.sort(key=lambda vs: (-len(vs), min(v.index for v in vs)))
piece_of_face, piece_points = {}, []
for i, vs in enumerate(pieces):
    lo = [min(v.co[k] for v in vs) for k in range(3)]
    hi = [max(v.co[k] for v in vs) for k in range(3)]
    faces = {f.index for v in vs for f in v.link_faces}
    piece_points.append([v.co.copy() for v in vs])
    for f in faces:
        piece_of_face[f] = i
    print("PIECE %2d verts %4d faces %4d  x %+.3f %+.3f  y %+.3f %+.3f  z %.3f %.3f" %
          (i, len(vs), len(faces), lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))

# ---- how much texture each group has ----
image = next((im for im in bpy.data.images if im.size[0] > 0), None)
if image is not None:
    W, H = image.size
    tex = np.empty(W * H * 4, dtype=np.float32)
    image.pixels.foreach_get(tex)
    tex = tex.reshape(H, W, 4)[..., :3]
    uvl = bm.loops.layers.uv.active
    rng = np.random.default_rng(1)

    def report(name, face_ids):
        ids = list(face_ids)
        idset = set(ids)
        area3d = uv_area = 0.0
        samples = []
        for fi in ids:
            f = bm.faces[fi]
            area3d += f.calc_area()
            ls = f.loops
            for k in range(1, len(ls) - 1):
                ua, ub, uc = (np.array(l[uvl].uv) for l in (ls[0], ls[k], ls[k + 1]))
                t = abs((ub[0] - ua[0]) * (uc[1] - ua[1]) - (ub[1] - ua[1]) * (uc[0] - ua[0])) / 2
                uv_area += t
                n = min(400, max(1, int(t * W * H / 4)))            # about one sample per 4 texels
                r = rng.random((n, 2))
                flip = r.sum(axis=1) > 1
                r[flip] = 1 - r[flip]
                pts = ua + r[:, :1] * (ub - ua) + r[:, 1:] * (uc - ua)
                px = np.clip((pts[:, 0] % 1.0 * W).astype(int), 0, W - 1)
                py = np.clip((pts[:, 1] % 1.0 * H).astype(int), 0, H - 1)
                samples.append(tex[py, px])
        # UV islands: faces count as joined across an edge only where both corners carry the same UVs
        parent = {}

        def find(x):
            while parent.get(x, x) != x:
                x = parent[x]
            return x

        for fi in ids:
            for l in bm.faces[fi].loops:
                for o in l.link_loops:
                    oi = o.face.index
                    if oi in idset and oi != fi:
                        same = ((l[uvl].uv - o.link_loop_next[uvl].uv).length < 1e-5 and
                                (l.link_loop_next[uvl].uv - o[uvl].uv).length < 1e-5)
                        if same and find(fi) != find(oi):
                            parent[find(fi)] = find(oi)
        islands = len({find(fi) for fi in ids})
        col = np.concatenate(samples) if samples else np.zeros((1, 3))
        texels = uv_area * W * H
        mean, std = col.mean(axis=0), col.std(axis=0)
        warm = float(((col[:, 0] > col[:, 2] * 1.15) & (col[:, 0] > 0.08)).mean())
        cyan = float(((col[:, 2] > 0.5) & (col[:, 1] > 0.5) & (col[:, 0] < col[:, 2] * 0.6)).mean())
        dark = float((col.max(axis=1) < 0.04).mean())
        cm2 = area3d * 1e4
        print("TEXTURE %-24s faces %5d area %7.1f cm2 texels %9.0f (%5.2f%% of atlas) %5.1f px/cm islands %4d | "
              "mean %.3f %.3f %.3f std %.3f %.3f %.3f | warm %4.1f%% cyan %4.1f%% black %4.1f%%" %
              (name, len(ids), cm2, texels, 100 * uv_area, (texels / cm2) ** 0.5 if cm2 else 0, islands,
               mean[0], mean[1], mean[2], std[0], std[1], std[2], 100 * warm, 100 * cyan, 100 * dark))

    print("ATLAS", image.name, W, H)
    report("whole gun", [f.index for f in bm.faces])
    report("main body (piece 0)", [f for f, pi in piece_of_face.items() if pi == 0])
    for g in groups:
        report("group " + "_".join(str(i) for i in g), [f for f, pi in piece_of_face.items() if pi in g])
bm.free()

if groups:
    def plain(name, rgb, strength):
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        d = nt.nodes.new("ShaderNodeBsdfDiffuse")
        d.inputs["Color"].default_value = rgb + (1,)
        e = nt.nodes.new("ShaderNodeEmission")
        e.inputs["Color"].default_value = rgb + (1,)
        e.inputs["Strength"].default_value = strength
        add = nt.nodes.new("ShaderNodeAddShader")
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(d.outputs[0], add.inputs[0])
        nt.links.new(e.outputs[0], add.inputs[1])
        nt.links.new(add.outputs[0], out.inputs["Surface"])
        return m

    sc = bpy.context.scene
    if mode == "red":
        me.materials.clear()
        me.materials.append(plain("Grey", (0.45, 0.47, 0.5), 0.0))
        me.materials.append(plain("Red", (1.0, 0.05, 0.02), 0.6))
    elif mode == "flat":
        for m in bpy.data.materials:
            nt = m.node_tree
            t = next((n for n in nt.nodes if n.bl_idname == "ShaderNodeTexImage"), None)
            o = next((n for n in nt.nodes if n.bl_idname == "ShaderNodeOutputMaterial"), None)
            if t and o:
                t.interpolation = "Closest"
                em = nt.nodes.new("ShaderNodeEmission")
                nt.links.new(t.outputs["Color"], em.inputs["Color"])
                nt.links.new(em.outputs[0], o.inputs["Surface"])
    else:
        for nm, rot, en in (("Key", (48, 0, 150), 3.5), ("Fill", (65, 0, -60), 0.8)):
            ld = bpy.data.lights.new(nm, "SUN")
            ld.energy = en
            ld.angle = math.radians(3)
            lob = bpy.data.objects.new(nm, ld)
            lob.rotation_euler = [math.radians(a) for a in rot]
            sc.collection.objects.link(lob)
    sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("OPTIX", "CUDA"):
        try:
            prefs.compute_device_type = kind
            prefs.get_devices()
            for d in prefs.devices:
                d.use = True
            sc.cycles.device = "GPU"
            break
        except Exception as e:
            print("no", kind, e)
    sc.cycles.samples = 64 if mode == "lit" else 24
    sc.cycles.use_denoising = True
    sc.view_settings.view_transform = "AgX" if mode == "lit" else "Standard"
    sc.render.resolution_x, sc.render.resolution_y = (1600, 900) if mode == "red" else (1920, 1080)
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.6, 0.62, 0.66, 1) if mode == "red" else (0.20, 0.22, 0.26, 1)
    sc.world = world
    cd = bpy.data.cameras.new("C")
    cd.clip_start = 0.01
    cam = bpy.data.objects.new("C", cd)
    sc.collection.objects.link(cam)
    sc.camera = cam
    EYE = Vector((0.13, 0.42, 0.42))                     # the player's eye of shotgun_render_review.py ("fp")
    for g in groups:
        if mode == "red":
            for p in me.polygons:
                p.material_index = 1 if piece_of_face.get(p.index) in g else 0
            me.update()
        vs = [co for i in g for co in piece_points[i]]
        lo = Vector([min(v[k] for v in vs) for k in range(3)])
        hi = Vector([max(v[k] for v in vs) for k in range(3)])
        c, span = (lo + hi) / 2, max(hi - lo)
        tag = "_".join(str(i) for i in g)
        if mode == "red":
            shots = [("left", c + Vector((1, 0.25, 0.25)).normalized() * 0.55, c, 50),
                     ("right", c + Vector((-1, 0.25, 0.25)).normalized() * 0.55, c, 50),
                     ("whole", Vector((0, 0, 0.19)) + Vector((1, 0.0, 0.05)).normalized() * 1.6, Vector((0, 0, 0.19)), 50)]
        else:
            side = 1 if c.x >= 0 else -1                   # the side of the gun the group is on
            dist = span * 1.25 * 85 / 36                   # the group fills most of the image width at 85 mm
            shots = [("side", c + Vector((side, 0.0, 0.12)).normalized() * dist, c, 85),
                     ("above", c + Vector((side * 0.45, 0.0, 1)).normalized() * dist, c, 85),
                     ("rear34", c + Vector((side * 0.8, 0.9, 0.5)).normalized() * dist, c, 85),
                     ("player", EYE, c, (EYE - c).length / (span * 1.6) * 36)]
        for view, loc, target, lens in shots:
            cam.location = loc
            cam.rotation_euler = (target - loc).to_track_quat("-Z", "Y").to_euler()
            cd.lens = lens
            sc.render.filepath = os.path.join(out_dir, "piece_%s_%s_%s.png" % (mode, tag, view))
            bpy.ops.render.render(write_still=True)
            print("RENDER", os.path.basename(sc.render.filepath))
