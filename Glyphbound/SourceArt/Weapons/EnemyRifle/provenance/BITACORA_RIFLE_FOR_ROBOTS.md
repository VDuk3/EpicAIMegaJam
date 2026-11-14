# Rifle for Robots: bitácora y entrega ASSETS → CENTRAL

## Referencia de imagen aportada por la persona
Evidencia archivada: `referencia_rifle_generacion.png`.
- Endpoint visible: `Image · Text to Image`.
- Modelo visible: **Nano Banana Pro**.
- Vista múltiple y Eliminar fondo: activados.
- Pose: Ninguno.
- Relación de aspecto: 16:9.
- Resultado visible: rifle negro de ciencia ficción con detalles luminosos azules, cargador, culata y varias vistas sobre fondo oscuro.
- Plataforma/proveedor y versión técnica exacta no identificables con certeza. No se registran credenciales ni se considera el indicador de créditos prueba de facturación.

### Prompt literal
```text
black sci fi assault rifle
```
La captura documenta procedencia y opciones visibles, no la fecha exacta de generación ni el cumplimiento de cada opción en el resultado.

## Canvas inspeccionado directamente
https://studio.tripo3d.ai/es/canvas/share/01a1d72c-e8cd-77ce-84d7-cf0fac10ae8d
Proyecto: `Sin título`; autor de cuenta mostrado: `romelt766358`.

Siete nodos observados:
1. `Subir`: vista lateral del rifle presentada verticalmente.
2. `Subir (1)`: vista desde la zona posterior/culata.
3. `Subir (2)`: vista oblicua desde la boca del cañón.
4. `Malla de topología inteligente`: resultado de geometría gris.
5. `Smart UV`: resultado UV.
6. `Generación de texturas`: rifle negro con detalles azules.
7. `PBR`: resultado de materiales.

No se observan nodos de rigging o animación. Los cuatro resultados de procesamiento se disponen en fila; la vista compartida no expuso aristas en el árbol de accesibilidad, por lo que no se certifica cada dependencia. Prompts internos de Tripo, versiones/modelos, costes, parámetros, IDs y fechas originales no estaban disponibles. No se realizaron generaciones ni cambios al canvas.

## Auditoría estructural GLB
Original: `C:\Users\Romelt\Pictures\Rifle for Robots.glb`.
Lectura de JSON GLB mediante PowerShell/.NET:
- 8.094.588 bytes.
- Una malla; primitivas TRIANGLES (mode 4).
- 1.947 vértices contabilizados por accessors POSITION, no necesariamente posiciones geométricas únicas.
- 2.082 triángulos por recuento de índices.
- Un material y tres imágenes embebidas.
- Sin skins/esqueleto ni clips de animación.

No se inspeccionó visualmente el GLB ni se validó en Unreal. La relación con el canvas proviene del contexto aportado por la persona; no se verificó un ID de exportación.

## Entrega y pendiente para CENTRAL
Se entrega original sin modificar, captura y esta bitácora. Importar como accesorio para robots y comprobar escala, orientación, pivote y acoplamiento a mano/socket; revisar agarre a dos manos, boca de disparo y colisiones según gameplay. Verificar el material azul: el aspecto luminoso en la referencia no confirma emisión funcional en Unreal. Si se requiere recarga con cargador móvil o gatillo animado, comprobar/separar las piezas y preparar el movimiento: no hay clips y una sola malla no garantiza componentes independientes.

Pendientes validación de apariencia, integración y gameplay en Unreal. Derechos/condiciones de uso no verificados. Sin preguntas bloqueantes para recibir e inspeccionar el asset.

Inspección/registro: 2026-11-13 (no certifica fecha original de generación).
Carpeta CENTRAL: \\192.168.31.188\Incoming\ASSETS\20261113-231717_Rifle_For_Robots
SHA-256 GLB: C1983B626A7256A266B5CD5FB9C4EC3CE83AB9C987AE10031DB6008F99212AD8
SHA-256 captura: EA4021D46CEC5A47576C46FC41040DA296F9F8880C97CE864B920EEE27440337
Copias verificadas con Get-FileHash -Algorithm SHA256; originales conservados.
