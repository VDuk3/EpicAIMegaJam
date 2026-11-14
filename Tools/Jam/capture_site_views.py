# capture_site_views.py - renders review views of the site map from the Editor (one era at a time).
# Run through the MCP Python tool (it uses `await call_tool`), with SHOTS and OUT_DIR set before exec:
#   SHOTS = [('past', 'name', x, y, z, pitch, yaw), ...]; OUT_DIR = 'C:/.../'
import base64

_w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
_lv = {}
for _p in ('Site_Past', 'Site_Future'):
    _s = unreal.GameplayStatics.get_streaming_level(_w, unreal.Name('/Game/Glyphbound/Maps/' + _p))
    _lv[_p] = _s.get_loaded_level()


async def capture_views(shots, out_dir, width=1280, height=720, mode='Lit'):
    written = []
    try:
        for era, name, x, y, z, pitch, yaw in shots:
            show = 'Site_Past' if era == 'past' else 'Site_Future'
            hide = 'Site_Future' if era == 'past' else 'Site_Past'
            unreal.EditorLevelUtils.set_level_visibility(_lv[hide], False, False)
            unreal.EditorLevelUtils.set_level_visibility(_lv[show], True, False)
            await asyncio.sleep(0)
            cam = {'location': {'x': x, 'y': y, 'z': z}, 'rotation': {'pitch': pitch, 'yaw': yaw, 'roll': 0},
                   'projectionMode': 'Perspective', 'fOV': 90}
            r = await call_tool('SceneCaptureToolset.SceneCaptureToolset.Capture', world={'refPath': _w.get_path_name()},
                                camera=cam, mode=mode, options={'width': width, 'height': height, 'screenPercentage': 1.0, 'visualizationSubMode': ''})
            path = out_dir + name + '.png'
            with open(path, 'wb') as f:
                f.write(base64.b64decode(r['image']['data']))
            written.append(name)
    finally:
        for p in _lv:
            unreal.EditorLevelUtils.set_level_visibility(_lv[p], True, False)
    return written
