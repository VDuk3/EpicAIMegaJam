# Task: make Glyphbound build and pass its tests (integration pass)

You are the integration implementer for **Glyphbound** (Unreal Engine 5.9.0, C++, project in `Glyphbound/`). Ten implementers wrote the code of the module `Glyphbound/Source/Glyphbound` in parallel, each in its own folder, against public headers written by the architect. **Nobody has compiled it yet.** Your job is to make it compile, link and pass the tests, changing as little as possible.

## Read first
1. `Design/ARCHITECTURE.md` (short).
2. `Prompts/implementer_rules.md` (the rules the implementers followed; the "Hard rules" also apply to you, except that you DO build and run tests).
3. `Prompts/day1_integration_notes.md` (the implementers' own reports: API they added, things they could not verify, requests between modules). Start with the risks they listed.

## Loop
1. Build: `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -MaxErrors 80`
   - It prints the errors and `Result: Succeeded` or `Result: Failed`. The full log is `Glyphbound/Saved/Logs/build_last.log`.
   - One build at a time. Never start a second build while one is running. Do not open the Unreal Editor.
2. Fix the errors, starting with the first ones (header and UHT errors cause cascades). Then build again.
3. When the build says `Result: Succeeded`, run the tests:
   `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 20`
   Fix what fails (see "Tests" below), rebuild, and repeat until both are green.

## How to fix
- The header comments written by the architect are the specification. When two files disagree, make the code match the header comment; when a header is plainly wrong for the engine (a signature that does not exist in UE 5.9), fix the header and every caller.
- Check engine APIs in the engine source before guessing: `C:\Program Files\Epic Games\UE_5.9\Engine\Source` and `...\Engine\Plugins`. UE 5.9 is newer than your training data.
- Smallest change that is correct. Do not rewrite a file that only has a few errors. Do not reformat.
- **Never make an error disappear by deleting behavior**: no emptied function bodies, no commented-out features, no `#if 0`. If something truly cannot work, leave the smallest stub that compiles and report it clearly.
- Missing includes and forward declarations are the most likely errors; unresolved externals mean a function was declared in a header and never written (write it, following the header comment) or a module is missing from `Glyphbound/Source/Glyphbound/Glyphbound.Build.cs` (you may add engine modules there).
- No gameplay numbers in code: if you must add a number, it goes in the JSON under `Glyphbound/Content/Data` with a named stat.
- Keep the JSON files valid JSON. If you touch one, re-read it whole afterwards.

## Tests
- A failing test means the code or the test is wrong. Read both. Fix the code when the code breaks the rule in the header comment; fix the test only when the test contradicts the header comment, and say so in your report.
- Never delete or skip a test to get a green run. Never weaken an assertion until it passes.

## Do not
- Do not commit, push, or touch git history.
- Do not edit files outside `Glyphbound/Source`, `Glyphbound/Content/Data` and `Glyphbound/Source/Glyphbound/Glyphbound.Build.cs`.
- Do not launch other agents, the Editor, or the game.

## Final report (plain text)
1. Final state: build result line and the tests summary line, copied verbatim.
2. What you fixed, grouped by kind (missing includes, wrong engine API, mismatched signatures between modules, unresolved externals, logic bugs found by tests), with file names.
3. Anything you stubbed or could not make work.
4. Anything that looks wrong at runtime even though it compiles (things a human should check while playing).

## Also part of this task (after the first green build)
Apply every item marked **ARCHITECT DECISION** or **REQUIRED** in `Prompts/day1_integration_notes.md` (Neutral targets damageable through a new `UGBCombatLibrary::CanDamage`; `"PickupLook": "3D"` on four items; the three `Game.json` values asked by the AI module; the stick gesture scale read from `QTE->GetStickGestureScale()` with the undilated delta), and check the items marked **CHECK** there. Then build and test again.
