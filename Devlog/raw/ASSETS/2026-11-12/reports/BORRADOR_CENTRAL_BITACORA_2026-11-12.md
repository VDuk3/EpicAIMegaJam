# ASSETS → CENTRAL — bitácora de pistola 3D y continuación

Fecha: 2026-11-12. **Mensaje preparado localmente; no entregado al buzón** debido a acceso SMB denegado.

## Resumen

En la computadora ASSETS se está produciendo una pistola cyberpunk para primera persona de la Epic AI MegaJam. El registro general y el manifiesto detallado de producción se actualizaron. El usuario pidió que CENTRAL pueda acceder a ellos.

- Registro general en ASSETS: `C:\Epic AI GameJam\REGISTRO_ASSETS_3D.md`.
- Manifiesto de tareas, hashes, pruebas y decisiones en ASSETS: `C:\Epic AI GameJam\Documentation\AI3D_Weapons_Jam\reports\01_generation_manifest.md`.
- Handoff operativo para Cursor en ASSETS: `C:\Epic AI GameJam\Documentation\AI3D_Weapons_Jam\HANDOFF_CURSOR_PISTOLA_CYBERPUNK_2026-11-12.md`.
- Esas rutas son locales a ASSETS; **no se ha verificado que CENTRAL pueda abrirlas directamente**. El usuario pidió poner la información en el buzón. No copiar assets al buzón.

## Trazabilidad de la producción

- Tripo H3.1, tarea `bd2a20a1-3a02-42d9-ab72-178a2eaaf53d`, 30 créditos API; prototipo denso.
- Tripo P2, tarea `50e44d0b-5c36-48f2-a7a9-87d05b466f45`, 110 créditos API; base actual, 45.690 triángulos en el GLB original.
- Meshy, tarea `01a1d080-cccd-775f-adb9-8e1d2625e62b`, 15 créditos; 29.927 triángulos. Retuvo textos de referencia y presentó deformaciones.
- Se trabajó en Blender 5.2.3 LTS y 5.3.0; las copias actuales tienen acabado negro y blanco plateado y variantes de luces verde/morado.
- El asset continúa en desarrollo; no se ha entregado, importado ni validado en Unreal. El proyecto real está en la computadora principal y su cámara/versión no han sido verificadas desde ASSETS.

## Estado del prototipo

El gatillo y la pieza interior comparten pivote; hay ensayo de presión y una prueba de extracción del cartucho. El usuario aprobó la trayectoria visual y la base del cartucho sobresaliente. Aún deben validarse intersecciones, cámara real y recarga en juego.

La barra, la mira y el panel comparten un control de color en Blender; el indicador largo debe disminuir conforme se agota el imbuido elemental. La lógica de juego corresponde al agente central cuando el usuario decida coordinarla. El usuario indicó que todavía no se le entregue el asset al agente central. El marco de la mira está unido al objeto cuerpo, no soldado topológicamente; lente y retícula siguen separadas.

Existen mapas PBR 2K de color, ORM y normal, pero los materiales dinámicos y la transparencia requieren reconstrucción y pruebas en Unreal. El usuario pidió eliminar textos y marcas heredados; la limpieza completa y los derechos de referencias aún deben verificarse. No se debe declarar el asset terminado.

## Qué necesita CENTRAL

Conservar esta nota como referencia de bitácora. Consultar al usuario o establecer una ruta compartida autorizada para acceder a los archivos locales de ASSETS; no asumir que `C:\Epic AI GameJam` de CENTRAL sea la misma máquina ni el proyecto UE. No importar ni modificar `Content`, Blueprints o documentación canónica hasta que el usuario autorice esa fase.

## Preguntas / trabajo pendiente

1. Confirmar ruta compartida de acceso al registro y al handoff completo, si CENTRAL necesita leerlos directamente.
2. Pendiente en ASSETS: limpiar texturas y geometría, comprobar animación y materiales; pendiente en proyecto real: escala, pivotes, cámara, PBR, rendimiento y pruebas de UE.
3. Rodin está pausado por decisión del usuario. No ejecutar nuevas tareas pagadas sin verificar saldo, coste y autorización.


## Añadido al entregar

- **Herramientas y versiones:** Tripo CLI 0.11.2; Blender 5.2.3 LTS y 5.3.0; Meshy se usó para generación imagen a 3D.
- **Modelos de IA:** Tripo H3.1 (`v3.1-20260211`) y Tripo P2 (`P2-20260801`); Meshy `meshy-t2.5` para geometría y `meshy-7` para texturas PBR 2K.
- **Referencias y licencias de terceros:** se usaron referencias visuales aportadas por el usuario; incluyen marcas y una referencia secundaria con marca de agua. Sus derechos/licencias y la limpieza completa de marcas siguen sin verificarse. No consta una licencia de terceros confirmada para este prototipo.