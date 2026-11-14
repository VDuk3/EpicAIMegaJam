# Orc pistol (the horde chief's heavy hand cannon)

Delivered by the ASSETS machine on 2026-11-14 (Tripo; canvas and hashes in `provenance/BITACORA_EQUIPO_ORCO.md`), in `tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip` (SHA-256 422BF2D50A478614DFACB698B5EE52726B6EFAE58AAE191B647E3F5EE250C021). Moved here from `Incoming/ASSETS/20261114-133644_Escudo_y_Pistola_Orcos` the same day; every file checked against the delivery's SHA-256 list.

| File here | Name in the zip | SHA-256 |
|---|---|---|
| `OrcPistol.fbx` | `tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.fbx` | 47F9DA3B6E93C0BAA02540AF29F0A5FDFABE43D548E3C2A13C21A96B88DF2DEA |
| `OrcPistol_BaseColor.jpg` | `...fbm/tripo_rgb_3f13d34b-5381-4631-ac7f-552bfada6092.jpg` | 78EA3599AF2C6B3EDB57DBA066615453DF0E87B9B697459A310D3E090596BE43 |
| `OrcPistol_Normal.jpg` | `...fbm/tripo_normal_a2f54c1c-93d6-4fff-a0f8-9ff0c561a31d.jpg` | E72FDE7C7BB8425857863DD5F8163126D47095FFB2CD77188A5D48EE2CF9EDD9 |
| `OrcPistol_Metallic.jpg` | `...fbm/una_pistola_orca_puramente_visual_para_..._#5_metallic.jpg` | 3F15A56E4D9635B7074650BEA9BCD338F6FE02ABE72991B7EBA1D57815C9C6E0 |
| `OrcPistol_Roughness.jpg` | `...fbm/una_pistola_orca_puramente_visual_para_..._#5_roughness.jpg` | 92AC9858F6BC13A61184218DE00374BA2E185D03B6881F2B0604050308DE32AD |

As delivered: one mesh of 6,618 polygons, 1 m long, muzzle to -X. `Tools/Blender/orc_gear_export.py` turns the barrel to +X, puts the origin on the grip and sizes it 52 cm long, with a `Muzzle` socket at the bore; `Tools/Jam/import_orc_gear.py` imports it as `/Game/Glyphbound/Weapons/OrcPistol/SM_OrcPistol`. No moving parts.
