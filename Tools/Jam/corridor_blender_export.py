# corridor_blender_export.py - turns the Spaceship Corridor model into two meshes Unreal can take.
#   blender -b --disable-autoexec "<Spaceship corridor.blend>" --python Tools/Jam/corridor_blender_export.py -- <out_dir>
# Writes SM_ShipCorridor_Body.fbx (what the player touches: shell, floor grid, roof vents, pipes),
# SM_ShipCorridor_Details.fbx (what only dresses it: cables, wires, crates, extinguishers, axes, screens) and the two
# leaves of the far door, SM_ShipCorridor_DoorA.fbx and SM_ShipCorridor_DoorB.fbx, which slide open in the game. All four
# keep the model's own origin, so they are placed on the same point.
# Each group is joined into one mesh with its modifiers applied; material slots keep the model's material names,
# which Tools/Jam/import_corridor.py maps to our materials. The .blend itself is not changed (nothing is saved).
import bpy
import os
import sys

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
out_dir = argv[0] if argv else 'C:/GameJam/Glyphbound/Saved/corridor_export/'
out_dir = out_dir.replace('\\', '/').rstrip('/') + '/'
os.makedirs(out_dir, exist_ok=True)      # the folder is deleted after each import (the FBX files are intermediates)

BODY = ('Corridor', 'Floor_Grid', 'Roof_Vent', 'Vent_Grid', 'Roof_Pipes')
DOORS = (('SM_ShipCorridor_DoorA', 'BackDoor_1'), ('SM_ShipCorridor_DoorB', 'BackDoor_2'))
view_layer = bpy.context.view_layer


def export(name, objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.hide_set(False)
        o.select_set(True)
    view_layer.objects.active = objects[0]
    bpy.ops.object.make_single_user(object=True, obdata=True)
    bpy.ops.object.convert(target='MESH')            # applies the bevels of the crates
    # The file mixes UV layers called 'UVMap' and 'UVКарта'. Joined as they are, they become two channels and the
    # screens' picture lands in the second one (white screens in Unreal). One name, one channel.
    for o in objects:
        layers = o.data.uv_layers
        if len(layers):
            (layers.active_render if hasattr(layers, 'active_render') and layers.active_render else layers[0]).name = 'UVMap'
    if len(objects) > 1:
        bpy.ops.object.join()
    joined = view_layer.objects.active
    joined.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.export_scene.fbx(filepath=out_dir + name + '.fbx', use_selection=True, object_types={'MESH'},
                             use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False,
                             path_mode='STRIP')
    me = joined.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    print('EXPORTED %s: %d polygons, %d material slots %s, from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)' % (
        name, len(me.polygons), len(joined.material_slots), [s.material.name for s in joined.material_slots if s.material],
        min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)))


meshes = [o for o in bpy.data.objects if o.type == 'MESH' and not o.hide_render and o.name in view_layer.objects]
door_names = [d[1] for d in DOORS]
print('NEGATIVE SCALE: %s' % [o.name for o in meshes if o.matrix_world.determinant() < 0])
body = [o for o in meshes if o.name in BODY]
details = [o for o in meshes if o.name not in BODY and o.name not in door_names]
for mesh_name, object_name in DOORS:
    export(mesh_name, [o for o in meshes if o.name == object_name])
export('SM_ShipCorridor_Body', body)
export('SM_ShipCorridor_Details', details)
