# Escudo y pistola orcos: bitácora y entrega ASSETS → CENTRAL

Registro e inspección: 2026-11-14 (hora de Caracas). No acredita fecha original de generación.

## Archivos e identificación
- `escudo orco.zip`: un FBX `tripo_convert_5ead9fc7-f1b4-44ee-9fee-193b16c9e605.fbx` y carpeta `.fbm` con cuatro JPG: color, normal, metallic y roughness. Los nombres de dos mapas contienen `escudo_orco_goku`; se conserva como nombre original, no como atribución o autorización de franquicia.
- `tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip`: un FBX homónimo y carpeta `.fbm` con cuatro JPG de color, normal, metallic y roughness. Los nombres de textura identifican una pistola orca; el canvas confirma que existe esa rama visual. No se verificó ID interno que vincule exactamente exportación y nodo.
- Son dos paquetes distintos, no dos variantes demostradas del mismo asset.

## Canvas inspeccionado directamente
https://studio.tripo3d.ai/es/canvas/share/01a1d690-6fa1-72e3-beaa-9b0609938661
Título visible truncado: `una pistola orca puramente visual para …`.
Cuenta mostrada: `camilojuan602`. El nombre de cuenta no prueba identidad ni autoría de cada elemento.

Se observan 17 nodos:
- Cuatro de imagen: `Generar imagen`, `Generar imagen (1)`, `Generar imagen (2)`, `Generar imagen (3)`.
- Dos resultados de cada tipo: Malla de topología inteligente, Smart UV, Generación de texturas y PBR (nombres base y sufijo `(1)`).
- Un nodo `Multivista` y cuatro vistas: `Left`, `Back`, `Right`, `Front`.

La zona superior muestra tres iteraciones de un escudo ornamental con rostro/figura monstruosa, salientes y color metálico con tonos turquesa; a su derecha, los resultados de malla, UV, texturas y PBR con sufijo `(1)`.
La zona media muestra una pistola orca de superficie oscura, forma irregular y salientes; sus resultados de malla, UV, texturas y PBR tienen nombres base sin sufijo. Debajo aparecen Multivista y las cuatro vistas del arma.
No se observan nodos de rigging o animación. La vista compartida no expuso aristas en el árbol de accesibilidad: no se certifica cada dependencia. Prompts literales, modelos/versiones, costes, parámetros y fechas originales no estaban disponibles. No se realizaron generaciones ni modificaciones.

## Evidencia de archivos y límites
Ambos ZIP se abrieron con System.IO.Compression.ZipFile y se leyeron completamente sus cinco entradas para calcular SHA-256. Los paquetes se copiaron sin modificar y los hashes de destino coinciden con los originales. No se extrajeron ni ejecutaron archivos del paquete.
No se importó el FBX ni se contaron polígonos, huesos o clips: no se afirma que carezcan de rig/animación sin inspección de la escena. No hay validación visual de las mallas o integración en Unreal en esta entrega.

## Trabajo pendiente para CENTRAL
Importar cada FBX junto con su carpeta `.fbm`, conservar rutas/texturas y verificar conexiones de color, normal, metallic y roughness. Revisar escala, orientación, pivotes, materiales, colisión y geometría. Preparar acoplamiento modular del escudo y la pistola a los orcos mediante agarres/sockets apropiados. Revisar partes móviles si se requiere animación de disparo/recarga. No asumir gameplay funcional por el título o las imágenes.
Derechos, autoría y condiciones de uso no documentados para estas referencias. Sin preguntas bloqueantes para recibir e inspeccionar los paquetes.

## Paquetes entregados
Carpeta CENTRAL: \\192.168.31.188\Incoming\ASSETS\20261114-133644_Escudo_y_Pistola_Orcos

| ZIP | Bytes | SHA-256 |
|---|---:|---|
| escudo orco.zip | 12382856 | 11164A5E433F705C3ACCEC0DFF650CB144E56A6A4C062EE81BA64CE79746BB09 |
| tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip | 11253411 | 422BF2D50A478614DFACB698B5EE52726B6EFAE58AAE191B647E3F5EE250C021 |

## Inventario interno leído
| ZIP | Entrada | Bytes | SHA-256 |
|---|---|---:|---|
| escudo orco.zip | tripo_convert_5ead9fc7-f1b4-44ee-9fee-193b16c9e605.fbx | 327116 | 50A45475CEE5380DBDFD9C49BD094C281E6BC7F2A7984306AEF9DC35E9334126 |
| escudo orco.zip | tripo_convert_5ead9fc7-f1b4-44ee-9fee-193b16c9e605.fbm/tripo_normal_d88bbd2e-0fcc-4968-9d5e-a06fc29c439b.jpg | 2370362 | 76469F09C91E18EDD1EE1450051C150DA2750414C7240A00B687F5B576DA08E3 |
| escudo orco.zip | tripo_convert_5ead9fc7-f1b4-44ee-9fee-193b16c9e605.fbm/escudo_orco_goku_metallic.jpg | 2644200 | ED375B7A25972D2E041A4710058D91F3A130E2AD589B152714B28284AA376382 |
| escudo orco.zip | tripo_convert_5ead9fc7-f1b4-44ee-9fee-193b16c9e605.fbm/tripo_rgb_72081728-aee0-47c5-af23-fdc405c406b1.jpg | 4327341 | 8C70344C7EB731700D29A596E1130F6CCB524CFC1D82E66AE23D4951D2218CE9 |
| escudo orco.zip | tripo_convert_5ead9fc7-f1b4-44ee-9fee-193b16c9e605.fbm/escudo_orco_goku_roughness.jpg | 2798641 | 358C1F987DC52AAC2483647745041970F51B956357B9BE58AABC70EEAB3CF157 |
| tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip | tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.fbx | 338108 | 47F9DA3B6E93C0BAA02540AF29F0A5FDFABE43D548E3C2A13C21A96B88DF2DEA |
| tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip | tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.fbm/una_pistola_orca_puramente_visual_para_…_#5_roughness.jpg | 2119368 | 92AC9858F6BC13A61184218DE00374BA2E185D03B6881F2B0604050308DE32AD |
| tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip | tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.fbm/tripo_rgb_3f13d34b-5381-4631-ac7f-552bfada6092.jpg | 4320213 | 78EA3599AF2C6B3EDB57DBA066615453DF0E87B9B697459A310D3E090596BE43 |
| tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip | tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.fbm/tripo_normal_a2f54c1c-93d6-4fff-a0f8-9ff0c561a31d.jpg | 2482871 | E72FDE7C7BB8425857863DD5F8163126D47095FFB2CD77188A5D48EE2CF9EDD9 |
| tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.zip | tripo_convert_4b6f7f40-3fc3-4d1f-8362-1ec7caa3f103.fbm/una_pistola_orca_puramente_visual_para_…_#5_metallic.jpg | 2064473 | 3F15A56E4D9635B7074650BEA9BCD338F6FE02ABE72991B7EBA1D57815C9C6E0 |
