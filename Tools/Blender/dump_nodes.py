# Prints every node's non-linked input values, Math operations and Color Ramp stops of the given materials.
# Usage: blender -b <file.blend> --python dump_nodes.py -- <MaterialName> [...]
import bpy, sys

names = sys.argv[sys.argv.index("--") + 1:]
def fmt(v):
    try:
        return tuple(round(x, 4) for x in v)
    except TypeError:
        return round(v, 4) if isinstance(v, float) else v

for name in names:
    m = bpy.data.materials.get(name) or None
    tree = m.node_tree if m else (bpy.data.node_groups.get(name))
    print("=== ", name)
    for n in tree.nodes:
        info = n.bl_idname
        if hasattr(n, "operation"):
            info += f" op={n.operation} clamp={getattr(n,'use_clamp',None)}"
        if n.bl_idname == "ShaderNodeValue":
            info += f" value={n.outputs[0].default_value}"
        print(" NODE", n.name, info)
        if n.bl_idname == "ShaderNodeValToRGB":
            cr = n.color_ramp
            print("   ramp", cr.interpolation, [(round(e.position, 4), fmt(e.color)) for e in cr.elements])
        for i in n.inputs:
            if not i.is_linked and hasattr(i, "default_value") and i.enabled:
                print("   in", i.identifier, fmt(i.default_value))
    for l in tree.links:
        print(" LINK", l.from_node.name, l.from_socket.identifier, "->", l.to_node.name, l.to_socket.identifier)
