# 2026-11-15: the package under 1 GB (texture caps)

The owner (Spanish): *"analiza cómo bajar de esos 2gb a menos de 1gb"* (work out how to get from those 2 GB to under 1 GB), and after the analysis: *"Sí, pruébalo"* (yes, try it).

## What the zip was made of (2,000 MB)
- 1,859 MB: the game's content (`Glyphbound-Windows.ucas`), already compressed by the engine, so the zip saves almost nothing on it. 110 MB: the Development `.exe`.
- The cooked content was 5.4 GB before compression, and **4.86 GB of it were textures**: DeepWaterStation 1,411 MB, High_Tech_Base 1,387, SciFiWorld 818, ModularSciFiStation 589, MaterialsScifi 277, our own 165. Of the 745 cooked textures, 496 are 2048 px, 127 are 4096 px, and DeepWaterStation has long trim atlases up to 32768 x 8192 (265 MB each before compression).
- Estimate (from file sizes, the conservative way): cap 2048 gives about 1.35 GB (not enough), cap 1024 about 0.7 GB.

## Decision and how
- **World textures capped at 1024 px in the cooked game**, by the Windows device profile (`Glyphbound/Config/DefaultDeviceProfiles.ini`). Checked in the engine source first: the cooker writes a texture from the first mip the cook target's LOD settings allow (`TextureDerivedData.cpp`, `FirstMipToSerialize`), and groups a profile does not list come from `GlobalDefaults` (`DeviceProfile.cpp`, `ValidateTextureLODGroups`). The Editor uses the `WindowsEditor` profile and keeps every texture whole. The cap is config only: one file in git; no source art was changed.
- Two exceptions, so the cap does not hurt what it should not:
  - **The player's weapons are seen up close:** 23 textures under `/Game/Glyphbound/Weapons` (and one muzzle-flash pair) moved from the World groups to the Weapon groups, capped at **2048**.
  - **The long trim atlases:** a 1024 cap on a 32768 x 8192 atlas would leave 1024 x 256. The 50 textures whose long side is over 8192 and at least twice the short side go to `TEXTUREGROUP_Project01`, capped at **8192**: 32768 x 8192 becomes 8192 x 2048, the same 4x cut a 4K square texture gets.
  - Both regroupings are done by `Tools/Jam/Set-TextureGroups.py`, which runs again with nothing to do (checked: "0 textures regrouped"). It reads sizes from the asset registry, so it does not load the thousands of other textures.
- Implementer: the architect directly (a config file and a 50-line Editor script, inside an analysis it had just made).

## Result
- **Zip 925.7 MB** (was 2,000.3 MB); `Glyphbound-Windows.ucas` 797 MB (was 1,965 MB). Package 124 s.
- Packaged tour of the future base: 85 spots, 0 without floor, 0 engine errors. Packaged tour of the castle: 42 spots, 0 without floor, the seal's map can be opened, 0 engine errors. Full smoke: 0 LogGB errors, 0 engine errors (391 s).
- Pictures (packaged game): `media/2026-11-15/texcap_futurebase_tour_sheet.jpg` (all 85 spots), `media/2026-11-15/texcap_futurebase_24b_closeup.png` (a wall and a screen at arm's length).
- Margin: about 98 MB under the limit. New 4K textures in the World groups cost about 1.3 MB each now, not 21.
