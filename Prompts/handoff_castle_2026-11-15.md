# Handoff: castillo pixel, paquete final (2026-11-15, ~15:20 EST; la jam cierra a las 20:59 EST)

Lee primero `CLAUDE.md` y la memoria `speed-is-top-priority`. El dueño tiene prisa: respuestas cortas y nada de reanalizar código viejo.

## Estado ahora
- **Editor abierto con MCP** en el puerto 8059 (PID 49764) y con `/Game/Glyphbound/Maps/PastCastle` cargado. El dueño está haciendo cambios a mano. No lo cierres ni compiles sin preguntarle. Para cerrarlo, solo con `Tools/Jam/Stop-EditorMcp.ps1`.
- No hay `machine.lock` ni nada corriendo. El dueño detuvo la última verificación.
- **Último commit:** `900b614` en main, ya subido. El código del juego no cambia desde `77b4d40`; lo posterior son solo documentos.
- **Zip:** `C:\GameJam\Glyphbound\TeamTBD_Glyphbound.zip` (934 MB, empaquetado a las 15:08 desde 77b4d40 con el mapa guardado). **El dueño dice que en ese paquete solo ve la fogata, no el resto de correcciones.** No está diagnosticado.
  - Ojo: el tour empaquetado de las 14:55 sí mostraba la fogata, la luna pequeña y las ventanas emplomadas (`Devlog/media/2026-11-15_castle_entrance_moon_campfire.png`).
  - Primera sospecha: el dueño abrió otra copia del juego, o mira zonas que el tour no cubre. Pregúntale qué falta y dónde lo vio.

## Pregunta abierta del dueño
"En el viewport no se ve como cuando arranco PIE: ¿cómo lo veo igual en tiempo real, con enemigos, Niagara, luces?"
- Le dije: Realtime (Ctrl+R), Lit > Exposure > Game Settings, Game View (G) y Simulate (Alt+S) con Keep Simulation Changes (K). Respondió que **"eso no sale"**: los menús de UE 5.9 son distintos.
- Verifica los nombres reales en 5.9 antes de responder; usa el MCP del Editor si hace falta (`list_toolsets` → `describe_toolsets`).
- Hechos que sí valen:
  - Los enemigos (encuentros prespawneados), la llama pixel de la fogata y de las antorchas (`AGBTorch`, `AGBPixelVfxActor`) y sus luces se crean en BeginPlay. En el editor no se ven sin simular.
  - La exposición nocturna sale del post-proceso del juego.
  - Los agentes no lanzan PIE (regla del proyecto); el dueño sí puede.

## Lo hecho hoy (notas del playtest del castillo, todas en commits subidos)
- **IA (`4dd53fb`):**
  - enemigos del castillo un 35% más rápidos;
  - centauros con más cadencia;
  - cuerpo a cuerpo que ataca en cuanto el jugador está a su alcance;
  - sin dar vueltas cerca del jugador.
- **Mapa (`204715b`; el mapa y los assets no están en git, viven en Content):**
  - columnas que cierran la muralla con la torre de la puerta;
  - luna con menos halo (`MOON` en `Build-PastCastle.py`);
  - fogata `FluidFire` como `AGBTorch`;
  - paneles de vidrio pixel (`M_PxGlass`, `MI_Px_glass_leaded` / `_stained`, 165 emplomados + 20 vitrales; las torres sin vidrio);
  - escaleras con `M_PxStairs` (huella clara y contrahuella oscura);
  - tarima de tablones nivelada (`castle_floor_level.py`);
  - `r.Shadow.Virtual.AllowScreenOverflowMessages=0`.
- **HUD (`77b4d40`):**
  - sin ninguna mano en primera persona;
  - bastones de Romelt (blanco = inicial, `staff_basic`; fuego, hielo y tormenta en `Weapons.json`);
  - hielo y tormenta sin colocar en ningún cofre (provisional, GDD §8).
- **Nota 9 (desnivel entre salas): solo en parte.** Quedan escalones de 28 cm (planta baja) y 48 cm (Top Level) entre los tablones de las salas y el suelo de las puertas. Un intento de escalones intermedios se deshizo: el escaneo confunde la base de los muros con puertas. Pendiente de que el dueño diga cuál vio.

## Cuidado
- **No relances `Build-PastCastle.py`** sin avisar: regenera las etapas y borra los cambios a mano del dueño en esas piezas.
- Para empaquetar y verificar cuando el dueño termine y guarde:
  1. cierra el Editor con `Stop-EditorMcp.ps1`;
  2. ejecuta `Tools/Jam/Verify-Jam.ps1 -Visits` en segundo plano (unos 19 min, o unos 11 sin `-Visits`); deja el zip en la ruta de arriba.
- El zip se sigue llamando `TeamTBD_Glyphbound.zip`: confirma el nombre del equipo antes de entregar.
- Haz commit y push de cada cambio (mensajes en inglés, con la línea Co-Authored-By). La bitácora va en `Devlog/days/2026-11-15_overnight_block.md`.
