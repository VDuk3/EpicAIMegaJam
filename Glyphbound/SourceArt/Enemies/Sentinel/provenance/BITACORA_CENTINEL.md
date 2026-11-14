# Centinel: bitácora y entrega ASSETS → CENTRAL

## Referencia aportada por la persona
Captura: `referencia_centinel_generacion.png`.
- Endpoint visible: `Image · Text to Image`.
- Modelo: **Nano Banana Pro**.
- Vista múltiple y Eliminar fondo: activados.
- Pose: Ninguno.
- Relación de aspecto: 16:9.
- Resultado visible: esfera mecánica metálica plateada, paneles y líneas azules, aberturas circulares y núcleo/orbe azul visible en una de las vistas; tres vistas sobre fondo oscuro.
- Proveedor/plataforma y versión técnica exacta no identificables con certeza. No se registran credenciales ni se interpreta el indicador de créditos como facturación verificada.

### Prompt literal de la captura
```text
esfera flotante sci fi de metal con orbe azul en el centro visible
solo de un lado
```
El salto de línea refleja la presentación del campo. La visibilidad desde un solo lado y la flotación son intenciones del prompt: no constituyen pruebas de geometría o comportamiento del asset. Fecha exacta de generación no acreditada por la captura.

## Revisión directa del canvas
https://studio.tripo3d.ai/es/canvas/share/01a1d744-0c92-7762-8061-4ac8fa12e6a4
Proyecto: `Sin título`; cuenta mostrada: `romelt766358`.

Siete nodos observados:
1. `Subir`: vista oblicua de esfera con aberturas circulares y paneles.
2. `Subir (1)`: vista con núcleo azul claramente visible.
3. `Subir (2)`: vista posterior con paneles y piezas centrales oscuras.
4. `Malla de topología inteligente`: resultado gris de esfera mecánica.
5. `Smart UV`: resultado de procesamiento UV.
6. `Generación de texturas`: esfera texturizada con centro azul claro.
7. `PBR`: resultado metálico con centro azul/cian.

No se observan nodos de rigging ni animación. La vista compartida no expuso aristas en el árbol de accesibilidad: la fila de resultados no certifica todas las dependencias. No estaban disponibles prompts internos de Tripo, modelos/versiones, parámetros, costes, IDs ni fechas originales. No se realizaron nuevas generaciones ni cambios al proyecto.

## Auditoría estructural del GLB
Original: `C:\Users\Romelt\Pictures\Centinel.glb`.
Método: JSON GLB leído mediante PowerShell/.NET y Get-FileHash -Algorithm SHA256.
- 6.881.028 bytes.
- Una malla con primitivas TRIANGLES (mode 4).
- 3.335 vértices por accessors POSITION, no necesariamente posiciones únicas.
- 5.056 triángulos por recuento de índices.
- Un material y tres imágenes embebidas.
- Sin skins/esqueleto ni clips de animación.
- Material con base color, metallic/roughness, normal y doubleSided; no declara emissiveFactor ni emissiveTexture.

El aspecto brillante del núcleo en la referencia no equivale a emisión configurada en el GLB. No se ha inspeccionado visualmente el archivo ni validado en Unreal. La correspondencia con el canvas procede del contexto aportado por la persona; no se verificó un ID de exportación.

## Trabajo pendiente para CENTRAL
Importar e inspeccionar escala, orientación, pivote, materiales y colisión. Comprobar que el núcleo/orbe sea visible solo desde el lado previsto: la captura y los nodos no certifican ese requisito para toda la geometría. Preparar material emisivo del núcleo si se requiere y movimiento de flotación mediante el actor/Blueprint según el diseño del juego; el archivo no contiene animación de flotación. Una esfera rígida puede moverse sin un esqueleto propio. No considerar validado en juego hasta comprobar en Unreal.
Derechos/condiciones de uso no verificados. Sin preguntas bloqueantes para recibir e inspeccionar el asset.

Registro/inspección: 2026-11-13, sin acreditar fecha original de generación.
Carpeta CENTRAL: \\192.168.31.188\Incoming\ASSETS\20261113-234226_Centinel
SHA-256 GLB: 0A6F06AA647B0B70DAC1D7A100EB4954B8EC9D28D8D41D623D9DA78C22C0ACC5
SHA-256 captura: 826B64B420EA46506F665D9C051DA71C90DE8D5C9E191BB7995FD669387F1A3B
Copias verificadas; originales conservados sin modificaciones.
