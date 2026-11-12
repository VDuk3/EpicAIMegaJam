# Prints, for each material of the pistol, the node types and the Principled BSDF inputs (value or what feeds it),
# so the Unreal materials can copy the look. Usage: blender -b <Pistol.blend> --python pistol_list_materials.py
import bpy

KEYS = ["Base Color", "Metallic", "Roughness", "Alpha", "Emission Color", "Emission Strength", "Coat Weight",
        "Transmission Weight"]

def fmt(v):
    try:
        return tuple(round(x, 3) for x in v)
    except TypeError:
        return round(v, 3)

for m in bpy.data.materials:
    if not m.use_nodes:
        print("MAT", m.name, "no nodes", fmt(m.diffuse_color))
        continue
    nodes = m.node_tree.nodes
    print("MAT", m.name, "blend=", getattr(m, "surface_render_method", "?"), "nodes=",
          sorted({n.bl_idname.replace("ShaderNode", "") for n in nodes}))
    for n in nodes:
        if n.bl_idname in ("ShaderNodeBsdfPrincipled", "ShaderNodeEmission", "ShaderNodeBsdfTransparent"):
            parts = []
            for k in KEYS + ["Color", "Strength"]:
                s = n.inputs.get(k)
                if s is None:
                    continue
                if s.is_linked:
                    parts.append("%s<-%s" % (k, s.links[0].from_node.bl_idname.replace("ShaderNode", "")))
                elif hasattr(s, "default_value"):
                    parts.append("%s=%s" % (k, fmt(s.default_value)))
            print("   ", n.bl_idname.replace("ShaderNode", ""), n.name, "; ".join(parts))
        if n.bl_idname == "ShaderNodeRGB":
            print("    RGB", n.name, fmt(n.outputs[0].default_value))
        if n.bl_idname == "ShaderNodeValue":
            print("    VALUE", n.name, fmt(n.outputs[0].default_value))
        if n.bl_idname == "ShaderNodeMixShader":
            print("    MIX", n.name, "fac=", fmt(n.inputs[0].default_value), "linked=", n.inputs[0].is_linked)
