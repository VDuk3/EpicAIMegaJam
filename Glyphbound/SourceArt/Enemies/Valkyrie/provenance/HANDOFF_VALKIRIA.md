# ASSETS → CENTRAL: Valkiria, entrega única

Fecha UTC: 2026-11-13T19:33:28.8191965Z

## Entrega
Por instrucción expresa de la persona se envía un solo asset: `Valkiria.glb`.
Carpeta: \\192.168.31.188\Incoming\ASSETS\20261113-193325_Valkiria
Origen local: C:\Users\Romelt\Pictures\Valkiria.glb
SHA-256: 49F23405D1260DEC608B832741D5FA80A5E899E46B35488678E91BAB6C5D5C76
Copia verificada con Get-FileHash -Algorithm SHA256.

## Comparación con ValkiriaHolograma.glb
Se inspeccionaron los contenedores GLB con PowerShell/.NET, se calculó SHA-256 del bloque BIN y se comparó el JSON normalizando únicamente los nombres de las imágenes.
- Bloque BIN idéntico en ambos: CF656A8011B6626DC57BC12B92F226C18AB1A7FF5EAD819706658DE9BB768ABA.
- JSON idéntico salvo nombres de imágenes embebidas.
- Por tanto geometría, texturas, pesos, esqueleto y configuración de materiales son iguales. El archivo denominado Holograma no contiene una variante de efecto distinta.
- Valkiria.glb: 23.425.324 bytes. ValkiriaHolograma.glb: 23.425.328 bytes.
- No se copia ValkiriaHolograma.glb; los originales locales se conservan.

## Datos del asset
Una malla, 34.027 vértices del accessor POSITION, 50.517 triángulos, un skin con 79 articulaciones, un material y tres imágenes embebidas (base color, metallic/roughness y normal). Sin clips de animación.
El material no declara alphaMode transparente ni emisión de holograma; la declaración global de KHR_materials_volume no prueba por sí sola un efecto aplicado.

## Objetivo y pendiente
La persona intentó crear una versión holográfica. Mantener un único asset base y preparar en Unreal una variante de material de holograma, si corresponde al diseño del juego, sin duplicar geometría innecesariamente.
Pendiente comprobar apariencia, escala, orientación, rig/deformaciones e importación en Unreal. No se realizó inspección visual ni validación de juego en esta entrega. Derechos y parámetros exactos de generación no verificados. Sin preguntas bloqueantes.

## Trazabilidad
Canvas aportado por la persona: https://studio.tripo3d.ai/es/canvas/share/01a1d5f8-41fa-70b6-89bb-d0f6ede59cc7
El vínculo se registra como procedencia; no se afirma inspección de sus nodos en esta entrega.
## Ampliación de trazabilidad
Canvas revisado directamente el 2026-11-13. Ver BITACORA_CANVAS_VALKIRIA.md: inventario de 20 nodos, referencias, iteraciones frontal/posterior, resultados 3D y límites de la evidencia. Esta ampliación actualiza la nota anterior de vínculo sin inspección.

