# Task: integrate the changes of the owner's first play-test (build, tests, smoke)

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies).

The owner played the first build and asked for nine changes (GDD v0.14, see the changelog at the top of `Design/GDD.md`). Four implementers wrote them in parallel, each in its own files, **without compiling**. Their reports, with the new API, the things they could not verify and the requests between modules, are in `Prompts/feedback1_notes.md`: read it whole before the first build.

## What must be true when you finish
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded` (Editor target).
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded` (game target; catches Editor-only API).
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Make-Materials.ps1` creates `Glyphbound/Content/Glyphbound/Materials/M_PixelSurface.uasset` with no Python error (needs step 1 done).
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 22` passes (there should be more than 22 tests now; use the real count in your report).
5. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, and its `SMOKE` lines make sense (see "Read the smoke output").
One build or run at a time. Do not open the Unreal Editor yourself. Do not commit.

## How to fix
- The header comments are the specification; the notes file lists what each implementer added.
- Check engine APIs in the engine source (`C:\Program Files\Epic Games\UE_5.9\Engine\Source`, `...\Engine\Plugins`) before guessing.
- Smallest correct change. Never make an error disappear by deleting behavior (no emptied bodies, no commented-out features, no weakened or skipped tests). If something cannot work, leave the smallest stub that compiles and say so.
- No gameplay numbers in code: new tuning values go in the JSON under `Glyphbound/Content/Data` with a named key. Every key the notes list as "to add to Game.json" must be in `Game.json` with its default value.
- Keep every JSON file valid; in `Sprites.json` palette keys ignore case (never both 'a' and 'A' in one palette) and every row of a sprite has the same length.

## Read the smoke output
Besides the summary line, check: the level has enemies at the start and the player stays on the floor; casts succeed; the pistol is in the hands after the "take the pistol" step; the era is Future after the travel; the enemy pathing check added by the AI implementer reports that the orc reached the player (if it logs an error, the pathing does not work: fix the pathing, do not relax the check); after the respawn the player is alive and still owns its weapons and spells.
If the smoke script itself needs a small update because an API changed (it is `Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp`), update it; add one step that exercises the new rule "a second staff goes to the inventory" (give `Staff_Ember`, log that the staff in the slot is still `Staff_Apprentice` and that `Staff_Ember` is owned, then `EquipOwnedWeapon` and log again) and one that summons the ice shield with the QTE off, raises it with `SetBlockHeld(true)`, logs `HasShield` / `IsBlockRaised`, takes a screenshot named `9_shield`, and lowers it. Keep at least one second between a screenshot step and the next step that changes the screen.

## Final report (plain text)
1. The five result lines, copied verbatim.
2. What you fixed, grouped by kind, with file names.
3. Anything stubbed or not working.
4. Things a person should check by playing.
