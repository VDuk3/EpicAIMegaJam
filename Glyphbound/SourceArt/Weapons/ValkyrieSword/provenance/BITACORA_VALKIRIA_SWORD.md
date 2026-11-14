# Valkiria Sword: bitácora y entrega ASSETS → CENTRAL

## Procedencia de la referencia
La persona confirma que **la primera referencia es un dibujo a mano**. En el canvas se observa una imagen de espada dibujada sobre papel en el nodo `Subir`.
La autoría concreta de este dibujo de espada no fue indicada. No se atribuye automáticamente a Angélica Leon: su crédito confirmado anteriormente corresponde al dibujo inicial de Valkiria.
El dibujo manual se distingue de las iteraciones de imagen posteriores asistidas por IA.

## Canvas revisado directamente
https://studio.tripo3d.ai/es/canvas/share/01a1d69f-cbda-72ac-b2b8-5444731015e1
Proyecto visible: `Sin título`; autor de cuenta mostrado: `romelt766358` (no prueba autoría del dibujo). Sección: `Draft to Assets`.

Siete nodos visibles, descritos en orden espacial de izquierda a derecha:
1. `Subir`: imagen del dibujo sobre papel.
2. `Generar imagen (1)`: iteración que conserva la presentación sobre papel.
3. `Generar imagen`: interpretación renderizada de la espada sobre fondo gris, con hoja alargada, guarda ornamental y detalles de color.
4. `Malla de topología inteligente`: resultado de malla de la espada.
5. `Smart UV`: resultado de procesamiento UV.
6. `Generación de texturas`: resultado texturizado.
7. `PBR`: resultado de materiales.

No hay nodos visibles de rigging ni animación. La vista compartida no expuso aristas en el árbol de accesibilidad: el orden espacial no certifica cada conexión. No estaban disponibles prompts literales, modelos/versiones, parámetros, costes, IDs de tareas ni fechas originales. No se realizaron nuevas generaciones ni cambios al canvas. La inspección se realizó el 2026-11-13, sin afirmar que sea la fecha original de creación.

## Auditoría estructural del GLB
Original: `C:\Users\Romelt\Pictures\Valkiria Sword.glb`.
Método: lectura de JSON GLB mediante PowerShell/.NET y Get-FileHash -Algorithm SHA256.
- 18.413.680 bytes.
- Una malla con primitivas TRIANGLES (mode 4).
- 10.375 vértices contabilizados por accessors POSITION, no necesariamente posiciones únicas.
- 17.868 triángulos por recuento de índices.
- Un material y tres imágenes embebidas.
- Sin skins/esqueleto y sin clips de animación.

No se inspeccionó visualmente el GLB ni se validó en Unreal. El vínculo con este canvas procede del contexto aportado por la persona, sin correspondencia de ID de exportación verificada.

## Entrega y trabajo pendiente para CENTRAL
Se entrega el original sin modificar. Importar como arma/accesorio de Valkiria; comprobar materiales, orientación, escala, pivote en empuñadura y acoplamiento a mano/socket. Preparar colisión según el gameplay y verificar agarre/penetraciones durante animaciones. No requiere asumir un esqueleto propio para su uso como accesorio rígido. No se considera listo en juego hasta validar en Unreal.
La referencia inicial es manual según confirmación de la persona; autoría concreta y derechos/condiciones de uso aún no documentados. Sin preguntas bloqueantes para recibir e inspeccionar el asset.

Carpeta CENTRAL: \\192.168.31.188\Incoming\ASSETS\20261113-215857_Valkiria_Sword
SHA-256 GLB: B33DCD22D8E9DC30C55318CD7FC8058343147C643563B93421ECB667136A0688
Copia verificada: coincide con el original.
