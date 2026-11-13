# Block: the weapon enchant's element is chosen in its QTE, on fixed keys (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's request
> *"Antes de probarlo quisiera que cuando se vaya a encantar el arma, el jugador pudiera escoger el elemento en el QTE. Que los elementos siempre salgan en la misma posicion, no aleatorios cada vez."*

Translation: "Before testing it, I would like the player to choose the element in the QTE when enchanting the weapon. The elements should always appear in the same position, not random each time."

Until now, the enchant's QTE was the same as every spell's: four random W A S D keys, then a gesture. The element was set beforehand on the inventory's Spells page.

## What was built
- **QTE logic (`QTE/GBQTELogic.*`, pure, unit-tested).**
  - A new optional `Choice` phase before part 1, with one key per option. The keys always come in the same order, never shuffled.
  - A key on the list picks that option and part 1 starts. Any other key is ignored and the step waits.
  - The step has its own clock (`QTE.ChoiceTimeLimit` in `QTE.json`, 3 s, provisional), so choosing never eats the QTE's own time. When the clock runs out, nothing is picked.
  - `FGBQTEResult::Choice` carries the pick.
- **Spellbook (`Spells/GBSpellbookComponent.*`).**
  - The enchant passes its fixed options: **A / Square = fire, W / Triangle = lightning, D / Circle = ice**.
  - When the QTE ends, the picked element becomes the enchant element, for this cast and the next ones.
  - With nothing picked (time out, or the QTE switched off), the element stays as it was. The inventory picker stays for that case.
- **HUD (`UI/GBHUD.cpp`, `DrawQTEElementChoice`).**
  - Three tiles in the element colours, each with its key and name. W sits a little higher, as on the keyboard, and the current element is marked "CURRENT".
  - The timer bar shows the choice time, then the QTE's own.
  - There is an RPG version too, though the enchant is only cast with a gun in hand, so it shows in the cyberpunk look.

## Checks
- **New test `Glyphbound.QTE.SessionChoice`:**
  - Non-option keys are ignored.
  - The pick is index 1 for W.
  - The choice consumes no part-1 step and no QTE time.
  - Power counts part 1 only.
  - A timeout picks nothing, and a late option key then counts as part 1 input.
  - Without options there is no choice step.
- **UI smoke:** the enchant's QTE must open on the choice. A screenshot catches it; D is pressed, then one key of part 1; at the end the enchant element must be ice. It was: `current=Fire` before, `Ice` after.
- **Review:** GPT Sol 6.1 (Codex CLI, medium, read-only, logged through `Invoke-LoggedAgent.ps1`; prompt `Prompts/enchant_element_choice_review_sol.md`) found **no defects**:
  - Phases are compared by equality.
  - One press picks without counting as a step.
  - Gamepad face buttons reach the same keys.
  - Camera lock, held movement, busy state and switch lock follow the longer QTE.
  - The choice survives a fizzle; a cancel or timeout keeps the old element.
- **Final checks:** tests 45 run, 0 failed; Editor and game builds.
- **Full smoke:** `done=True`, 65 screenshots, but **one error outside this change**: `SMOKE entity sync FAILED: 0.47 (expected over 0.6)` (the Entity's tentacles, a 1-simulated-second measure).
  - The value has been unstable all day across smoke runs: 0.47 to 0.85, with 5 failures counting this one.
  - The Entity filter run twice right after gave 0.81 both times.
  - It is a flaky check in the Entity block, reported to the owner as its own follow-up. It is not hidden or excluded.
- **Not packaged in this block.**
- **Implementer:** the architect (a small pure-logic phase with its test, and HUD drawing judged from screenshots).

## Screenshots (`media/2026-11-13/`)
- `enchantchoice_qte_1080.png`: the enchant's QTE opening on the element choice, fire marked current.
- `enchantchoice_then_sequence_1080.png`: after D (ice), part 1's keys with the first one done.

## Package
Owner: "Enviame el empaquetado para probar los cambios" ("Send me the package to test the changes"). Made from `a583786` with a clean tree while holding the machine lock: `TeamTBD_Glyphbound.zip`, **299.7 MB**, BuildCookRun 94 s.

**Full smoke on the packaged game:** `done=True crashed=False LogGB errors=0`, no LogGB warnings, 65 screenshots.
- The packaged data has the 30 skill nodes.
- The enchant's QTE opens on the element choice, and D gives ice.
- The skill point dialog spends nothing on Esc and exactly one point on confirm.
- The Entity's tentacle sync is 0.84 this time.

The zip was sent to the owner to play.
