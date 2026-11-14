# Robot spider: bitácora y entrega ASSETS → CENTRAL

## Referencia documentada por captura de la persona
Archivo de evidencia: `referencia_robot_spider_generacion.png`.
- Endpoint: `Image · Text to Image`.
- Modelo visible: **Nano Banana Pro** (no Nano Banana 2).
- Vista múltiple y Eliminar fondo: activados.
- Pose: Ninguno.
- Relación de aspecto: 16:9.
- Resultado visible: araña robótica metálica con ojos mecánicos azules, patas articuladas y cuerpo segmentado, presentada en varias vistas sobre peana circular y fondo oscuro.
- Plataforma/proveedor y versión técnica exacta no identificables con certeza. No se registran credenciales. Los indicadores de tiempo/créditos de la interfaz no se consideran prueba de facturación.

### Prompt transcrito de la captura
```text
araña metálica con ojos mecánicos azules. Sci fi. Pequeña
50cm, 8 Patas. Insectoide. Similar a los Nak de rakion. Delgada
```
Los 50 cm y ocho patas son requisitos del prompt, no dimensiones ni anatomía comprobadas en el GLB. La referencia estilística se conserva como evidencia de procedencia, sin afirmar derechos ni autorización de una franquicia. La captura no establece la fecha exacta de generación.

## Revisión directa del canvas
https://studio.tripo3d.ai/es/canvas/share/01a1d68f-4408-7c6f-b285-3164e78f1f5e
Proyecto visible: `Sin título`; autor mostrado: `romelt766358`.

Siete nodos observados:
1. Subir: referencia frontal de la araña metálica con ojos azules y peana.
2. Subir (1): referencia posterior, cuerpo segmentado y peana.
3. Generar imagen: variante frontal sobre fondo claro, con ojos azules y peana.
4. Malla de topología inteligente: resultado de malla gris, con peana visible en miniatura.
5. Smart UV: resultado UV del modelo.
6. Generación de texturas: resultado texturizado metálico con ojos azules.
7. PBR: resultado de materiales, también con peana visible en miniatura.

No se observan nodos de rigging ni animación. La vista compartida no expuso aristas en el árbol de accesibilidad: la disposición de los resultados no certifica todas las conexiones. Prompts internos de Tripo, versiones, parámetros, costes e IDs de tareas no estaban disponibles. No se realizaron generaciones ni modificaciones al canvas.

## Auditoría estructural del archivo
Original: `C:\Users\Romelt\Pictures\Robot spider.glb`.
Método: lectura del JSON GLB con PowerShell/.NET y Get-FileHash -Algorithm SHA256.
- 6.312.244 bytes.
- Una malla, primitivas TRIANGLES (mode 4).
- 1.957 vértices contabilizados por accessors POSITION; no necesariamente posiciones únicas.
- 2.220 triángulos por recuento de índices.
- Un material y tres imágenes embebidas.
- Sin skins/esqueleto y sin clips de animación.

La inspección es estructural: no se han verificado visualmente el GLB, la anatomía, ocho patas completas, escala o articulaciones. La correspondencia con este canvas procede del contexto aportado por la persona, sin verificación de ID de exportación.

## Trabajo para CENTRAL
- Importar e inspeccionar el asset original conservando trazabilidad.
- Comprobar si la peana observada en las miniaturas está incorporada en la malla; separarla o retirarla en una copia de trabajo si el diseño del juego requiere una araña móvil sin base.
- Verificar ocho patas, integridad de geometría, orientación y escala objetivo de 50 cm (la dimensión exacta a la que aplica no se especifica en el prompt).
- Preparar rig o solución de articulación/animación según las necesidades del juego; el archivo recibido no la incluye.
- Validar materiales, ojos azules, colisiones y comportamiento en Unreal. No considerar validado en juego.
- Derechos de referencias/resultados no verificados. Sin preguntas bloqueantes para recibir e inspeccionar.

Inspección/registro: 2026-11-13.
Carpeta de entrega: \\192.168.31.188\Incoming\ASSETS\20261113-203524_Robot_Spider
SHA-256 GLB: 092A0C933BD5685910CD656C6D442CECB9F962F1DD1CD229E0BCFA3A90ADF55F
SHA-256 captura: 1006A2FE177DFB1147885F7633CE5A7580D495E520F58349F5058852B48D40C0
Copias verificadas: ambas coinciden con los originales. Archivos originales no modificados.
