# cap_kit_textures.py - caps the third-party kits' textures at 2048 px so the package stays under the jam's 1 GB.
# Run once in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/cap_kit_textures.py
# The kits ship 4K textures; with the future base in the cook the zip was 1232.8 MB (2026-11-13). A 2K texture on a
# 4 m panel is still 5 texels per cm. Idempotent: textures already capped are skipped.
# Cooked sizes before the cap: High Tech Base textures 1,655 MB, Modular SciFi Station 559 MB, Scifi Material Pack 278 MB.
CAP = 2048
ROOTS = ('/Game/High_Tech_Base', '/Game/ModularSciFiStation', '/Game/SciFiWorld', '/Game/MaterialsScifi')
ar = unreal.AssetRegistryHelpers.get_asset_registry()
eal = unreal.EditorAssetLibrary
changed = 0
seen = 0
for root in ROOTS:
    for a in ar.get_assets_by_path(root, recursive=True):
        if str(a.asset_class_path.asset_name) != 'Texture2D':
            continue
        seen += 1
        t = a.get_asset()
        # No size test here: the Editor reported 2048 for textures that cook at 4096 (the first run capped nothing
        # and the zip did not shrink). The cap is set on every texture that does not have one.
        cur = t.get_editor_property('max_texture_size')
        if cur and cur <= CAP:
            continue
        t.set_editor_property('max_texture_size', CAP)
        eal.save_asset(str(a.package_name), only_if_is_dirty=False)
        changed += 1
        if changed % 10 == 0:
            await asyncio.sleep(0)
'%d textures seen, %d capped at %d' % (seen, changed, CAP)
