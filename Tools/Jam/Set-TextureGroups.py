# Set-TextureGroups.py - puts textures in the LOD groups whose size caps Config/DefaultDeviceProfiles.ini sets for the
# game (owner, 2026-11-15: the zip under 1 GB). Run in the Editor: node Tools/Jam/mcp_call.js py Tools/Jam/Set-TextureGroups.py
# - every texture under /Game/Glyphbound/Weapons -> Weapon / WeaponNormalMap / WeaponSpecular (cap 2048: seen up close);
# - the long trim atlases (long side above 8192 and at least twice the short side) -> Project01 (cap 8192), so a
#   32768 x 8192 atlas is cut to 8192 x 2048 and not to 1024 x 256 by the World cap.
# Saves only the textures it changes and returns what it did.
import unreal

eal = unreal.EditorAssetLibrary
TG = unreal.TextureGroup
WEAPON = {TG.TEXTUREGROUP_WORLD: TG.TEXTUREGROUP_WEAPON, TG.TEXTUREGROUP_WORLD_NORMAL_MAP: TG.TEXTUREGROUP_WEAPON_NORMAL_MAP,
          TG.TEXTUREGROUP_WORLD_SPECULAR: TG.TEXTUREGROUP_WEAPON_SPECULAR}
done = []


def textures(folder):
    for p in eal.list_assets(folder, recursive=True, include_folder=False):
        a = eal.load_asset(p.split('.')[0])
        if isinstance(a, unreal.Texture2D):
            yield p.split('.')[0], a


def set_group(path, tex, group):
    if tex.get_editor_property('lod_group') != group:
        tex.set_editor_property('lod_group', group)
        eal.save_asset(path, only_if_is_dirty=False)
        done.append('%s -> %s' % (path, group))


for path, tex in textures('/Game/Glyphbound/Weapons'):
    g = tex.get_editor_property('lod_group')
    if g in WEAPON:
        set_group(path, tex, WEAPON[g])

# (the sizes come from the asset registry's tags, so the thousands of other textures are not loaded)
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for ad in ar.get_assets_by_path('/Game', recursive=True):
    if str(ad.asset_class_path.asset_name) != 'Texture2D':
        continue
    dim = str(ad.get_tag_value('Dimensions') or '')
    if 'x' not in dim:
        continue
    w, h = (int(float(v)) for v in dim.split('x')[:2])
    if max(w, h) > 8192 and max(w, h) >= 2 * min(w, h):
        path = str(ad.package_name)
        set_group(path, eal.load_asset(path), TG.TEXTUREGROUP_PROJECT01)

'%d textures regrouped\n%s' % (len(done), '\n'.join(done))
