# Review: the weapon enchant's element chosen in its QTE — Glyphbound (UE 5.9)

You are reviewing, read-only. Run `git diff HEAD -- Glyphbound/Source Glyphbound/Content/Data` and read around it in
`QTE/GBQTELogic.h/.cpp`, `QTE/GBQTEComponent.h/.cpp`, `Spells/GBSpellbookComponent.h/.cpp`, `UI/GBHUD.cpp`
(`DrawQTE`, `DrawQTEElementChoice`), `Player/GBPlayerCharacter.cpp` (how QTE input reaches `HandleKey`, the camera lock
and the held movement during the QTE), `Tests/GBQTETests.cpp` (new `Glyphbound.QTE.SessionChoice`) and the changed UI
smoke steps in `Core/GBDevCommands.cpp`. Ignore Devlog and GDD files.

What changed (owner, Nov 13): the weapon enchant's QTE starts with a new `Choice` phase. The spellbook passes fixed
choice keys (A / Left = fire, W / Up = lightning, D / Right = ice) through `StartQTE`; a key of the list picks that
option and part 1 starts; other keys are ignored; the step has its own clock (`QTE.ChoiceTimeLimit`, 3 s) and, when it
runs out, nothing is picked and part 1 starts. `FGBQTEResult::Choice` carries the pick; `HandleQTEFinished` sets the
enchant element from it before the cast is finished. The HUD shows three element tiles with their keys while choosing,
and the timer shows the choice time.

## What to look for (ranked)
1. Logic: the `Choice` value added at the end of `EGBQTEPhase` (anything that compares phases with < or >, or treats
   "not Sequence and not Gesture" as finished or inactive; `Advance` returning early; `IsFinished`; `CancelQTE` during
   the choice; `GetResult` power / reach / success when the choice timed out or the QTE was cancelled in the choice).
2. Input during the choice: the same keys (W / A / D) are part 1's keys; can one press both pick and count as a
   sequence step? Gamepad face buttons map to the same enum? Gesture input during the choice? The held movement and
   camera lock during a longer QTE (up to 3 s more).
3. Gameplay: the time dilation now lasts up to 3 s longer; anything timed to the QTE's old maximum length (AI,
   enchant cooldown starting, smoke steps, the spellbook's busy state, weapon switch lock).
4. The element applied when the cast fizzles or is cancelled; the QTE off path (no choice, element unchanged).
5. The new test: can it fail for a wrong implementation?

Do not suggest style rewrites or new features. For each finding give: file:line, what goes wrong, a concrete
scenario, and a fix. If you find nothing real, say so plainly.
