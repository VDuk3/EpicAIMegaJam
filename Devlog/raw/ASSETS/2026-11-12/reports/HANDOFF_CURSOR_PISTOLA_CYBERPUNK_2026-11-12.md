# Handoff para Cursor — pistola cyberpunk 3D de la Epic AI MegaJam

Fecha: 2026-11-12. Este archivo es el contexto operativo de **este chat**. Lee también `C:\Epic AI GameJam\AGENTS.md`. No es documentación canónica del juego.

## Instrucción de arranque

Abre `C:\Epic AI GameJam` como workspace de Cursor. Lee este handoff y el registro `Documentation/AI3D_Weapons_Jam/reports/01_generation_manifest.md`; este handoff expresa el estado más reciente y prevalece sobre estados históricos del registro. Inspecciona el archivo de trabajo actual, los renders y las herramientas efectivamente disponibles. Continúa desde ahí, con cuidado, sin reiniciar el diseño ni generar modelos nuevos por rutina.

Trabaja de forma autónoma en tareas autorizadas y reversibles. No te limites a proponer un plan. Preserva originales y versiones aceptadas; crea nuevas copias versionadas. Ante un resultado visual dudoso, compáralo con la versión previa en vistas de cerca, primera persona aproximada y varias posiciones de animación; no entregues como acabado algo que no hayas verificado.

## Objetivo

Producir una pistola de ciencia ficción/cyberpunk **fotorrealista** para primera persona en Unreal, con calidad visual alta, piezas animables y luces controlables. El proyecto de Unreal está en la computadora principal; **no hay .uproject en este workspace de producción**. El agente CENTRAL informó el 12 de noviembre de 2026 que la versión objetivo de la jam es **UE 5.9.0**; este dato procede de CENTRAL y no de una inspección directa del proyecto. UE 5.8 y 5.9 están instalados en esta máquina.

No uses ni modifiques `C:\Panthelia Project\...` por suposición. No edites `Source`, `Content`, `Config`, Blueprints, uassets, documentación canónica ni Git del proyecto sin autorización explícita para esa fase; no hagas commit/push. Staging y copias locales autorizados bajo `C:\Epic AI GameJam\Documentation\AI3D_Weapons_Jam\`. El usuario indicó que **todavía no hay que entregar nada al agente central**: no envíes mensajes ni prepares una entrega externa sin instrucción posterior.

## Estado real y archivos de partida

Usa las versiones más recientes; no uses pruebas históricas por error:

- **Verde:** `C:\Epic AI GameJam\Documentation\AI3D_Weapons_Jam\work\tripo_pistol_B\pistol_optica_integrada_verde_v1.blend` — SHA-256 `A07AD18581C68E35889F7F7AF4A2903CC4F87BAE89DC94F8948D1A7356178268`.
- **Morado:** `C:\Epic AI GameJam\Documentation\AI3D_Weapons_Jam\work\tripo_pistol_B\pistol_optica_integrada_morado_v1.blend` — SHA-256 `142A03A7C646F93A466675EA4BBCDE289420A990DD9E2F0866BF3A042D1F0615`.
- Vista cercana de la mira: `audit/optic_integrada_v1_close.png`. Render de la óptica morada anterior a unir el marco al objeto: `audit/optic_marco_morado_v1_close.png`. La forma visible se comprobó después de unirlo.
- `pistol_restaurada_verde.blend` y `pistol_restaurada_morada.blend` son copias seguras previas a modificar el gatillo. Mantén intactas estas y las versiones más recientes.
- Referencia principal aportada por el usuario: `C:\Epic AI GameJam\Imagen de ChatGPT 12 nov 2026, 11_47_54 a.m.png`; referencias de luces en `concept/references_user/`. Una referencia tiene marca de agua: conserva su atribución y úsala solo como guía visual, no como textura final. El usuario pidió eliminar **todos** los textos, números y marcas heredados. No supongas que ya se hayan eliminado todos.
- Inventario: `reports/00_environment_inventory.md`; manifiesto detallado y decisiones históricas: `reports/01_generation_manifest.md`; pruebas en `audit/`; scripts reproducibles en `scripts/`. Estos informes son históricos, y algunas líneas describen estados ya superados.

La base de trabajo es **Tripo B**. Original preservado en `raw/tripo_pistol_B/tripo-out/imagen-de-chatgpt-12-nov-2026-11-2-50e44d0b/model.glb`; SHA-256 `4B4BA6B4B8A26D94B0E49E16EEB35A050A1D9FB3E3F9E83EBE65CF4A1E76D092`. La malla original tenía 45.690 triángulos. Una versión refinada previa medía 48.390 polígonos / 51.650 triángulos / 18 objetos; **no atribuyas esas cifras automáticamente a la versión actual**: vuelve a medir. La alternativa Meshy tenía 29.927 triángulos, pero conservó marcas y mostró deformaciones. No hay ganador validado en Unreal.

## Dirección aprobada y preferencias del usuario

- Conservar la silueta y el diseño ya desarrollado. El usuario prefiere las zonas **negras**, paneles **blanco/gris plateado**, superficies oscuras pulidas y puntos convertidos en detalles mecánicos limpios. Evitar inscripciones, logotipos, manchas y parches evidentes.
- Luz corta delantera **continua**, no segmentada. Barra larga sobre el gatillo, extendida hacia adelante, repartida en dos tramos que respetan un refuerzo vertical. Evitar que parezca una pieza pegada; debe verse tecnológica e integrada.
- Barra, mira y panel comparten color elemental, con variantes verde y morada. La barra larga expresa tiempo restante del imbuido y disminuye al agotarse. La lógica de gameplay la hará el agente central, **no este agente**. Los materiales deben permitir esa conexión.
- La base del cartucho sobresaliente en la prueba V6 fue aceptada por el usuario. También dijo que la animación del cartucho se ve bien. Conserva su trayectoria y aspecto salvo que una corrección sea necesaria y se compare.
- El usuario aceptó los dientes de sierra del gatillo **solo si no se ven desde el punto de vista del jugador**. La cámara real aún no se ha comprobado. Una modificación anterior quitó/tapó piezas del alojamiento: el usuario pidió retroceder. No repitas borrados o cubiertas que pierdan esos detalles.
- La cámara y renders en `audit/fps_*/` son **aproximaciones**, no cámara del juego real. No des por validado el POV, el disparo, la puntería o la recarga.

## Estado técnico de la copia más reciente

- La óptica tiene una lente transparente, retícula emisiva y marco añadido. El arco del marco se convirtió de curva a malla y se unió al **objeto** `Body_original_mesh`, pero sus vértices **no se soldaron** a la carcasa. Lente y retícula son objetos separados. La retícula debe comprobarse en la cámara real; no asumir que representa un punto de impacto ni que tiene paralaje correcto. La transparencia y renderizado deberán reconstruirse/comprobarse en UE.
- El panel posterior se rehízo con superficies y gráficos superpuestos, sin texto. El grupo de nodos `Weapon_Element_Color` coordina las luces en Blender; ese grafo no se convierte automáticamente en un material dinámico de Unreal. La máscara por color usada en partes de la malla es provisional. La barra incluye un control `ImbueRemaining`; comprobar en la copia actual si el comportamiento y dirección siguen correctos, incluyendo la luz corta continua.
- Gatillo y parte interior comparten pivote y se mueven juntos. La rotación de prueba es en fotogramas 1–16. Se unieron para que al extenderlos no aparezcan picos; se inspeccionó en reposo y presionado. Quedan por comprobar todos los fotogramas y posibles intersecciones desde el POV real.
- El cargador experimental sale y vuelve entre fotogramas 24–72. En V6, cuerpo y base no registraron cruces superficiales BVH contra cuerpo y zona de cavidad en los 72 fotogramas; eso no prueba ausencia de solapamientos de volúmenes ni contra todos los objetos. El usuario aprobó que la base sobresalga. El mecanismo aún es prototipo.
- Se comprobaron matrices de controles del gatillo y cartucho iguales en fotogramas 1, 8, 16, 24, 40, 56 y 72 entre la versión anterior de mira y la integrada. No es una prueba de jugabilidad.
- PBR existente: mapas 2K incorporados `Color` (sRGB), `ORM` y `NormalGL` (Non-Color); B de ORM conecta a Metallic y G a Roughness, normal por Normal Map. Canal AO no comprobado como conectado. Hay superficies generadas y materiales procedurales/superpuestos; quedan por revisar reflejos o luces horneados, tangentes, UVs, metal y rugosidad. No declarar PBR ni asset game-ready sin comprobarlo.
- Numerosas aristas de borde/no manifold son diagnóstico y pueden deberse a costuras de importación; no son fallo automático. No soldar indiscriminadamente: una prueba anterior alteró el sombreado.

## Herramientas, coste y seguridad

- Blender observado en `D:\SteamLibrary\steamapps\common\Blender\blender.exe`; versión observada más reciente 5.3.0. Verifica al ejecutar. Funciona en modo headless. El MCP de Blender estaba configurado pero no conectado; verifica antes de depender de él.
- Tripo CLI 0.11.2 autenticada en perfil API; `tripo balance` devolvió **860 créditos API** en la verificación al crear este handoff. Studio y API tienen saldos separados; el usuario indicó 3.200 en Studio previamente, no verificado directamente. No reveles claves ni tokens.
- Meshy MCP/CLI funcionaban; se había consultado 1.100 créditos antes de gastar 15, sin nueva consulta fiable posterior. El usuario autorizó exactamente esa generación. Verifica saldo si hace falta.
- Hyper3D/Rodin: el usuario dijo **no por ahora**; mantenlo pausado.
- Tripo A costó 30 API, Tripo B 110 API, Meshy A 15. No hay autorización general para generaciones, conversiones, remesh, segmentación o add-ons pagados adicionales. Antes de cualquier gasto nuevo verifica herramienta, saldo y coste específico y pide consentimiento. No reintentes silenciosamente tareas asíncronas: conserva task ID y consulta estado.
- No sobrescribas ni borres originales, no publiques URLs privadas ni coloques contraseñas, tokens o assets en la bandeja CENTRAL. No reinstales ni reconfigures por rutina.

## Siguientes pasos concretos

1. Inspeccionar la copia actual en Blender, no solo un render. Validar coherencia del marco de la mira desde frente/laterales y decidir si conviene soldar/reconstruir unión **en una copia** sin dañar normales ni materiales; preservar aspecto aprobado.
2. Revisar todas las caras y los detalles restantes, especialmente zonas con parches/inscripciones heredadas; corregir sin eliminar mecanismos. No inventar ni esconder una pieza para que parezca solucionada.
3. Hacer inspección reproducible de las animaciones del gatillo y recarga, y confirmar color verde/morado y llenado. Conservar comportamiento y comparar contra versiones aceptadas. Mantener en mente que la base sobresaliente fue aceptada.
4. Preparar materiales PBR y máscaras explícitas coherentes para futura importación, sin suponer que Blender exporta materiales dinámicos o controles de gameplay. Revisar el comportamiento de la lente y retícula. Mantener las variantes y la copia original.
5. Solo cuando el proyecto objetivo y la autorización estén claros, preparar staging para Unreal con escala, ejes, pivotes y exportación; no tocar `Content` hasta recibir permiso. Las pruebas en UE y de rendimiento son pendientes, no realizadas.

Documenta cambios, pruebas, rutas de archivos y riesgos en Markdown. El usuario prefiere avances reales, no informes que sustituyan al trabajo. Si te pregunta algo mientras trabajas, responde brevemente y retoma la tarea. Según CENTRAL, la entrega debe listar todas las herramientas utilizadas, incluidas Tripo, Meshy, Blender y el generador de la referencia; conserva la trazabilidad y verifica las instrucciones oficiales de entrega.

Mantén actualizado `C:\Epic AI GameJam\REGISTRO_ASSETS_3D.md` al crear, modificar o entregar assets en esta computadora: agrega entradas breves con fecha, herramienta, origen, estado, derechos pendientes y enlaces a evidencia detallada; no borres el historial. Mantén también el manifiesto de generación y los renders/auditorías para trazabilidad. No inventes horas, licencias, pruebas ni entregas.

