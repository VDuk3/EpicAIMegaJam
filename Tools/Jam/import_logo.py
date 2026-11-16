# import_logo.py - imports the Arcane Override logo (SourceArt/UI/Arcane_Override_logo.png, made with ChatGPT by the
# owner's team, see Arcane_Override_logo_PROVENANCE.md) as /Game/Glyphbound/UI/T_ArcaneOverride_Logo: UI group,
# no mips, alpha kept, no sRGB change. Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/import_logo.py
import os
import unreal

SRC = os.path.join(unreal.Paths.project_dir(), 'SourceArt', 'UI', 'Arcane_Override_logo.png')
DEST_DIR = '/Game/Glyphbound/UI'
NAME = 'T_ArcaneOverride_Logo'

task = unreal.AssetImportTask()
task.set_editor_property('filename', os.path.abspath(SRC))
task.set_editor_property('destination_path', DEST_DIR)
task.set_editor_property('destination_name', NAME)
task.set_editor_property('replace_existing', True)
task.set_editor_property('automated', True)
task.set_editor_property('save', False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

path = DEST_DIR + '/' + NAME
tex = unreal.EditorAssetLibrary.load_asset(path)
tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
tex.set_editor_property('never_stream', True)
tex.set_editor_property('srgb', True)
unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
print('imported %s %dx%d' % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
