# Prints the layout along X of the energy bar (V4_energy_cells) and of the rear display planes: the connected pieces
# of the bar with their X range, and the world bounds of the panel objects. Used to split the bar into the long
# enchant bar and the small light next to it, and to place the ammo display.
# Usage: blender -b Pistol.blend --python pistol_bar_layout.py
import bpy, bmesh

def world_bounds(ob):
    vs = [ob.matrix_world @ v.co for v in ob.data.vertices]
    return [round(min(v[i] for v in vs), 4) for i in range(3)], [round(max(v[i] for v in vs), 4) for i in range(3)]

bar = bpy.data.objects["V4_energy_cells"]
bm = bmesh.new()
bm.from_mesh(bar.data)
bm.verts.ensure_lookup_table()
seen = set()
pieces = []
for v in bm.verts:
    if v.index in seen:
        continue
    stack, comp = [v], []
    seen.add(v.index)
    while stack:
        a = stack.pop()
        comp.append(a)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b.index not in seen:
                seen.add(b.index)
                stack.append(b)
    xs = [(bar.matrix_world @ c.co).x for c in comp]
    pieces.append((round(min(xs), 4), round(max(xs), 4), len(comp)))
pieces.sort()
print("BAR pieces", len(pieces))
for p in pieces:
    print("  PIECE x", p[0], "..", p[1], "verts", p[2])
for name in ["Panel_Finish_Bezel", "Panel_Finish_Illuminated", "Panel_UI_Top", "Panel_UI_Bottom", "Panel_UI_Cell",
             "Panel_UI_Cell.004", "Panel_UI_StatusLeft", "Panel_UI_StatusRight", "V4_conformal_carrier",
             "V4_conformal_carrier.001"]:
    ob = bpy.data.objects[name]
    mn, mx = world_bounds(ob)
    n = ob.data.polygons[0].normal if ob.data.polygons else None
    print("OBJ", name, "min", mn, "max", mx, "normal", tuple(round(c, 3) for c in (ob.matrix_world.to_3x3() @ n)) if n else None)
