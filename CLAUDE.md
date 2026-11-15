# CLAUDE.md (proyecto de la Epic AI MegaJam 2026)

Juego para la Epic AI MegaJam 2026, en **Unreal Engine 5.9.0** (CL 60351404). La jam va del 12 nov 00:00 EST al **15 nov 20:59 EST**; pasada esa hora no se aceptan entregas. **Tema (anunciado el 11 nov): DIRECTOR'S CUT** — "Your vision, your edits, your final cut": la mecánica que quedó fuera del alcance, la idea archivada; y dirigir agentes de IA también cuenta (fijar la visión, delegar, decidir qué entra al corte final).

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
- **PIE (Play In Editor) lo hace SIEMPRE un humano.** Los agentes de IA no lanzan PIE (trabajan muy mal con él): compilan, empaquetan o corren tests y luego piden a una persona que pruebe.
- **MCP de Unreal:** descubrimiento progresivo (`list_toolsets` → `describe_toolsets` → `call_tool`). No inventes nombres de tools.
- **Commits** pequeños y frecuentes, con mensajes claros: el borrador del día los lista.
- **Push automático:** cada commit se sube enseguida a `origin main`, sin preguntar (dueño, 2026-11-12: los agentes de las otras máquinas tienen que ver la repo en todo momento). Esto prevalece sobre el "nunca push salvo que se pida" de la regla global. Nunca force-push.
- **Implementador:** por defecto, la implementación la hace **Muse Spark 1.3 Contributor en High** a través de OpenCode (`opencode-go/muse-spark-1.3-contributor#high`, ya fijado en `opencode.jsonc`). Excepciones solo por decisión del arquitecto o de quien esté de turno: cambios pequeños que haga directamente el orquestador, o una tarea demasiado compleja para Muse que se le da a Opus u otro modelo. Si se hace una excepción, se anota el motivo en la entrada del día. OpenCode se lanza SOLO con `Tools/Devlog/Invoke-LoggedAgent.ps1`.

## Autonomía del arquitecto
- El **arquitecto** es el chat de Claude Code que esté implementando en ese momento. Su fuente de verdad del diseño es `Design/GDD.md`.
- **Las decisiones técnicas y de ingeniería las toma el arquitecto, sin consultar.** Nadie del equipo es programador: no se pregunta por arquitectura de código, C++ o Blueprints, estructura de clases, plugins, herramientas, formatos de datos, optimización ni nada parecido. Se decide, se ejecuta y se anota en la bitácora (decisión y motivo, que también suma para Glass Box).
- Si falta un dato de diseño (gameplay, arte, historia), el arquitecto **no se detiene**: elige un valor razonable y provisional, lo marca en el GDD como provisional y sigue. Solo pregunta al dueño cuando una decisión de diseño cambie de verdad la visión del juego, y lo hace en una sola pregunta, sin parar el resto del trabajo.
- El tiempo manda: entre dos opciones correctas, gana la que antes deja un build jugable.

## Tiempo de iteración (el tiempo premia en este proyecto)
- **Plugins:** el proyecto se crea con los plugins que ya trae el motor por defecto **desactivados si no se usan**, porque alargan el arranque y la compilación. Se dejan solo los que el juego o el flujo necesitan (el MCP, Enhanced Input, el de DLSS si se usa, Paper2D si se usa para sprites, etc.). Se mide el arranque del Editor antes y después.
- **Compilar poco:** cambios solo en `.cpp` con Live Coding; build completo solo cuando cambian cabeceras, `UPROPERTY`/`UFUNCTION` o módulos. Una compilación o corrida a la vez por máquina.
- **No correr la suite entera en cada cambio:** en cada cambio, solo el filtro anclado de lo que se tocó. La suite completa (por debajo de 30 s) una vez antes de empaquetar al final del día.
- **Arranque del Editor:** mapa inicial casi vacío, `bEnableUncontrolledChangelists=False` y, en corridas sin ventana, `-unattended -nopause -nosplash -NullRHI -nosound -nocrashreports` (detalles en `Devlog/PREJAM_PREP.md`, "Where the time really went").
- **Prueba automática (smoke) filtrada mientras se itera** (dueño, 2026-11-13: sacar a cada momento una ventana que corre el juego entero es perder el tiempo si no se está tocando todo):
  - mientras se trabaja en una parte, se corre solo esa parte y sin ventana: `Tools/Jam/Smoke-Jam.ps1 -Only <Paso> -Offscreen` (por ejemplo `-Only Entity`, unos 20 s frente a los 95 s de la completa). Si la parte aún no tiene filtro, se añade su paso a `gb.Smoke <Prefijo>`;
  - mientras se itera, se añade `-FailFast`: la corrida se corta 3 s después del primer error de `LogGB` o del primer crash, en vez de seguir hasta el final o hasta el timeout (una corrida roja de CastleVisit o Enemies3DBase costaba de 160 a 290 s). No se usa en la verificación final, que debe listar todos los fallos;
  - mientras se itera, se compila solo el target del Editor;
  - el filtro baja hasta un sub-bloque, sin comillas (los espacios del nombre del paso se ignoran): `-Only Enemies3D:tactics` tarda 45 s frente a los 223 s del bloque. Una corrida filtrada termina 5 s después de su último paso (antes esperaba hasta la hora del final de la corrida completa);
  - **cierre de un bloque de trabajo: un solo comando en segundo plano**, `Tools/Jam/Verify-Jam.ps1` (suite, paquete y smoke completo del juego empaquetado; con `-Visits`, también FogVisit, CastleVisit y Enemies3DBase en sus mapas, solo si se tocaron esos mapas, el humo o la IA). Con `-Visits -Parallel`, el smoke completo corre al lado de las visitas (dos juegos a la vez). Medido el 15 nov sobre el mismo juego: 647 s frente a 1.098 s, pero el smoke completo sacó dos rojos (Valkyrie judgement, Castle) que la corrida secuencial no tuvo, porque sus pasos van por reloj real y la máquina iba cargada; las tres visitas pasaron igual. Por eso `-Parallel` solo vale como vistazo rápido: un verde es de fiar, un rojo del smoke completo se repite sin `-Parallel`, y el build de entrega se verifica siempre sin él. Imprime una línea por etapa y VERIFY GREEN o RED. No se repite el smoke completo sin cocinar (son los mismos 559 pasos que el empaquetado), ni `Build-Jam.ps1 -Target Glyphbound` (el paquete ya compila el juego), ni smokes filtrados que el completo ya cubre;
  - cambios solo en datos (`Content/Data/*.json`) no necesitan compilar;
  - todo encargo a un implementador (Muse, Sonnet u otro) incluye esta regla.
- Comandos largos en segundo plano, sin bucles de espera.

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
- Campo **"Tools you used"** del envío: listar todas las herramientas usadas (algunos premios de socios dependen de él: Hyper3D/Rodin y NVIDIA RTX Award). Llevar la lista al día en la bitácora.
- Si se compite por el NVIDIA RTX Award: el juego debe usar al menos una función de NvRTX, NVIDIA DLSS o NVIDIA ACE, y se responden las preguntas de NVIDIA al enviar.
