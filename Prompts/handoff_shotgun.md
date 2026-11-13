# Handoff: la escopeta (Glyphbound), con el mismo proceso que el rifle

**Para:** un chat nuevo de Claude Code en `C:\GameJam` (máquina CENTRAL).
**De:** el chat que rehízo el rifle el 2026-11-12 (bloques 18 a 23 y 25 de `Devlog/days/2026-11-12.md`).
**Antes de nada:** lee `CLAUDE.md`, `Devlog/README.md` y esos bloques de la bitácora: son la historia completa de lo que aquí se resume.

## El encargo
El dueño quiere para la escopeta lo mismo que se hizo con el rifle: un arma 3D en primera persona que se vea sólida y limpia, completa y funcional (gatillo, recarga animada, barra de energía, holograma de munición, luz de punto débil, fogonazo y retroceso), al nivel de la pistola y del rifle, que ya aprobó.

## Qué hay hoy
- **Datos:** la escopeta existe en `Glyphbound/Content/Data/Weapons.json` (`"Id": "Shotgun"`): 8 perdigones de 10, un disparo cada 0,8 s, 6 cartuchos, 4 cargadores de reserva, recarga de 2 s. **No tiene `"ViewModel"`**, así que en el juego se ve con formas provisionales.
- **Arte:** no hay nada de la escopeta en `Glyphbound/SourceArt/Weapons/` ni en `/Game/Glyphbound/Weapons/`.
  - En `C:\Users\VNasher\Downloads` hay un zip de Tripo reciente sin identificar (`tripo_convert_8f45a6e1-....zip`, del 12 nov 22:01: un FBX de 0,7 MB y una sola textura). **No sé si es la escopeta.** Pregunta al dueño cuál es el archivo fuente antes de abrir nada.
- **Código:** las dos armas reales comparten un solo camino en `Glyphbound/Source/Glyphbound/Weapons/GBGun.cpp`, descrito por una tabla de arte por modelo (`FGBGunModelArt`, constantes `PistolArt` y `RifleArt`, y la lista de `FindModelArt`). Añadir un tercer modelo es una constante nueva más una entrada en esa lista, **si el arma encaja en el contrato de abajo**.

## Contrato que el código espera de un modelo
**Mallas en `/Game/Glyphbound/Weapons/<Arma>/`**, todas obligatorias salvo la barra: si falta una, el juego avisa y vuelve a las formas provisionales.
- `SM_<Arma>_Body`: todo lo que no se mueve.
- `SM_<Arma>_Trigger`: origen en su bisagra.
- `SM_<Arma>_Magazine`: origen en su posición de reposo.
- `SM_<Arma>_WeakPointLight`: lo que se enciende al acertar en punto débil.
- `SM_<Arma>_AmmoDisplay`: un panel 2:1 mirando al jugador; U hacia la derecha del jugador.
- `SM_<Arma>_EnergyBar`: solo si la barra es una malla aparte (pistola). En el rifle va dentro del material del cuerpo.

**Ejes y unidades:** cañón hacia +X, origen del cuerpo en el punto de la mano (arriba de la empuñadura), 1 unidad = 1 cm.

**Parámetros de material que pone el juego** en todas las piezas: `ElementColor`; además `ImbueRemaining` (barra de energía de 13 celdas, según el UV 1), `Flash` (luz de punto débil) y `DisplayTexture` (holograma).

**El holograma lo dibuja el juego** si el modelo pone `bDrawDisplayFrame` (rifle): líneas de marco, número y un punto por bala. Con 6 cartuchos saldrán 6 puntos.

## Dos decisiones de diseño que la escopeta obliga a tomar
Son del arquitecto, pero cambian cómo se ve el arma, así que cuéntaselas al dueño en una sola pregunta y sigue con un valor provisional mientras responde.
1. **Recarga.** El código actual es de cargador: saca uno, mete otro y las balas llegan al final (`FinishReload`). Una escopeta clásica se carga cartucho a cartucho por un tubo.
   - Lo que no cuesta código: una escopeta de cargador de caja, que reutiliza la animación de recarga tal cual.
   - Lo que sí cuesta: cartucho a cartucho. Cambia la lógica de munición (recarga parcial, interrumpible) y la animación.
2. **Corredera.** Con 0,8 s entre disparos, lo natural es una corredera que va y vuelve tras cada tiro. Es una pieza móvil nueva (`SM_<Arma>_Pump`) y un movimiento nuevo en `UpdateViewModel`; el contrato de hoy no la tiene.

## El proceso que funcionó con el rifle
1. **Inspecciona la fuente antes de tocarla.** Ábrela en Blender sin interfaz y mira triángulos, piezas sueltas, piezas tras soldar vértices, texturas y mapas que trae.
   - El GLB del rifle era una exportación de pocos polígonos (5.498 triángulos) con una sola textura de color con luz horneada y manchas, sin rugosidad, metálico, normales ni oclusión. Esa era la causa de que se viera "de gelatina", no los ajustes en Unreal.
   - Si la fuente de la escopeta es igual de pobre, dilo pronto: pedir a Tripo una exportación con más detalle y texturas PBR sale más barato que reconstruir. **No gastes créditos de Tripo, Meshy ni Hyper3D sin permiso.**
2. **Renderiza la fuente tal cual desde la cámara del jugador** y míralo tú antes que nadie. `Tools/Blender/rifle_render_review.py` hace las vistas (primera persona, primeros planos y ortográficas a escala para medir); las posiciones de cámara son las del rifle, cópialo y adáptalo.
3. **Reconstruye el aspecto con un script**, no a mano: `Tools/Blender/rifle_build_blend.py` es la plantilla (GLB → `Rifle.blend`, unos 4 minutos). Lo que hace:
   - suelda vértices, tira las normales de Tripo, marca aristas duras y recalcula normales por área de cara;
   - pone un tono plano por panel (mediana de brillo ajustada a una paleta: polímero blanco, dos grises, metal oscuro) y recorta nítidos los detalles pequeños dibujados;
   - hornea en Cycles la oclusión y un mapa de normales con aristas redondeadas;
   - fuerza piezas enteras a un tono cuando sus paneles no coinciden, y limpia manchas por posición en el arma (cada téxel sabe dónde está);
   - modela lo que la fuente trae roto o no trae: en el rifle, el riel, las barras de energía, los paneles laterales, el emisor del holograma y el cargador.
4. **Mide antes de modelar.** Los mapas de profundidad con rayos (BVH de Blender, un rayo cada 5 mm) dieron el plano exacto de cada cara y localizaron un surco que a ojo se leía como "roto".
5. **Enseña renders y espera el visto bueno antes de importar** (regla del dueño). Con el rifle hubo cuatro rondas de notas sobre renders antes del "sí, impórtalo". Dos lecciones:
   - los renders deben ser desde el punto de vista del jugador: el dueño lo dijo expresamente;
   - el render tiene que mostrar lo que mostrará el juego. El holograma se dibujó opaco en el render cuando en el juego era translúcido, y el dueño pidió arreglar algo que el juego no habría enseñado.
6. **Exporta sin reconstruir mallas.** `Tools/Blender/rifle_export_fbx.py` copia cada objeto del `.blend` y lo mueve; un paso por bmesh pierde las normales propias y el arma vuelve a verse blanda. Los pivotes van como propiedades de los objetos.
7. **Importa** con el Editor cerrado: `Tools/Jam/Import-Rifle.ps1` y `Tools/Jam/import_rifle.py` son la plantilla (texturas, materiales construidos por script, comprobaciones de ranuras y del UV de la barra).
8. **Verifica sin PIE:** `Tools/Jam/Build-Jam.ps1` si tocas C++, `Tools/Jam/Smoke-Jam.ps1` (debe acabar en `done=True crashed=False LogGB errors=0`) y amplía las capturas de `Glyphbound/Saved/Screenshots/Smoke/`. La primera captura tras cambiar un material sale beige y granulada: es el material provisional mientras compilan los shaders.
9. **Empaqueta:** `Tools/Jam/Package-Jam.ps1 -Project C:\GameJam\Glyphbound\Glyphbound.uproject -ZipName TeamTBD_Glyphbound` (hoy 263,7 MB).

## Código
- Para el código de este frente el dueño pidió **Sonnet**, no Muse Spark. Funcionó bien como subagente con un encargo escrito: mira `Prompts/feedback14_reload_animation.md` (animación de recarga) y `Prompts/feedback12_rifle_viewmodel.md` (modelo del rifle) como ejemplos, y `Prompts/implementer_rules.md`.
- Revisa lo que entregue mirando las capturas tú mismo: en la recarga, la mano derecha quedaba flotando junto a la empuñadura del rifle y hubo que devolverlo.
- La animación de recarga depende solo del progreso de la recarga y sus valores están en `FGBGunModelArt` (pose, caída del cargador, manos). Un modelo nuevo de cargador solo necesita sus valores.
- El smoke tiene un bloque por arma en `Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp` (busca "rifle:"). La escopeta necesita el suyo.

## Convivencia con otros chats
- Hay otros chats trabajando en la misma carpeta. Ahora mismo uno tiene a medias el código de la Entity (`AI/GBEntity.*`, `AI/GBTentacle*`, y cambios sin commitear en `GBDevCommands.cpp`, `GBEnemyCharacter.*`, `Enemies.json` y `Game.json`).
- **Haz commit solo de tus archivos.** Si un archivo mezcla cambios tuyos y de otro chat (pasó con `GBDevCommands.cpp`), separa el diff por bloques y aplica solo los tuyos al índice (`git apply --cached`).
- Una sola compilación, smoke, importación o empaquetado a la vez en la máquina. Si la compilación falla en archivos ajenos, espera y reintenta.
- Si el Editor está abierto no se puede compilar el objetivo del Editor (Live Coding); el del juego sí (`Build-Jam.ps1 -Target Glyphbound`). No cierres un Editor que no abriste.

## Reglas que aplican (resumen)
- Respuestas al dueño en español; bitácora y commits en inglés, con un bloque nuevo por tanda de trabajo (decisión y motivo).
- Commit y push a `origin main` en cada cambio; nunca force-push.
- Una sola copia de cada asset: la fuente va a `Glyphbound/SourceArt/Weapons/Shotgun/` (fuera de git, con su `README.md` dentro de git) y el resultado a `/Game/Glyphbound/Weapons/Shotgun/`. Nada de FBX ni PNG intermedios guardados.
- Nada de código ni assets de proyectos anteriores. Assets de terceros, con licencia y anotados en la bitácora.
- La pistola y el rifle no se tocan: el dueño los aprobó.

## Estado al entregar
- Último commit de este frente: `052e2be`. El rifle está importado, con animación de recarga, y empaquetado.
- Pendiente de que alguien juegue: el ritmo de la recarga, el holograma en movimiento y el encuadre del rifle con la pantalla más grande.
