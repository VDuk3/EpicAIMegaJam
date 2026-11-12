# Headless inventory of a pistol .blend: objects, triangles, materials, images, actions.
# Usage: blender -b <file.blend> --python pistol_inspect.py -- <out_dir>
# Writes <out_dir>/inventory.txt and unpacks every embedded image to <out_dir>/images/.
import bpy, sys, os

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
out_dir = argv[0] if argv else os.path.join(os.path.dirname(bpy.data.filepath), "inspect")
os.makedirs(os.path.join(out_dir, "images"), exist_ok=True)
lines = []
P = lines.append

P("file: " + bpy.data.filepath)
P("blender: " + bpy.app.version_string)
sc = bpy.context.scene
P(f"scene: {sc.name} frames {sc.frame_start}-{sc.frame_end} fps {sc.render.fps} unit_scale {sc.unit_settings.scale_length} unit {sc.unit_settings.length_unit}")

total = 0
P("\n== objects ==")
dg = bpy.context.evaluated_depsgraph_get()
for ob in sorted(bpy.data.objects, key=lambda o: o.name):
    tris = 0
    mats = []
    if ob.type == 'MESH':
        me = ob.evaluated_get(dg).to_mesh()
        me.calc_loop_triangles()
        tris = len(me.loop_triangles)
        ob.evaluated_get(dg).to_mesh_clear()
        mats = [s.material.name if s.material else "-" for s in ob.material_slots]
        total += tris
    P(f"{ob.name} [{ob.type}] parent={ob.parent.name if ob.parent else '-'} loc={tuple(round(v,4) for v in ob.location)} "
      f"rot={tuple(round(v,4) for v in ob.rotation_euler)} scale={tuple(round(v,4) for v in ob.scale)} "
      f"dims={tuple(round(v,4) for v in ob.dimensions)} tris={tris} hide_render={ob.hide_render} "
      f"anim={ob.animation_data.action.name if ob.animation_data and ob.animation_data.action else '-'} mats={mats}")
P(f"TOTAL tris (visible+hidden meshes): {total}")

P("\n== materials ==")
for m in bpy.data.materials:
    P(f"-- {m.name} users={m.users} blend={getattr(m,'surface_render_method','?')}")
    if m.use_nodes:
        for n in m.node_tree.nodes:
            extra = ""
            if n.type == 'TEX_IMAGE' and n.image:
                extra = f" image={n.image.name} cs={n.image.colorspace_settings.name}"
            if n.type == 'GROUP' and n.node_tree:
                extra = f" group={n.node_tree.name}"
            P(f"   node {n.name} ({n.bl_idname}){extra}")
        for l in m.node_tree.links:
            P(f"   link {l.from_node.name}.{l.from_socket.identifier} -> {l.to_node.name}.{l.to_socket.identifier}")

P("\n== node groups ==")
for g in bpy.data.node_groups:
    P(f"-- {g.name} users={g.users}")
    for it in g.interface.items_tree:
        if getattr(it, "item_type", "") == 'SOCKET':
            P(f"   {it.in_out} {it.name} {it.socket_type} default={getattr(it,'default_value',None) if not hasattr(getattr(it,'default_value',None),'__len__') else tuple(it.default_value)}")

P("\n== images ==")
for im in bpy.data.images:
    P(f"{im.name} size={tuple(im.size)} packed={im.packed_file is not None} cs={im.colorspace_settings.name} users={im.users}")
    if im.size[0] > 0:
        safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in im.name)
        if not safe.lower().endswith(".png"):
            safe += ".png"
        try:
            im2 = im.copy()
            im2.filepath_raw = os.path.join(out_dir, "images", safe)
            im2.file_format = 'PNG'
            im2.save()
            bpy.data.images.remove(im2)
        except Exception as e:
            P(f"   save failed: {e}")

P("\n== actions ==")
for a in bpy.data.actions:
    P(f"{a.name} range={tuple(a.frame_range)} users={a.users}")

with open(os.path.join(out_dir, "inventory.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
print("WROTE", os.path.join(out_dir, "inventory.txt"))
