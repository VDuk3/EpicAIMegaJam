# Handoff: el aspecto del rifle de asalto (Glyphbound)

**Para:** un chat nuevo de Claude Code en `C:\GameJam` (máquina CENTRAL).
**De:** el chat que integró el rifle el 2026-11-12/13. El dueño cortó aquí porque el resultado no le sirve.
**Antes de nada:** lee `CLAUDE.md`, `Devlog/README.md` y, en `Devlog/days/2026-11-12.md`, el **decimosexto bloque**, que tiene la historia completa del rifle.

## El problema, en palabras del dueño
- "El arma se ve asquerosa".
- "Parece hecha de gelatina".
- Antes, sobre el encuadre: "sigue muy lejos", "no va alineada hacia la mira".
- Antes: "analiza las capturas de cerca: hay imperfecciones".

Las tres rondas de arreglos de este chat no le convencieron. **El objetivo es que el rifle en primera persona se vea sólido, nítido y de calidad comparable a la pistola**, que el dueño sí aprobó.

## Diagnóstico honesto (lo que este chat cree que pasa de fondo)
**La fuente es pobre para estar a 10-60 cm de la cámara:**
- Es un GLB de Tripo de 5.498 triángulos.
- Es una "sopa" de unos 1.000 paneles sueltos, con paneles duplicados espalda con espalda.
- Solo trae un albedo de 4096 con iluminación y manchas horneadas, y una máscara de neón.
- No trae mapa normal, ni de rugosidad, ni de AO.
- Las superficies grandes (culata, guardamanos) son lisas y redondeadas, sin detalle.
- La pistola, en cambio, tiene unos 52.900 triángulos, un atlas PBR completo y materiales por pieza.

**Lo que ya se intentó y su efecto:**
- Aristas duras por ángulo (30°) en la exportación, porque se perdían las normales propias del GLB. Mejoró algo.
- Rugosidad y metálico deducidos del brillo del albedo: plástico mate en lo claro, metal en lo oscuro.
- Autosombra en las piezas del arma (`bSelfShadowOnly`), para las dos armas.
- Texturas a 4096, sin streaming.
- Albedo × 0,8.
- Plano cercano a 3 cm.
- Material de dos caras: **revertido**, porque producía z-fighting y el arma se veía granulada.
- Encuadre: escala 0,62, raíz (-12, -6, 2) cm y yaw -0,4°. El arma queda paralela a la vista y el cañón converge en la mira.

**Conclusión:** seguir retocando parámetros no basta. Hace falta mejorar el **asset** o cambiar de enfoque.

## Opciones (elige con criterio; las decisiones técnicas las toma el arquitecto sin preguntar)
1. **Rehacer el aspecto en Blender, sobre la misma forma.** Es lo más seguro sin gastar créditos.
   - Unir y limpiar la sopa: quitar paneles duplicados y fusionar vértices por distancia.
   - Biselar las aristas duras.
   - Hornear con Cycles un AO, y si se puede curvatura o normales desde una copia biselada o subdividida.
   - Materiales por zona, al estilo de la pistola: polímero blanco mate, metal oscuro satinado, gomas negras.
   - En vez de confiar en el albedo de Tripo con sus manchas, se puede:
     - usar el albedo solo como máscara de zonas;
     - limpiarlo como se limpió el atlas de la pistola (`Tools/Blender/pistol_clean_atlas.py`).
2. **Regenerar el modelo** con más calidad: Tripo con más polígonos o con texturas PBR, o Meshy.
   - **Gasta créditos de pago:** pide permiso al dueño antes (regla del proyecto).
   - Mantén las piezas móviles con el mismo contrato (ver abajo) para no tocar el código.
3. **Pedir al equipo de ASSETS** (la otra máquina) que trabaje el rifle como trabajaron la pistola (Codex y Cursor en Blender). El dueño decide.
4. **Encuadre y lente:** valorar un FOV propio para el modelo del arma (un render del arma con otro FOV, lo que hacen muchos FPS) y una escala distinta. No es la causa principal, pero ayuda.

Pregunta al dueño **una sola vez** si quiere gastar créditos (opción 2) o que se trabaje sobre la forma actual (opción 1). Mientras responde, empieza por la 1.

## Contrato que el código ya espera (no lo rompas sin cambiar `AGBGun`)
**Assets en `/Game/Glyphbound/Weapons/Rifle/`:**
- Mallas `SM_Rifle_Body`, `SM_Rifle_Trigger`, `SM_Rifle_Magazine`, `SM_Rifle_WeakPointLight` y `SM_Rifle_AmmoDisplay`.
- Cañón hacia +X; origen del cuerpo en el punto de la mano (arriba de la empuñadura); unidades en cm; el cuerpo mide unos 100 cm.

**Parámetros de material que pone el juego:**
- `ElementColor` en todas las piezas.
- `ImbueRemaining` en el cuerpo. La franja de neón lateral se apaga en 13 celdas desde la boca, según el UV 1 del cuerpo: U de 0 a 1 a lo largo de la franja; V = 0 en la franja y V = 1 fuera, una vez en Unreal.
- `Flash` en la luz de punto débil (las dos lentes de la mira).
- `DisplayTexture` en el panel de munición, que usa `M_GunHolo`, compartido con la pistola.

**Desplazamientos y valores** en la tabla `RifleArt` de `Glyphbound/Source/Glyphbound/Weapons/GBGun.cpp`:
- gatillo (6,5, 0, 0,3);
- cargador en reposo (15,8, 0, -2,8) y fuera (2, 0, -17);
- punta del cañón (69, 0, 6,6);
- escala 0,62, raíz (-12, -6, 2), yaw -0,4.

Si cambias la geometría, actualiza esos números. También los escribe `SourceArt/Weapons/Rifle/export_report.json`.

## Archivos y flujo
**Fuente única:** `Glyphbound/SourceArt/Weapons/Rifle/Rifle.blend`.
- Fuera de git, igual que `Content/`; su README sí está en git.
- Se creó con `Tools/Blender/rifle_import_glb.py` a partir de `C:\Users\VNasher\Downloads\carabina-roja.zip` (`model.glb`).
- Regla del dueño: una sola copia de cada asset; nada de FBX ni PNG intermedios guardados.

**Exportación:** `Tools/Blender/rifle_export_fbx.py`.
- Separa las piezas por cajas de paneles.
- Modela el cargador, que la fuente no tenía.
- Crea el panel holográfico y el UV 1 de la franja.
- Marca las aristas duras.

**Importación:** `Tools/Jam/Import-Rifle.ps1`, con el Editor cerrado.
- Ejecuta la exportación y `Tools/Jam/import_rifle.py` (commandlet de Python).
- Crea `M_RifleAtlas`, `M_RifleIndicator` y las instancias del cargador.
- Reutiliza `M_GunSolid` y `M_GunHolo` de `/Game/Glyphbound/Weapons/Pistol`.

**Verificación**, sin PIE (lo hace un humano). Desde `C:\GameJam`:
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1`: solo si tocas C++.
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1`: debe terminar en `done=True crashed=False LogGB errors=0`.
- Capturas en `Glyphbound/Saved/Screenshots/Smoke/`: `14a_rifle_hip` (reposo), `14b` (recarga), `14c` (punto débil), `14d_rifle_half_bar` (encantado), `14e_rifle_fire` (fogonazo).
- Amplíalas con un recorte (System.Drawing en PowerShell) y míralas de cerca.
- **Ojo:** justo después de cambiar un material, `14a` sale beige y granulada (material provisional mientras compilan los shaders). Para juzgar, mira `14d`, o corre el smoke dos veces.
- Juzga también con renders de Blender desde un ángulo de primera persona, antes de importar. Es más rápido que el ciclo completo de unos 2 minutos.

**Empaquetado:** `Tools/Jam/Package-Jam.ps1 -Project C:\GameJam\Glyphbound\Glyphbound.uproject -ZipName TeamTBD_Glyphbound`.
- La ruta tiene que ser absoluta.
- Hoy pesa 256 MB.

## Reglas que aplican (resumen)
- Respuestas al dueño en español.
- Bitácora y commits en inglés: añade un bloque nuevo a la entrada del día con la decisión y el motivo.
- Commit y push a `origin main` en cada cambio; nunca force-push.
- El Editor se cierra solo con `Tools/Jam/Stop-EditorMcp.ps1`.
- Los agentes externos se lanzan solo con `Tools/Devlog/Invoke-LoggedAgent.ps1`.
- Para tareas de código en este frente, el dueño pidió **Sonnet**, no Muse Spark.
- Los assets de terceros, con licencia y anotados en la bitácora.
- No gastar créditos de Tripo, Meshy ni Hyper3D sin permiso.

## Estado al entregar
- Último commit de este frente: `1f16bd8`.
- Todo compila, el smoke pasa y el zip de 256 MB está hecho.
- La pistola no se toca: el dueño la aprobó.
