# Handoff between architect chats

> Kept by whichever Claude Code chat is the architect. Read `CLAUDE.md`, `Design/GDD.md` and `Design/ARCHITECTURE.md` first; this file only says where things stand.
> Last update: 2026-11-12 (day 1), by the first architect chat. **Status: integration in progress** (see "Right now").

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
- **Day entry:** `Devlog/days/2026-11-12.md` (architect's entry; has TODO marks to fill at the end of the day).

## How to run the usual things
```
powershell -File Tools/Jam/Build-Jam.ps1                      # Editor target; Editor must be closed
powershell -File Tools/Jam/Test-Jam.ps1 -MinTests 20          # whole suite (20 tests)
powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.QTE.'
powershell -File Tools/Jam/Package-Jam.ps1 -Project C:\GameJam\Glyphbound\Glyphbound.uproject -ZipName TeamTBD_Glyphbound
powershell -File Tools/Devlog/New-DevlogDay.ps1 -Date 2026-11-12
```
Muse Spark: `Tools/Devlog/Invoke-LoggedAgent.ps1 -Agent opencode -Model 'opencode-go/muse-spark-1.3-contributor#high' -PromptFile Prompts\X.md -Purpose "..." -Command "opencode run --auto -m 'opencode-go/muse-spark-1.3-contributor#high' (Get-Content Prompts\X.md -Raw)"`.
Codex: same wrapper with `-Shell cmd -Command 'codex exec -m gpt-6.1-sol -c model_reasoning_effort=medium -s read-only -C . - < Prompts\X.md'`.

## Right now
- TODO (filled at the end of the block).

## Next steps
- TODO.

## Things a human must check by playing (nobody has played the game yet)
- TODO.
