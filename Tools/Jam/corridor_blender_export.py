# corridor_blender_export.py - turns the Spaceship Corridor model into two meshes Unreal can take.
#   blender -b --disable-autoexec "<Spaceship corridor.blend>" --python Tools/Jam/corridor_blender_export.py -- <out_dir>
# Writes SM_ShipCorridor_Body.fbx (what the player touches: shell, floor grid, far doors, roof vents, pipes) and
# SM_ShipCorridor_Details.fbx (what only dresses it: cables, wires, crates, extinguishers, axes, screens).
# Each group is joined into one mesh with its modifiers applied; material slots keep the model's material names,
# which Tools/Jam/import_corridor.py maps to our materials. The .blend itself is not changed (nothing is saved).
import bpy
import os
import sys

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
out_dir = argv[0] if argv else 'C:/GameJam/Glyphbound/Saved/corridor_export/'
out_dir = out_dir.replace('\\', '/').rstrip('/') + '/'
os.makedirs(out_dir, exist_ok=True)      # the folder is deleted after each import (the FBX files are intermediates)

BODY = ('Corridor', 'Floor_Grid', 'BackDoor_1', 'BackDoor_2', 'Roof_Vent', 'Vent_Grid', 'Roof_Pipes')
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
    bpy.ops.object.join()
    joined = view_layer.objects.active
    joined.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.export_scene.fbx(filepath=out_dir + name + '.fbx', use_selection=True, object_types={'MESH'},
                             use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False,
                             path_mode='STRIP')
    me = joined.data
    print('EXPORTED %s: %d polygons, %d material slots %s, dims %.2f x %.2f x %.2f' % (
        name, len(me.polygons), len(joined.material_slots), [s.material.name for s in joined.material_slots if s.material],
        joined.dimensions.x, joined.dimensions.y, joined.dimensions.z))


meshes = [o for o in bpy.data.objects if o.type == 'MESH' and not o.hide_render and o.name in view_layer.objects]
body = [o for o in meshes if o.name in BODY]
details = [o for o in meshes if o.name not in BODY]
export('SM_ShipCorridor_Body', body)
export('SM_ShipCorridor_Details', details)
