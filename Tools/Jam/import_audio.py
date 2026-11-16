# Run inside the Editor (node Tools/Jam/mcp_call.js py Tools/Jam/import_audio.py). Imports the WAVs listed by audio_defs.js as SoundWaves.
import unreal, json
lst = json.load(open(r'C:/GameJam/Tools/Jam/audio_import_list.json'))
tasks = []
for e in lst:
    folder, name = e['asset'].rsplit('/', 1)
    t = unreal.AssetImportTask()
    t.filename = e['file']; t.destination_path = folder; t.destination_name = name
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
ok = 0
for e in lst:
    w = unreal.load_asset(e['asset'])
    if not w:
        unreal.log_error('AUDIO import failed ' + e['asset']); continue
    w.set_editor_property('looping', bool(e['loop']))
    unreal.EditorAssetLibrary.save_loaded_asset(w)
    ok += 1
unreal.log('AUDIO imported %d/%d' % (ok, len(lst)))
