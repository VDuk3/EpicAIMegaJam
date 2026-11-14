# Robot: bitácora y entrega ASSETS → CENTRAL

## Referencia de imagen
Captura aportada por la persona: `referencia_robot_generacion.png`.
- Endpoint visible: `Image · Text to Image`.
- Modelo visible: `Nano Banana 2`.
- Vista múltiple: activada.
- Eliminar fondo: activado.
- Pose seleccionada: A-Pose.
- Relación de aspecto seleccionada: 16:9.
- Resultado visible: robot humanoide metálico gris, diseño sencillo, vistas frontal, lateral y posterior sobre fondo oscuro. La selección de opciones no garantiza por sí sola su cumplimiento en el resultado.
- Plataforma/proveedor y versión técnica exacta no identificables con certeza en la captura. No se registran credenciales. El alias de la clave no es una clave API.

### Prompt literal visible
```text
robot cyberpunk sencillo sin demasiados mecanismos.
Humanoide, 170 cm
```
La altura de 170 cm es una intención del prompt, no una medición validada del GLB. No se atribuye autoría manual a esta referencia: esa aclaración correspondía al dibujo inicial de Valkiria.

## Canvas inspeccionado directamente
https://studio.tripo3d.ai/es/canvas/share/01a1d66f-97b6-7bef-ba3b-67bf649f8dc5
Proyecto mostrado: `Sin título`; autor mostrado: `romelt766358`.

Se observaron siete nodos:
1. Subir: referencia frontal del robot.
2. Subir (1): referencia posterior del robot.
3. Malla de topología inteligente: miniatura de malla del humanoide.
4. Smart UV: resultado de procesamiento UV.
5. Generación de texturas: humanoide texturizado gris.
6. PBR: resultado de materiales.
7. Rigging: resultado del humanoide con esqueleto.

Los cinco pasos de procesamiento aparecen dispuestos en fila de izquierda a derecha. La vista compartida no expuso aristas en el árbol de accesibilidad: se documentan los resultados y su disposición, sin certificar cada dependencia. No aparece nodo de animación en esta revisión. Los prompts internos, parámetros, costes, IDs de tareas y fechas de generación no estaban expuestos. No se realizaron generaciones ni cambios al proyecto.

## Auditoría del GLB
Original proporcionado: `C:UsersRomeltPicturesRobot.glb`.
Inspección del JSON GLB con PowerShell/.NET:
- Tamaño: 6.240.448 bytes.
- Una malla; primitivas TRIANGLES (mode 4).
- 9.309 vértices contabilizados por accessors POSITION de las primitivas; no equivale necesariamente a posiciones geométricas únicas.
- 4.257 triángulos por recuento de índices.
- Un skin con 80 articulaciones.
- Un material y tres imágenes embebidas.
- Sin clips de animación.

No se ha abierto el GLB para inspección visual de pesos o deformaciones ni se ha validado en Unreal. La relación con este canvas procede del contexto proporcionado por la persona; el visor compartido no certifica la correspondencia exacta de la exportación.

## Entrega y trabajo pendiente
Se entrega Robot.glb original sin modificaciones y la captura de procedencia fuera del buzón de mensajes. CENTRAL debe importar y verificar escala (objetivo 170 cm), orientación, materiales, esqueleto y deformaciones; incorporar animaciones según el diseño del juego. No dar por validado en juego por contar con rig. Derechos y condiciones de uso no verificados. Sin preguntas bloqueantes para recibir e inspeccionar el asset.

Registro/inspección: 2026-11-13. No acredita la fecha original de generación.
Carpeta CENTRAL: \\192.168.31.188\Incoming\ASSETS\20261113-201741_Robot
SHA-256 Robot.glb: 3FFACB9C63873340C69BE93033ECE7485C6908CABDC32A50B8B3C335CA72EAE3
SHA-256 captura: 7E4E49884970F120D93B48FB84D9E83895E5E30F92B41FE57C1D28503BC778ED
Copias verificadas con Get-FileHash -Algorithm SHA256.
