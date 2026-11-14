# corridor_blender_render.py - renders the Spaceship Corridor model from the player's eye, in Blender, without a window.
#   blender -b --disable-autoexec "<Spaceship corridor.blend>" --python Tools/Jam/corridor_blender_render.py -- <out_dir> [game]
# Without "game": the model as its author made it (his procedural materials, his lights).
# With "game": the look it will have in Unreal, where procedural materials do not exist: each material becomes a plain
# colour with a metal and a roughness value (the same table Tools/Jam/import_corridor.py uses), so the owner judges
# what the game will show.
import bpy
import sys
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
out_dir = argv[0] if argv else 'C:/GameJam/Glyphbound/Saved/corridor_review/'
game = len(argv) > 1 and argv[1] == 'game'

# name starts with -> (base colour, metallic, roughness, emission colour, emission strength)
GAME_LOOK = [
    ('Procedural Imperfections', (0.20, 0.21, 0.23), 0.55, 0.55, None, 0.0),
    ('Roof_Grid_Metal', (0.10, 0.10, 0.11), 0.90, 0.45, None, 0.0),
    ('Metal Pipes', (0.30, 0.29, 0.27), 0.90, 0.35, None, 0.0),
    ('Rubber', (0.03, 0.03, 0.035), 0.0, 0.45, None, 0.0),
    ('Floor_wires', (0.45, 0.02, 0.02), 0.0, 0.60, None, 0.0),
    ('Emmision', (0.0, 0.0, 0.0), 0.0, 0.5, (1.0, 0.93, 0.82), 12.0),
    ('Flickering_emmision', (0.0, 0.0, 0.0), 0.0, 0.5, (1.0, 0.75, 0.45), 8.0),
    ('Glass', (0.02, 0.03, 0.04), 0.0, 0.08, None, 0.0),
]


def simplify():
    for m in bpy.data.materials:
        for prefix, base, metal, rough, em, strength in GAME_LOOK:
            if m.name.startswith(prefix):
                m.use_nodes = True
                nt = m.node_tree
                nt.nodes.clear()
                out = nt.nodes.new('ShaderNodeOutputMaterial')
                bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
                bsdf.inputs['Base Color'].default_value = (base[0], base[1], base[2], 1.0)
                bsdf.inputs['Metallic'].default_value = metal
                bsdf.inputs['Roughness'].default_value = rough
                if em:
                    bsdf.inputs['Emission Color'].default_value = (em[0], em[1], em[2], 1.0)
                    bsdf.inputs['Emission Strength'].default_value = strength
                nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
                break


SCREENS = "C:/GameJam/Glyphbound/SourceArt/Environment/SpaceshipCorridor/Spaceship Corridor_blend/Screen_textures/"
# The screens' pictures are next to the .blend, not where the file remembers them (they render magenta otherwise).
for img in bpy.data.images:
    if img.name.startswith("Screen"):
        img.source = "FILE"
        img.filepath = SCREENS + "Screen.jpg"
        img.reload()

if game:
    simplify()
    # No planet outside: the base is under a castle. The windows look onto darkness.
    world = bpy.context.scene.world
    if world:
        world.use_nodes = True
        world.node_tree.nodes.clear()
        bg = world.node_tree.nodes.new("ShaderNodeBackground")
        bg.inputs["Color"].default_value = (0.0, 0.0, 0.0, 1.0)
        outw = world.node_tree.nodes.new("ShaderNodeOutputWorld")
        world.node_tree.links.new(bg.outputs["Background"], outw.inputs["Surface"])

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 48
scene.cycles.use_denoising = True
try:
    prefs = bpy.context.preferences.addons['cycles'].preferences
    prefs.compute_device_type = 'OPTIX'
    prefs.get_devices()
    for d in prefs.devices:
        d.use = True
    scene.cycles.device = 'GPU'
except Exception as e:
    print('GPU not set: %s' % e)
scene.render.resolution_x = 1280
scene.render.resolution_y = 720
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'

cam_data = bpy.data.cameras.new('ReviewCam')
cam_data.lens_unit = 'FOV'
cam_data.angle = 1.5708          # 90 degrees, as in the game
cam = bpy.data.objects.new('ReviewCam', cam_data)
scene.collection.objects.link(cam)
scene.camera = cam

info = []
for name in ('BackDoor_1', 'BackDoor_2', 'FrontDoor_1.001', 'Corridor', 'Cube', 'Cube.002', 'FE', 'Screen1', 'Screen_video', 'Flickering_emmision'):
    o = bpy.data.objects.get(name)
    if o:
        info.append('%s at %s' % (name, [round(v, 2) for v in o.matrix_world.translation]))
print('LAYOUT ' + ' ; '.join(info))


def shot(name, pos, look):
    cam.location = Vector(pos)
    direction = Vector(look) - Vector(pos)
    cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = out_dir + ('game_' if game else 'author_') + name + '.png'
    bpy.ops.render.render(write_still=True)


# The corridor runs along Y from about -22.6 to 17.9; the floor is at Z 0; the player's eye is 1.6 m up.
shot('01_entry', (0.0, -21.5, 1.6), (0.0, 0.0, 1.5))
shot('02_middle', (0.3, -4.0, 1.6), (0.0, 17.0, 1.4))
shot('03_floor_and_wall', (-0.6, 2.0, 1.6), (1.6, 6.0, 0.4))
shot('04_far_door', (0.0, 12.5, 1.6), (0.0, 17.9, 1.5))
print('RENDERED ' + out_dir)
