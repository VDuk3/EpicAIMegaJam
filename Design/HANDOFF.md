# Handoff between architect chats

> Kept by whichever Claude Code chat is the architect. Read `CLAUDE.md`, `Design/GDD.md` and `Design/ARCHITECTURE.md` first; this file only says where things stand.
> Last update: 2026-11-12 (day 1), by the first architect chat.

## Standing instructions from the owner (2026-11-12)
- Do not stop to ask for go-aheads or yes/no questions. Decide, do, log. The owner is away for hours at a time.
- The codebase is the architect's: every code decision is yours, and so is its quality.
- Everything modular, scalable and with editable parameters, combat above all. Spells must be modular with every parameter adjustable.
- Looks do not matter yet; structure and function first. "A first-person RPG shooter that does not want to be too complicated."
- Work routing: one code batch at a time -> Muse Spark through OpenCode (only one OpenCode session at once). Work that can really run in parallel -> Claude subagents (Opus / Sonnet / Haiku, never above effort High). Second opinions and important reviews -> Codex CLI, Sol 6.1 at Medium.
- PIE is always run by a human. The content backup task stays off for now.

## What exists
- **Project:** `Glyphbound/Glyphbound.uproject`, UE 5.9.0, one C++ module, no Blueprint or binary asset needed to run. Default map = engine `Entry` map; `AGBGameMode` builds `AGBTestArena` in code when the map has no PlayerStart.
- **Code:** every system of GDD 16 has a first implementation (about 17,500 lines, 103 files). Layout and rules: `Design/ARCHITECTURE.md`.
- **Data:** `Glyphbound/Content/Data/*.json` (11 files). All gameplay numbers are there. `gb.ReloadData` reloads in game; `gb.QTE 0` turns the QTE off.
- **Scripts (all in `Tools/Jam/`):** `Build-Jam.ps1` (compile), `Test-Jam.ps1` (pure-logic tests, anchored filter), `Measure-EditorBoot.ps1`, `trim_plugins.js` (regenerates the plugin list in the .uproject), plus the kit's `Start-EditorMcp.ps1`, `Stop-EditorMcp.ps1`, `Package-Jam.ps1`.
- **Prompts given to agents:** `Prompts/` (implementer rules, integration task and notes, Codex review).
- **Day entry:** `Devlog/days/2026-11-12.md` (architect's entry; a person adds the play-test notes). Smoke screenshots: `Devlog/media/2026-11-12/`.

## How to run the usual things
```
powershell -File Tools/Jam/Build-Jam.ps1                      # Editor target; Editor must be closed
powershell -File Tools/Jam/Test-Jam.ps1 -MinTests 22          # whole suite (22 tests)
powershell -File Tools/Jam/Smoke-Jam.ps1                      # scripted run of the game (add -Packaged for the packaged exe)
powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.QTE.'
powershell -File Tools/Jam/Package-Jam.ps1 -Project C:\GameJam\Glyphbound\Glyphbound.uproject -ZipName TeamTBD_Glyphbound
powershell -File Tools/Devlog/New-DevlogDay.ps1 -Date 2026-11-12
```
Muse Spark: `Tools/Devlog/Invoke-LoggedAgent.ps1 -Agent opencode -Model 'opencode-go/muse-spark-1.3-contributor#high' -PromptFile Prompts\X.md -Purpose "..." -Command "opencode run --auto -m 'opencode-go/muse-spark-1.3-contributor#high' (Get-Content Prompts\X.md -Raw)"`.
Codex: same wrapper with `-Shell cmd -Command 'codex exec -m gpt-6.1-sol -c model_reasoning_effort=medium -s read-only -C . - < Prompts\X.md'`.

## Right now (end of the first block, 2026-11-12)
- Editor build and game build green; 22 pure-logic tests pass; the scripted smoke run (`Tools/Jam/Smoke-Jam.ps1`) ends with no errors.
- Codex reviewed the code once (9 findings, all fixed). The fixes were verified by build, tests and smoke only.
- **No human has played yet.** That is the next thing that must happen.
- Packaging result: see "Numbers of the day" in `Devlog/days/2026-11-12.md`.

## What the game does today
Start in the past with the Apprentice staff, 3 spells in the loadout and double jump. The arena has: a movement course (double jump steps, a dash gap), a central portal (G to travel; past and future look different), a pistol and consumables near the start, four spell books (all 13 spells), a chest (ember staff, sword hilt, gems), two training dummies, a past wave encounter and a future one, an era-gated room (shotgun; closed in the past, open in the future) and a future-only ramp that only exists once the past wave is cleared (rifle and armor chest).
Controls are GDD 9. Menus: Tab inventory (Q/E pages), K skill trees, Esc pause. Console: `gb.ReloadData`, `gb.QTE 0` / `gb.QTE 1`, `gb.Smoke`.

## Next steps (architect's order; the owner's play-test notes come first)
1. Read the owner's play-test notes and fix what blocks playing.
2. Gamepad pass (bindings exist, never tried).
3. Enemy behavior with a real navmesh once there is a real level (today they steer straight at the player).
4. Second review round with Codex on the fixes of this block (fixes 2, 4, 5, 7 touch collision and AI and are only code-read).
5. DLSS plugin for UE 5.9.0 (NVIDIA award): not installed in the engine; needs a download, so ask the owner first.
6. GDD day 2 list: QTE second version, enchant polish, both HUD skins with real art, consumables in play.
7. Real levels: the era system already supports sub-levels named `*_Past` / `*_Future` and per-actor `UGBEraComponent`.

## Things a human must check by playing (nobody has)
- Movement feel: speed, jump height, double jump, dash distance and whether the dash should have a brief invulnerability.
- Mouse look speed, and the QTE gesture sensitivity (the mouse-to-pixels conversion assumes the legacy 0.07 scale; if gestures are judged far too early or too late, the constant is in `Player/GBPlayerCharacter.cpp`).
- The QTE itself: is 3 s enough for 4 keys and 1 gesture; do gestures get recognized.
- Shooting: do hitscan shots hit enemies, do headshots register on sprites, recoil feel, reload.
- Ice wall: blocks enemies and shots, and wears down when shot.
- Era travel: fade, restore, the era-gated room and the unlock ramp.
- Death and respawn in the middle of an encounter.
- Size of the pistol placeholder in view (it looks big in the screenshots).
- Sprites that face the camera: sizes of enemies, pickups and spell effects.

## Known limits (accepted for now)
- Enemies do not attack spell walls on purpose.
- A dropped gun comes back fully loaded when picked up again.
- Spell cooldowns persist through death.
- `gb.*` console commands do not exist in Shipping builds (we package Development).
- The warm Editor boot did not get faster with fewer plugins (about 10-11 s either way).
