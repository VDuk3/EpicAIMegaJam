# Drone: bitácora y entrega ASSETS → CENTRAL

## Referencia de imagen
Captura aportada: `referencia_drone_generacion.png`.
- Endpoint: `Image · Text to Image`.
- Modelo visible: **Nano Banana Pro**.
- Vista múltiple y Eliminar fondo: activados.
- Pose: Ninguno.
- Relación de aspecto: 16:9.
- Resultado visible: dron metálico oscuro, rotores/hélices, tren de aterrizaje y módulo inferior esférico con lente roja; vistas frontal, lateral y posterior.
- Proveedor/plataforma y versión técnica exacta no identificables con certeza; no se registran credenciales ni facturación inferida de indicadores de interfaz.

### Prompt literal
```text
aerial laser drone
```
El término laser es intención del prompt: no acredita un sistema de disparo funcional. La captura no acredita fecha exacta de generación.

## Canvas inspeccionado directamente
https://studio.tripo3d.ai/es/canvas/share/01a1d751-c925-7a36-8b12-028dd97cd97f
Proyecto visible: `Sin título`; cuenta mostrada: `romelt766358`.

Se observaron **14 nodos en dos grupos visuales distintos**.

### Grupo superior: motocicleta (no atribuido al GLB Drone)
`Subir`, `Generar imagen (1)`, `Generar imagen`, `Malla de topología inteligente`, `Smart UV`, `Generación de texturas`, `PBR`.
Las miniaturas muestran una motocicleta, inicialmente con apariencia de boceto y después renderizada/texturizada. No se ha confirmado autoría manual de ese boceto y no se vincula esa rama al archivo de dron entregado.

### Grupo inferior: dron
- `Subir (1)`: referencia frontal con módulo/lente roja inferior.
- `Subir (2)`: referencia posterior.
- `Subir (3)`: referencia lateral.
- `Malla de topología inteligente (1)`: resultado de geometría del dron.
- `Smart UV (1)`: procesamiento UV.
- `Generación de texturas (1)`: resultado texturizado con módulo rojo.
- `PBR (1)`: resultado de materiales del dron.

No se observan nodos de rigging ni animación. La vista compartida no expuso aristas en el árbol de accesibilidad: la disposición no certifica cada dependencia. Prompts internos de Tripo, versiones/modelos, costes, parámetros, IDs y fechas originales no estaban disponibles. No se realizaron generaciones ni modificaciones al canvas.

## Auditoría estructural del GLB
Original: `C:\Users\Romelt\Pictures\Drone.glb`.
Inspección de JSON GLB mediante PowerShell/.NET:
- 5.931.456 bytes.
- Una malla; primitivas TRIANGLES (mode 4).
- 4.570 vértices contabilizados por accessors POSITION, no necesariamente posiciones únicas.
- 5.645 triángulos por recuento de índices.
- Un material y tres imágenes embebidas.
- Sin skins/esqueleto ni clips de animación.

No se inspeccionó visualmente el GLB ni se validó en Unreal. No se afirma que las hélices o el módulo inferior estén separados: una sola malla no determina la conectividad de sus partes. Correspondencia con canvas basada en contexto aportado, sin ID de exportación verificado.

## Trabajo pendiente para CENTRAL
Importar e inspeccionar geometría, escala, orientación, pivote, materiales y colisiones. Si se necesitan hélices giratorias o módulo inferior orientable, comprobar y separar componentes en copia de trabajo o preparar el sistema apropiado; no hay animaciones incorporadas. Implementar vuelo, orientación y efecto/disparo láser según diseño de juego, sin asumir esas funciones por el prompt. Validar apariencia, movimiento y gameplay en Unreal antes de considerarlo listo.
Derechos/condiciones de uso no verificados. Sin preguntas bloqueantes para recibir e inspeccionar el asset.

Registro/inspección: 2026-11-13; no acredita fecha original de generación.
Carpeta CENTRAL: \\192.168.31.188\Incoming\ASSETS\20261113-235807_Drone
SHA-256 GLB: 68BA9D7ED6CC6350F9F0AC65201D3C933D4E94A173BDDD296EE6FE8CD0A8FB9E
SHA-256 captura: 9D0DEBCA86640C3826335CFC812858D852677A91B576BFBB976456C25FD91ABA
Copias verificadas; originales sin modificar.
