# capture_views.py - renders review views of the level open in the Editor, offscreen (no PIE, no viewport change).
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/capture_views.py
# It reads its shot list from the JSON file named in SHOTS_FILE below (rewritten by the caller before each run):
#   {"out": "C:/dir/", "width": 1280, "height": 720, "mode": "Lit",
#    "shots": [["name", x, y, z, pitch, yaw, fov], ...]}
# fov is optional (default 90). A shot with "ortho": true in a dict form is not supported; use a high pitch -90 view.
import base64
import json

SHOTS_FILE = r'C:/GameJam/Glyphbound/Saved/capture_shots.json'
cfg = json.load(open(SHOTS_FILE))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
written = []
for shot in cfg['shots']:
    name, x, y, z, pitch, yaw = shot[:6]
    fov = shot[6] if len(shot) > 6 else 90
    cam = {'location': {'x': x, 'y': y, 'z': z}, 'rotation': {'pitch': pitch, 'yaw': yaw, 'roll': 0},
           'projectionMode': 'Perspective', 'fOV': fov}
    if len(shot) > 7:
        # 8th value: orthographic view width in cm (a plan or an elevation; the camera position is the cut plane).
        cam = {'location': {'x': x, 'y': y, 'z': z}, 'rotation': {'pitch': pitch, 'yaw': yaw, 'roll': 0},
               'projectionMode': 'Orthographic', 'orthoWidth': shot[7], 'orthographicClipPlanes': {'x': 1.0, 'y': 20000.0}}
    if cfg.get('method') == 'viewport':
        # The real level viewport: Lumen with its temporal history and the viewport's exposure (what a player sees).
        # The offscreen Capture has no temporal history, so a Lumen-lit interior comes out nearly black.
        tr = {'translation': {'x': x, 'y': y, 'z': z}, 'rotation': {'pitch': pitch, 'yaw': yaw, 'roll': 0},
              'scale3D': {'x': 1, 'y': 1, 'z': 1}}
        for _ in range(int(cfg.get('warm', 2))):
            await call_tool('SceneCaptureToolset.SceneCaptureToolset.CaptureEditorViewport', captureTransform=tr)
            await asyncio.sleep(0.3)
        r = await call_tool('SceneCaptureToolset.SceneCaptureToolset.CaptureEditorViewport', captureTransform=tr)
    else:
        r = await call_tool('SceneCaptureToolset.SceneCaptureToolset.Capture', world={'refPath': world.get_path_name()},
                            camera=cam, mode=cfg.get('mode', 'Lit'),
                            options={'width': cfg.get('width', 1280), 'height': cfg.get('height', 720), 'screenPercentage': 1.0, 'visualizationSubMode': ''})
    img = r['image']
    data = img['data'] if isinstance(img, dict) else img
    with open(cfg['out'] + name + '.png', 'wb') as f:
        f.write(base64.b64decode(data))
    written.append(name)
    await asyncio.sleep(0)
'wrote %d: %s' % (len(written), ', '.join(written))
