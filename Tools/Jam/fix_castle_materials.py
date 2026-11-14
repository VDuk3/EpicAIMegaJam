# fix_castle_materials.py - replaces, in the castle pack's mesh assets themselves, a material that does not compile in
# this project. Overriding it on the placed pieces is not enough: the mesh asset still names it, so the cook still
# takes it and the packaged game logs "Loading a material resource None with an invalid ShaderMap".
#   MI_Stab_Algae (two trim slots of SM_Middle_Arch and SM_Front_Top, the gatehouse): "Failed to compile Material
#   Instance with Base M_BaseMaterial1 ... Error on property Normal". Replaced by the pack's plain slab stone.
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/fix_castle_materials.py   (on the empty start-up map)
import unreal

BAD = {'MI_Stab_Algae': '/Game/CastleTown/Materials/Instances/MI_Slabs_03'}
reg = unreal.AssetRegistryHelpers.get_asset_registry()
out = []
for ad in reg.get_assets_by_path('/Game/CastleTown/Static_Mesh', recursive=True):
    if str(ad.asset_class_path.asset_name) != 'StaticMesh':
        continue
    deps = [str(d) for d in reg.get_dependencies(ad.package_name, unreal.AssetRegistryDependencyOptions(include_soft_package_references=False)) or []]
    if not any(d.split('/')[-1] in BAD for d in deps):
        continue
    sm = ad.get_asset()
    n = 0
    for i, slot in enumerate(sm.static_materials):
        mi = slot.material_interface
        if mi and mi.get_name() in BAD:
            sm.set_material(i, unreal.load_asset(BAD[mi.get_name()]))
            n += 1
    if n:
        unreal.EditorAssetLibrary.save_asset(str(ad.package_name), only_if_is_dirty=False)
        out.append('%s: %d slots' % (sm.get_name(), n))
'replaced in %d meshes: %s' % (len(out), ', '.join(out))
