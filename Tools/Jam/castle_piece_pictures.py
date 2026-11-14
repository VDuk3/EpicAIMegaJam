# castle_piece_pictures.py - one picture of each static mesh of a list, so every piece is looked at before it is
# used (a piece is never trusted by its name). Each mesh stands alone on a floor in a review level, lit by a sun and
# a sky light, and is rendered offscreen from a three-quarter view at a distance that fits its bounds. A 1 m and a
# 1.8 m post stand beside it for scale.
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/castle_piece_pictures.py
# Settings: Glyphbound/Saved/castle_pictures.json
#   {"out": "C:/dir/", "pieces": "C:/GameJam/Design/Levels/kits/castle/pieces.json", "match": "Castle_Kit" (or a list of texts),
#    "size": 512, "yaw": 215, "pitch": -22}
import base64
import json
import math
import os

import unreal

CFG = json.load(open(os.path.join(unreal.Paths.project_saved_dir(), 'castle_pictures.json')))
LEVEL = '/Game/Glyphbound/Maps/Dev/Review_Castle'
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
os.makedirs(CFG['out'], exist_ok=True)

if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    LES.load_level(LEVEL)
else:
    LES.new_level(LEVEL)
for a in EAS.get_all_level_actors():
    if a.get_actor_label().startswith('RV_'):
        EAS.destroy_actor(a)


def box(label, x, y, z, sx, sy, sz):
    a = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z))
    a.set_actor_label('RV_' + label)
    a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.set_actor_scale3d(unreal.Vector(sx, sy, sz))
    return a


box('Floor', 0, 0, -5, 400, 400, 0.1)
sun = EAS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(roll=0, pitch=-45, yaw=-60))
sun.set_actor_label('RV_Sun')
sky = EAS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000))
sky.set_actor_label('RV_Sky')
sky.light_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property('intensity', 1.5)
pp = EAS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
pp.set_actor_label('RV_PP')
pp.set_editor_property('unbound', True)
s = pp.get_editor_property('settings')
s.set_editor_property('override_auto_exposure_method', True)
s.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
s.set_editor_property('override_auto_exposure_bias', True)
s.set_editor_property('auto_exposure_bias', float(CFG.get('exposure', 11.0)))
pp.set_editor_property('settings', s)

MATCH = CFG.get('match', '')
MATCH = MATCH if isinstance(MATCH, list) else [MATCH]
pieces = [p for p in json.load(open(CFG['pieces'])) if any(m in p['mesh'] for m in MATCH)]
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
yaw, pitch, fov = float(CFG.get('yaw', 215)), float(CFG.get('pitch', -22)), 40.0
done = []
for p in pieces:
    mesh = unreal.load_asset(p['mesh'])
    if not mesh:
        continue
    a = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -p['min'][2]))
    a.set_actor_label('RV_Piece')
    a.static_mesh_component.set_static_mesh(mesh)
    cx, cy = (p['min'][0] + p['max'][0]) / 2, (p['min'][1] + p['max'][1]) / 2
    h = p['size'][2]
    r = 0.5 * math.sqrt(p['size'][0] ** 2 + p['size'][1] ** 2 + h ** 2)
    # scale posts: 1 m and 1.8 m, at the piece's corner nearest the camera
    px, py = p['max'][0] + 30, p['min'][1] - 30
    posts = [box('Post1', px, py, 50, 0.1, 0.1, 1.0), box('Post2', px + 30, py, 90, 0.1, 0.1, 1.8)]
    dist = max(r, 120) / math.tan(math.radians(fov / 2)) * 1.15
    yr, pr = math.radians(yaw), math.radians(pitch)
    fwd = (math.cos(pr) * math.cos(yr), math.cos(pr) * math.sin(yr), math.sin(pr))
    cam = {'location': {'x': cx - fwd[0] * dist, 'y': cy - fwd[1] * dist, 'z': h / 2 - fwd[2] * dist},
           'rotation': {'pitch': pitch, 'yaw': yaw, 'roll': 0}, 'projectionMode': 'Perspective', 'fOV': fov}
    res = await call_tool('SceneCaptureToolset.SceneCaptureToolset.Capture', world={'refPath': world.get_path_name()},
                          camera=cam, mode=CFG.get('mode', 'Lit'),
                          options={'width': CFG.get('size', 512), 'height': CFG.get('size', 512), 'screenPercentage': 1.0,
                                   'visualizationSubMode': ''})
    img = res['image']
    data = img['data'] if isinstance(img, dict) else img
    name = p['mesh'].split('/')[-1]
    with open(os.path.join(CFG['out'], '%s_%dx%dx%d.png' % (name, p['size'][0], p['size'][1], p['size'][2])), 'wb') as f:
        f.write(base64.b64decode(data))
    done.append(name)
    for x in [a] + posts:
        EAS.destroy_actor(x)
    await asyncio.sleep(0)
'%d pictures -> %s' % (len(done), CFG['out'])
