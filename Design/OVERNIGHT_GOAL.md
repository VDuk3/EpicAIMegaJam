# Overnight goal (2026-11-15): instrucciones del dueño

> **Relee este archivo al terminar cada punto, antes de empezar el siguiente**, y actualiza la tabla de estado del final.
> Prompt del equipo, en español (permitido por CLAUDE.md). Si se cita en la bitácora, se añade la traducción.

## Reglas de trabajo

- **Trabaja hasta que los 7 puntos estén hechos y en buen estado.** Un punto no se da por hecho hasta que el resultado sea satisfactorio de verdad.
- **Subagentes:** Opus, Sonnet o Haiku, según la dificultad de cada tarea (Muse sigue siendo el implementador por defecto de CLAUDE.md; cada excepción se anota en la bitácora con su motivo).
- **Auditoría:** revisa todo lo que hagan los subagentes como si fuera trabajo ajeno: lee el diff, compila, corre el smoke filtrado y mira capturas o renders. La revisión de código sigue siendo con Sol 6.1 (Codex, a través del wrapper con registro).
- **El dueño está durmiendo y lejos varias horas:**
  - no le preguntes nada;
  - no propongas separar trabajo en otros chats o sesiones;
  - las decisiones las tomas tú y, si dudas, junto con el advisor (Fable 5.2);
  - las decisiones de diseño se marcan como provisionales en `Design/GDD.md` y se anotan en la bitácora.
- **Arte:** esta noche el dueño autoriza importar arte sin su OK previo (sustituye a la regla "ask before importing art"). La auto-revisión sigue vigente: captura cada zona y cada enemigo, critícalo y corrígelo antes de darlo por bueno. Pixel art = bloques limpios.
- **Fab:** autorización total para revisar la biblioteca de Fab del dueño y descargar lo que haga falta. Cada asset de terceros se anota en la entrada del día (nombre, fuente, licencia) y en "Tools you used" si aplica.
- **Cierre de la jam: hoy, 15 nov, 20:59 EST.**
  - Nunca dejes el juego roto: un commit con push por punto (o por sub-paso estable).
  - Al final tiene que haber un paquete jugable de menos de 1 GB.
  - Si se acerca la hora, prioriza un build entregable sobre terminar el punto en curso.
- **Sin PIE:** se verifica con compilación, smoke filtrado offscreen, capturas o renders y tours del juego empaquetado. Lo que necesite ojos humanos se lista para el dueño al final.
- Siguen vigentes todas las reglas de `CLAUDE.md`: bitácora, contenido del juego en inglés, Teen, iteración barata, lock de máquina compartida (`Glyphbound/Saved/machine.lock`) y commit solo de tus cambios.

## Los 7 puntos

### 1. Valquiria del mundo cyberpunk (3D)
- **Alas:** ahora son un desastre. Rigging para que se mantengan en su posición, estables, como las alas de espalda de cualquier humanoide de fantasía.
- **Socket de la mano:** vigílalo de cerca. El arma tiene que quedar bien ajustada; compruébalo con capturas en varias poses y animaciones.
- **IA:** combate tanto de cerca como de lejos, con al menos un hechizo. Se permiten placeholders de animación o VFX si funcionan.

### 2. Muerte del dron y del centinela
Hoy, al morir, se quedan quietos un segundo y luego desaparecen, y eso es muy poco natural. Tienen que caer al suelo y, al tocarlo, sacar chispas, explotar o desarmarse (o algo parecido). Tiene que verse mucho más natural.

### 3. Animaciones de muerte para los enemigos 3D
Fuera el ragdoll en **todos** los enemigos 3D. Cada uno tiene su animación de muerte, a ser posible distinta para cada enemigo. Busca en Fab: está autorizado revisar y descargar.

### 4. Humo o niebla en los túneles hacia la sala del boss de tentáculos
- Sistema Niagara hecho con **Niagara Fluids**.
- Se mueve lento y reacciona a físicas: el paso del jugador, los disparos y la colisión con el túnel.
- **La calidad va por encima de todo.** Nada de figuras, siluetas o formas de nube: tiene que verse como humo de la vida real.
- Vigila el coste en el RTX 4080 Laptop (CENTRAL).

### 5. Enemigos pixel art del castillo medieval
- **Orcos:** dales animaciones de combate, aunque sean sencillas. Nada de sprites estáticos que caminan hacia el jugador como si fueran cartones.
- **Enemigos nuevos:**
  - típicos de los juegos pixel art de fantasía medieval en primera persona y de los RPG de los años 90;
  - variados, pero no demasiados: grandes y pequeños, unos que aguanten más y otros menos;
  - tiene que haber atacantes a distancia (magia o ataque físico: flechas, perdigones o lo que mejor encaje);
  - cada uno con animaciones y VFX acordes.
- **Colocación:**
  - analiza cada zona abierta del castillo: desniveles, piso superior, pasillos cercanos;
  - monta combates equilibrados y diversos en cada una.
- **Boss final del castillo: la Valquiria pixel art.**
  - Elige la mejor zona en la parte más alta del castillo.
  - Los assets ya existen, pero puede que sigan en la laptop: búscalos primero en la repo, en `Incoming` y en el historial de git.
  - Si de verdad no están, deja la zona, la IA y el spawn listos con un placeholder del mismo tamaño, y anótalo en el reporte final para el dueño.

### 6. IA de los enemigos del castillo
- El mismo nivel de trabajo de IA que ya se hizo con los enemigos del mundo cyberpunk (reutiliza ese sistema donde se pueda).
- Cada enemigo se comporta según su estilo de combate (más cerca o más lejos).
- Usan coberturas, se juntan de a varios para atacar al mismo tiempo e intentan acorralar al jugador.
- Son inteligentes y se mueven en grupo.

### 7. Plan de portales entre los mundos
- Plan, en el GDD, de dónde poner un portal en cada mundo.
- El jugador avanza alternando épocas y se ve un poco obligado a ir de una a otra.
- El castillo es más grande, pero eso no tiene por qué desequilibrar el recorrido.
- Idea del dueño: zonas del castillo inaccesibles que solo se alcanzan desde el mundo cyberpunk. El resto queda a tu creatividad.

## Estado (actualizar al terminar cada punto)

| # | Punto | Estado | Evidencia (commit, captura, smoke) |
|---|-------|--------|-------------------------------------|
| 1 | Valquiria cyberpunk: alas, socket, IA | hecho | alas rígidas en la espalda (commit b65ae01, renders en siete clips); espada en la mano con agarre cerrado (captura valkyrie_hand del juego empaquetado); IA cerca y lejos con Juicio y lanzas, fase dos (commit ec56b69; smoke Valkyrie en verde, también empaquetado) |
| 2 | Muerte del dron y del centinela | hecho | commit 5490c4d: caen, al tocar el suelo chispean y explotan, sueltan piezas (rotores, ojo, núcleo), se hunden; smoke Enemies3D 3 de 3 en verde ("Drone_Future wreck: fell 0.78 s, bounces=13, pieces=6, spark bursts=3") |
| 3 | Animaciones de muerte de los 3D | hecho | commit 5490c4d: sin ragdoll; caídas completas del ARPG Pack (Fab) reasignadas, una distinta por tipo de orco, robot y Valquiria; pelvis a 12-25 cm del suelo en el smoke; renders en Saved/Review/deaths |
| 4 | Humo con Niagara Fluids en los túneles | hecho | commits 8c67581 y 2d1fc75: gas Grid3D de Niagara Fluids en el pasillo oscuro y en los últimos 15 m del pasillo gastado, bajo y lento, sin formas de nube (rango de densidad 0-5); el jugador lo empuja según su velocidad, los disparos y las explosiones lo rasgan, choca con el túnel. gb.Smoke FogVisit en verde en el mapa real: simula, la inyección y los disparos llegan al canal de datos (312 y 264 entradas), cuesta unos 0,8-1,4 ms offscreen en el RTX 4080 Laptop; capturas fogvisit_*_before/after |
| 5 | Enemigos pixel art y boss Valquiria | hecho | commits 9b49578 y c4ec4ac: hojas de 8 direcciones horneadas en Blender (guerrero y campeón orco, bruto, chamán, barghest, centauro arquero, Valquiria pixel) con ataque, golpe y muerte; proyectiles con VFX (fuego arcano, flechas). Ocho peleas, una por zona abierta, y la Valquiria pixel en la sala más alta del Top Level (commit 2d1fc75). gb.Smoke CastleVisit en PastCastle: las 8 peleas detectan, atacan, mandan todas sus oleadas (oleadas atacando = oleadas que llegan) y terminan limpias; la Valquiria recibe daño, hace combos, lanza Juicio y muere. Los assets pixel de la Valquiria no estaban (solo el dibujo suelto 16_pixelart_valki.png): la hoja se horneó del modelo 3D con esos colores |
| 6 | IA de grupo en el castillo | hecho | commit 9b49578: la IA de escuadra del cyberpunk reutilizada para los sprites (distancia por estilo, coberturas, turnos), más pinza desde lados opuestos y acorralar hacia los lados abiertos del jugador; smoke Castle en la arena 3 de 3 en verde y CastleVisit en el mapa real (líneas TACTICS por escuadra) |
| 7 | Plan de portales | hecho | GDD v0.70 §4 "Portals and the route between the eras": P0-P4, seis tramos alternos, zonas del castillo solo desde el futuro (commit 945e6c6) |

## Al terminar todo
1. Suite completa y `Smoke-Jam.ps1` completo, una sola vez, en verde.
2. `Build-Jam.ps1 -Target Glyphbound` y el paquete (< 1 GB), con el tour del juego empaquetado.
3. Entrada de bitácora del día con las decisiones, las excepciones de implementador y los assets de terceros.
4. Commit y push.
5. Revisión final con el advisor y reporte al dueño: qué quedó, qué necesita sus ojos (PIE) y qué decisiones se tomaron.
