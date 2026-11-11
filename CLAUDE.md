# CLAUDE.md (proyecto de la Epic AI MegaJam 2026)

Juego para la Epic AI MegaJam 2026, en **Unreal Engine 5.9.0** (CL 60351404). La jam va del 12 nov 00:00 EST al **15 nov 20:59 EST**; pasada esa hora no se aceptan entregas. El tema se anuncia el 11 nov.

## Reglas duras
- **La bitácora lo es todo** (objetivo: el premio Glass Box). Lee `Devlog/README.md` antes de trabajar.
  - Claude Code queda registrado solo por los hooks de usuario (`Tools/Devlog/Install-DevlogHooks.ps1`). No los desactives y no borres `Devlog/.jamroot`.
  - Cualquier otro agente o CLI se lanza SOLO con `Tools/Devlog/Invoke-LoggedAgent.ps1` (con `-Shell cmd` si el comando usa `<`).
  - El Editor se cierra SOLO con `Tools/Jam/Stop-EditorMcp.ps1`, que además recoge su log.
  - Al cerrar el día: `Tools/Devlog/New-DevlogDay.ps1`.
- **Nada de código ni assets de proyectos anteriores** (en particular, nada de Panthelia). Todo se crea durante la jam o está declarado en `Devlog/PRE_JAM_DECLARATION.md`.
- **Assets de terceros:** solo con licencia válida, y cada uno anotado en la entrada del día (nombre, fuente, licencia).
- **Gameplay:** tiene que ir más allá de las plantillas de Epic, y no puede ser una extensión de un tutorial oficial de Epic. Tiene que mostrar el tema de la jam.
- **Todo el contenido del juego va en INGLÉS** (regla 7(i) de las Official Rules): textos en pantalla, audio y voces, el video y los datos de la entrega. La bitácora (Glass Box) se escribe **en inglés desde el inicio** (decisión del dueño, 2026-11-10): entradas `Devlog/days/*.md`, pies de `media/`, `PRE_JAM_DECLARATION.md`, `PREJAM_PREP.md`, y también los **mensajes de commit**, porque el borrador diario los lista. Los registros crudos (`Devlog/raw`) y los prompts del equipo pueden estar en español; si se citan en una entrada, se añade la traducción.
- **Contenido apto para Teen (13+).** Sin marcas ni logos ajenos; el único permitido es el logo de Unreal Engine, según sus guías de marca.
- **Historial de git limpio y completo:** Epic puede pedir el historial de control de versiones y los materiales preparados antes de la jam para verificar.
- **No debe requerir software adicional para jugarse** (por ejemplo, nada de un servidor local de LLM).
- **Respuestas** al equipo en español.

## Flujo
- Prioridad: un juego jugable y empaquetado cada día, no funcionalidades a medias.
- **Empaquetar:** `Tools/Jam/Package-Jam.ps1 -Project <uproject> -ZipName <Equipo>_<Juego>`. Hazlo al final de CADA día, no solo el último.
- **Editor con MCP:**
  - abrir con `Tools/Jam/Start-EditorMcp.ps1 -Project <uproject> -Port 8059`;
  - probar con `Tools/Jam/Test-UnrealMcp.ps1`;
  - cerrar con `Tools/Jam/Stop-EditorMcp.ps1` (nunca matar el proceso).
  - Recién abierto, el Editor responde "slow task" durante un rato: espera y reintenta.
- **MCP de Unreal:** descubrimiento progresivo (`list_toolsets` → `describe_toolsets` → `call_tool`). No inventes nombres de tools.
- **Commits** pequeños y frecuentes, con mensajes claros: el borrador del día los lista.

## Tests y verificación
Reglas que salen de lo que nos costó horas en el proyecto anterior. El porqué y las cifras están en `Devlog/PREJAM_PREP.md`, sección "Lessons from our previous project: tests and verification time"; léela antes de escribir un test o una herramienta de Editor.
1. **Pocos tests, y solo de lógica pura** que sea fácil de romper y difícil de ver jugando (puntuación, guardado, máquinas de estado, generadores, matemáticas). Toda la suite por debajo de 30 s. Lo demás se verifica jugando el juego empaquetado.
2. **Verde o rojo.** Nada de listas de "fallos conocidos", tests excluidos ni catálogos. Un test en rojo se arregla o se borra el mismo día.
3. **Cada test tiene que poder fallar:** antes de darlo por bueno, pregúntate qué implementación incorrecta lo pasaría igual. No escribas tests que copian constantes, miran el texto del código o comprueban que un campo existe.
4. **Corridas baratas:** filtros anclados (`^Game.Save.`, con `^` y punto final), varios filtros unidos con `+` en una sola corrida, y un mapa inicial del Editor casi vacío (no el nivel de juego). El nombre de un test nunca puede ser también la carpeta de otro (`A.B` junto a `A.B.C`): Unreal deja de correr `A.B` sin avisar.
5. **Mide antes de optimizar:** arranque del Editor, suma de duraciones de los tests y tiempo total. Mide el conjunto, no una parte, y compara sobre el mismo código.
6. **Nada puede costar en proporción a todo lo cargado:** ni una llamada de herramienta ni un test. No uses `CollectGarbage(RF_NoFlags)` en un test (descarga los assets; usa `GARBAGE_COLLECTION_KEEPFLAGS`), no guardes una caché por tamaño y fecha de archivo (Unreal da la fecha con precisión de un segundo), y usa referencias blandas en el Blueprint del jugador para lo que no haga falta al aparecer.
7. **UE 5.9.0 fijo hasta entregar.** Si algo falla sin que haya cambiado el código, compara primero la versión del motor en los dos logs.

Scripts de un solo uso: en un archivo, no en línea dentro de un comando; en ASCII o con BOM si son `.ps1` (PowerShell 5.1 lee como ANSI un script sin BOM); y en JavaScript, para reemplazos literales usa `split(a).join(b)`, no `replace(a, b)`.

## Entrega (checklist)
- Zip Win64 empaquetado, menor de 1 GB, llamado `<Equipo>_<Juego>.zip` y sin `.pdb`.
- Subido a itch.io y enviado a la jam antes de la hora de cierre.
- Video de gameplay de 30 a 60 s, **1920×1080 o más**, en inglés y con gameplay en los primeros 5 s.
- Link a la bitácora en inglés (Glass Box), entregado junto con el juego **antes del cierre**.
- Nombre del equipo, nombre del juego, y nombre completo y email de la cuenta de Epic de cada miembro (uno es el Team Owner).
- Assets de terceros listados y trabajo previo declarado.
