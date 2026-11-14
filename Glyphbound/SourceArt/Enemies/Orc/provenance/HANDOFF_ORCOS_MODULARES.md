# Entrega ASSETS → CENTRAL: orcos modulares

Fecha UTC: 2026-11-13T17:30:45.8081161Z

## Objetivo autorizado por la persona
Crear varios tipos de orcos en el editor mediante un cuerpo base común y piezas de armadura intercambiables. Conservar este objetivo de modularidad; no consolidar todo en un único modelo fijo como solución final.

## Archivos entregados
Carpeta: \\192.168.31.188\Incoming\ASSETS\20261113-173042_Orcos_Modulares

| Archivo | Bytes | Mallas | Skins | Articulaciones | Animaciones | SHA-256 |
|---|---:|---:|---:|---:|---:|---|
| Base Rigged Orc.glb | 4064708 | 1 | 1 | 80 | 0 | 7E30CC12BBF51BCB4A6666B25B4A08168FB65C33E20D8B53087607B9D8D01726 |
| Orc Armor 1.glb | 7944924 | 1 | 0 | 0 | 0 | 771BAA2B50F8394F0C92B0C428DE1A00A93F0F9FB86C7061EB3E5D49668726E8 |
| Orc Armor 2.glb | 7937520 | 1 | 0 | 0 | 0 | 624AD51702A26950A8E6A3C89F8B0A6E88F97CE500A316A18C3DE52EB6B9AF88 |

## Evidencia y alcance
- Originales recibidos de C:\Users\Romelt\Documents. Copiados sin modificar.
- Cabeceras GLB verificadas y JSON interno leído con PowerShell/.NET.
- Get-FileHash -Algorithm SHA256: los tres destinos coinciden con sus originales.
- Cada archivo contiene un material y tres imágenes embebidas según su JSON.
- El cuerpo base tiene skin de 80 articulaciones. Las dos armaduras no tienen skin y cada archivo contiene una sola malla: no se han entregado como piezas independientes listas para intercambiar.
- La presencia de una sola malla no determina si tiene islas geométricas separables. Pendiente inspección visual y geométrica.
- Sin validación visual, de pesos, ajuste o importación en Unreal en esta entrega.

## Trabajo solicitado a CENTRAL
1. Incorporar los originales conservando trazabilidad y preparar un actor/Blueprint configurable en el editor.
2. Inspeccionar ambas armaduras y separar las piezas aprovechables por ranura (por ejemplo cabeza, torso, hombros, brazos, pelvis y piernas), según lo que realmente contengan.
3. Ajustar escala, orientación, pivotes y encaje al cuerpo base. Revisar penetraciones y ocultación del cuerpo bajo armadura.
4. Elegir por pieza: accesorio rígido sujeto a hueso/socket, o malla con pesos al mismo esqueleto para las partes que deban deformarse. No asumir compatibilidad directa con Leader Pose para mallas sin skin.
5. Exponer selección de piezas y presets para generar distintos tipos de orcos desde el editor, reutilizando el cuerpo/esqueleto cuando sea viable.
6. Validar deformaciones y animaciones en Unreal antes de dar por listo el sistema. Si no hay clips en los originales, incorporarlos mediante el flujo de animación acordado.

## Origen
Tripo, según el flujo compartido por la persona: https://studio.tripo3d.ai/es/canvas/projects/01a1d5a7-7b6d-75c1-a04d-c64ddc167d85
No se han verificado licencia, derechos de referencias ni eliminación completa de marcas en estos archivos.

## Pendiente y preguntas
Pendiente ensamblaje modular, adaptación de armaduras, presets y validación en Unreal. Sin preguntas bloqueantes para recibir e inspeccionar los archivos. Esta nota es una entrega nueva solicitada por la persona; no responde a un mensaje pendiente del buzón.