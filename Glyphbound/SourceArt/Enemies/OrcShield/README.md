# Orc shield (the horde chief's)

Delivered by the ASSETS machine on 2026-11-14 (Tripo; canvas and hashes in `provenance/BITACORA_EQUIPO_ORCO.md`), in `escudo orco.zip` (SHA-256 11164A5E433F705C3ACCEC0DFF650CB144E56A6A4C062EE81BA64CE79746BB09). Moved here from `Incoming/ASSETS/20261114-133644_Escudo_y_Pistola_Orcos` the same day; every file checked against the delivery's SHA-256 list.

| File here | Name in the zip | SHA-256 |
|---|---|---|
| `OrcShield.fbx` | `tripo_convert_5ead9fc7-f1b4-44ee-9fee-193b16c9e605.fbx` | 50A45475CEE5380DBDFD9C49BD094C281E6BC7F2A7984306AEF9DC35E9334126 |
| `OrcShield_BaseColor.jpg` | `...fbm/tripo_rgb_72081728-aee0-47c5-af23-fdc405c406b1.jpg` | 8C70344C7EB731700D29A596E1130F6CCB524CFC1D82E66AE23D4951D2218CE9 |
| `OrcShield_Normal.jpg` | `...fbm/tripo_normal_d88bbd2e-0fcc-4968-9d5e-a06fc29c439b.jpg` | 76469F09C91E18EDD1EE1450051C150DA2750414C7240A00B687F5B576DA08E3 |
| `OrcShield_Metallic.jpg` | `...fbm/escudo_orco_goku_metallic.jpg` | ED375B7A25972D2E041A4710058D91F3A130E2AD589B152714B28284AA376382 |
| `OrcShield_Roughness.jpg` | `...fbm/escudo_orco_goku_roughness.jpg` | 358C1F987DC52AAC2483647745041970F51B956357B9BE58AABC70EEAB3CF157 |

The files were renamed on arrival (the generator's file names carry a working title; the model itself is an orc shield with a skull and has no third party's mark on it: checked on renders).

As delivered: one mesh of 6,279 polygons, 1 m tall, its face to -Y, a handle on its back 46 % up. `Tools/Blender/orc_gear_export.py` turns the face to +X, puts the origin on the handle and sizes it 1.15 m tall (93 cm wide, 50 cm deep with its spikes); `Tools/Jam/import_orc_gear.py` imports it as `/Game/Glyphbound/Characters/Orc/Gear/SM_OrcShield` with an instance of the enemies' master material. Intermediates live in `Glyphbound/Saved/orc_gear_stage`.
