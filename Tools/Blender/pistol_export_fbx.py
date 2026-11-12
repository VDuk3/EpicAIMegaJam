# Exports the pistol as four static-mesh FBX files for Unreal (the game animates the moving parts in code):
#   SM_Pistol_Body      everything that does not move (optic pane, reticle, panels, vents, rails included)
#   SM_Pistol_Trigger   pivot = Trigger_pivot_trial (Blender pulls it -8 deg around Y at frame 8)
#   SM_Pistol_Magazine  pivot = TRIAL_Magazine_Control (Blender slides it out and back, frames 24-72)
#   SM_Pistol_EnergyBar V4_energy_cells alone, so its second UV map (ImbueU) reaches UE as UV channel 1
# Every part is taken at frame 1 (rest pose), in world space, then moved so that:
#   - the hand point HAND (top of the grip, under the slide) is the origin of the body,
#   - each moving part has its pivot at its own origin,
#   - the gun is turned 180 deg around Z so the muzzle points to +X (Unreal forward) and the energy bar side
#     (Blender -Y) ends on +Y in Blender, which the FBX importer mirrors to -Y in Unreal (the side the camera sees).
# Faces with Optic_OriginalLens_Invisible (the old lens, transparent in Blender) are dropped.
# Units: Blender metres, real size (0.97 m long); the game scales the viewmodel.
# Usage: blender -b Pistol.blend --python pistol_export_fbx.py -- <out_dir>
import bpy, bmesh, sys, os, json
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
out_dir = argv[0]
os.makedirs(out_dir, exist_ok=True)

scene = bpy.context.scene
scene.frame_set(1)

TRIGGER = ["Trigger_Complete_Moving"]
MAGAZINE = ["TRIAL_Magazine_Body", "TRIAL_Magazine_Base"]
ENERGY = ["V4_energy_cells"]
DROP_MATERIALS = {"Optic_OriginalLens_Invisible"}
moving = set(TRIGGER + MAGAZINE + ENERGY)
BODY = [o.name for o in bpy.data.objects if o.type == "MESH" and o.name not in moving]

# Hand point: X of the magazine pivot (the grip axis at the top), Z of the trigger pivot (under the slide).
mag_pivot = bpy.data.objects["TRIAL_Magazine_Control"].matrix_world.translation.copy()
trig_pivot = bpy.data.objects["Trigger_pivot_trial"].matrix_world.translation.copy()
HAND = Vector((mag_pivot.x, 0.0, trig_pivot.z))
TURN = Matrix.Rotation(3.141592653589793, 4, "Z")


def build(name, sources, pivot):
    """One new mesh object = the sources in world space, with pivot at the origin, turned to face +X."""
    bm = bmesh.new()
    materials = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for src_name in sources:
        src = bpy.data.objects[src_name]
        ev = src.evaluated_get(depsgraph)
        me = ev.to_mesh()
        me.transform(src.matrix_world)
        # map this object's material slots to the joined list
        remap = []
        for slot in src.material_slots:
            mat = slot.material
            if mat is None or mat.name in DROP_MATERIALS:
                remap.append(-1)
                continue
            if mat not in materials:
                materials.append(mat)
            remap.append(materials.index(mat))
        tmp = bmesh.new()
        tmp.from_mesh(me)
        ev.to_mesh_clear()
        drop = [f for f in tmp.faces if remap and remap[f.material_index] < 0]
        bmesh.ops.delete(tmp, geom=drop, context="FACES")
        for f in tmp.faces:
            f.material_index = remap[f.material_index] if remap else 0
        tmp_me = bpy.data.meshes.new("tmp")
        tmp.to_mesh(tmp_me)
        tmp.free()
        bm.from_mesh(tmp_me)
        bpy.data.meshes.remove(tmp_me)
    bm.transform(TURN @ Matrix.Translation(-pivot))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for mat in materials:
        me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]; zs = [v.co.z for v in me.vertices]
    info = {"tris": sum(len(p.vertices) - 2 for p in me.polygons),
            "min": [round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)],
            "max": [round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)],
            "materials": [m.name for m in materials], "uv_maps": [l.name for l in me.uv_layers]}
    print("PART", name, json.dumps(info))
    return ob, info


def export(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(out_dir, ob.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", use_mesh_modifiers=False,
                             add_leaf_bones=False, bake_anim=False, use_custom_props=False, path_mode="STRIP",
                             axis_forward="-Z", axis_up="Y")
    print("WROTE", path)


report = {"hand_point_blender": [round(v, 4) for v in HAND], "parts": {}}
for name, sources, pivot in [("SM_Pistol_Body", BODY, HAND),
                             ("SM_Pistol_Trigger", TRIGGER, trig_pivot),
                             ("SM_Pistol_Magazine", MAGAZINE, mag_pivot),
                             ("SM_Pistol_EnergyBar", ENERGY, HAND)]:
    ob, info = build(name, sources, pivot)
    # where the part's origin sits relative to the body origin, in the exported (turned) frame, metres
    offset = TURN @ (pivot - HAND)
    info["offset_from_body"] = [round(v, 4) for v in offset]
    report["parts"][name] = info
    export(ob)

# Motion of the moving parts, in the exported frame (metres / degrees), from the Blender keys.
mag_out = Vector((0.4465, 0.0, -0.4661))  # TRIAL_Magazine_Control location at frames 40-56
report["magazine_out_offset"] = [round(v, 4) for v in (TURN.to_3x3() @ (mag_out - mag_pivot))]
report["trigger_pull_deg_about_blender_y"] = -8.0
with open(os.path.join(out_dir, "export_report.json"), "w") as f:
    json.dump(report, f, indent=2)
print("REPORT", json.dumps(report))
