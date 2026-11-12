# Handoff between architect chats

> Kept by whichever Claude Code chat is the architect. Read `CLAUDE.md`, `Design/GDD.md` (v0.15) and `Design/ARCHITECTURE.md` first; this file says where things stand and how the work is run.
> Last update: 2026-11-12 (day 1, after the owner's second round of play-test notes), by the first architect chat (Claude Fable 5.2). The next architect is a Claude Opus chat that continues the owner's tweaks.

## Standing instructions from the owner (2026-11-12)
- Do not stop to ask for go-aheads or yes/no questions. Decide, do, log. If the architect stops, the project stops.
- The codebase is the architect's: every code decision is yours, and so is its quality. Nobody on the team is a programmer.
- Everything modular, scalable and with editable parameters, combat above all. Spells must be modular with every parameter adjustable.
- Looks do not matter yet; structure and function first. "A first-person RPG shooter that does not want to be too complicated."
- Work routing: one code batch at a time -> Muse Spark through OpenCode (only one OpenCode session at once). Work that can really run in parallel -> Claude subagents (Opus / Sonnet / Haiku, never above effort High). Second opinions and important reviews -> Codex CLI, Sol 6.1 at Medium.
- PIE is always run by a human. The content backup task stays off for now.
- Always commit (English message). Never push unless asked. Answers to the team in Spanish; game content, dev log and commits in English.

## How the owner works (what to expect)
- He plays the packaged build and sends short notes in Spanish, several in one message, sometimes one more while the work is running. Each note is a design decision: write it into the GDD first (changelog line, the section it belongs to, and section 18 for any detail you had to invent), then implement.
- When a note leaves a detail open, pick a reasonable value, mark it provisional in the GDD, put it in data, and list it in the final answer. Do not ask.
- He unzips `Glyphbound/TeamTBD_Glyphbound.zip` next to it to play (`Glyphbound/TeamTBD_Glyphbound/`, git-ignored). After every block: package again and tell him to unzip again.
- The final answer of each block, in Spanish: what changed per note, the provisional choices, what he should check by playing, what was not verified.

## The loop for a round of tweaks (what worked three times today)
1. GDD: record the notes (bump the version, changelog line). Commit.
2. Read the code involved yourself before writing the brief (the brief must name files, functions and the exact rule).
3. Brief in `Prompts/<block>.md`: the rule per item, new data keys with their values, pure functions to add (so they can be tested), the verification list (both builds, tests with `-MinTests`, smoke), "do not commit", the final report format. Model briefs: `Prompts/feedback2_jump_melee.md` (small serial batch), `Prompts/feedback1_integration.md` + `feedback1_notes.md` (integration after parallel implementers), `Prompts/feedback1_review_codex.md` (review), `Prompts/feedback1_fixes_review.md` (fix batch). `Prompts/implementer_rules.md` is shared by all.
4. Small serial batch -> Muse Spark. Several independent areas -> write the public header declarations yourself, then one Claude subagent per area (each owns its files, none builds), collect their reports into a notes file, then ONE Muse Spark integration pass that builds, tests and runs the smoke script.
5. Look at the result yourself: the report's verbatim result lines, the diff of the risky parts, the smoke screenshots (`Glyphbound/Saved/Screenshots/Smoke`).
6. A block that touches several systems gets a Codex read-only review, then a Muse fix batch with your decision per finding. A small block does not need it.
7. Commit. Package. Run the smoke script on the packaged exe (`-Packaged`). Update the day entry, this file, re-run `New-DevlogDay.ps1`. Commit.

## Rules learned the hard way (all happened today)
- One build or run at a time on this machine. Long commands in the background.
- While an agent is editing, `git add` explicit paths only (a "commit everything" swept half-edited files once). Do not edit a file an agent is working on; data edits wait until it ends.
- Always build the game target too (`-Target Glyphbound`): Editor-only API compiles in the Editor and breaks packaging.
- The smoke script (`gb.Smoke`, in `Core/GBDevCommands.cpp`) is the only automated check that the game actually runs: it caught "no floor under the player" and magenta sprites that compiler, tests and reviewers all missed. When a feature changes what the script does or its timing, update the script in the same batch. Screenshots are captured a few frames late: leave a second after each one, and never ask for a screenshot on the frame an animation starts.
- Unreal string and JSON map keys ignore case: never two keys that differ only by case (sprite palettes use digits for that reason).
- The Cascade plugin must stay enabled (the cook fails without it). Plugin list is generated by `Tools/Jam/trim_plugins.js`.
- One-off scripts go in a file (scratchpad), never inline `node -e` with backticks inside bash; literal replacements with `split(a).join(b)`.
- A class member must never be named `Cast`. Files of the module are compiled together (unity build): file-local helper names must be unique across files.
- Tests: pure logic only, each must be able to fail, names `Glyphbound.<Area>.<Name>` where no name is a prefix of another.

## What exists
- **Project:** `Glyphbound/Glyphbound.uproject`, UE 5.9.0, one C++ module (`Glyphbound`, prefix `GB`), no Blueprints. Default map = engine `Entry` map; `AGBGameMode` builds `AGBTestArena` in code when the map has none.
- **Code:** 113 files, about 21,500 lines, a first implementation of every system of GDD 16. Layout, principles and conventions: `Design/ARCHITECTURE.md`.
- **Data:** `Glyphbound/Content/Data/*.json` (11 files, loose files next to the packaged exe). All gameplay numbers are there. `gb.ReloadData` reloads in game.
- **Scripts (all in `Tools/Jam/`):** `Build-Jam.ps1`, `Test-Jam.ps1`, `Smoke-Jam.ps1`, `Make-Materials.ps1`, `Measure-EditorBoot.ps1`, `trim_plugins.js`, plus the kit's `Start-EditorMcp.ps1`, `Stop-EditorMcp.ps1`, `Package-Jam.ps1`.
- **The one binary asset:** `Glyphbound/Content/Glyphbound/Materials/M_PixelSurface.uasset` (world-space pixel tiles for 3D structures). NOT in git: generated by `Tools/Jam/Make-Materials.ps1` from `make_materials.py`. It exists on this machine (CENTRAL). **Run the script after a fresh clone and before packaging on another machine** (Editor closed, Editor target built). Without it structures fall back to plain shapes.
- **Day entry:** `Devlog/days/2026-11-12.md` (three blocks; a person adds the human side). Screenshots: `Devlog/media/2026-11-12/`.

## Where to tweak what (most of the owner's notes are one of these)
| The owner says | Look here first |
|---|---|
| Movement, jumps, dash, health, mana, starting gear | `Player.json`; `Player/GBPlayerCharacter.cpp` (`ApplyMovementStats`, `OnJumped_Implementation`, `InputDash`) |
| A weapon's numbers (damage, rate, recoil, head bonus, melee range) | `Weapons.json`; `Weapons/GBGun.cpp`, `GBStaff.cpp`, `GBSword.cpp` |
| A spell's numbers, QTE scaling, look | `Spells.json` (each param has `Base / Min / Max / ScaledBy`); behavior classes `Spells/GBSpell_*.cpp` (the header of each lists its params) |
| QTE time, keys, gestures | `QTE.json`; `QTE/GBQTELogic.cpp`, `QTE/GBQTEComponent.cpp` |
| Enemies: health, damage, ranges, speed, drops | `Enemies.json`, `Encounters.json`; movement tuning (`Enemy*` keys) in `Game.json`; `AI/GBEnemyCharacter.cpp` |
| Perks | `Skills.json` (modifier keys: `Weapon.<Id|Gun|Staff|Sword|All>.<Stat>`, `Spell.<Id|All>.<Param>`, player stats by name) |
| Burn, chill, shock, slows | `StatusEffects.json` |
| Items, armor, recipes | `Items.json`; `Items/GBInventoryComponent.cpp` |
| HUD, menus, damage numbers | `UI/GBHUD*.cpp`; damage-number tuning in `Game.json` |
| Kill height, corpse time, global values | `Game.json` |
| Placeholder sprites and tiles | `Sprites.json` (rows of text) |
| Keys and buttons | `Player/GBInputSet.cpp` (GDD 9 is the list) |
| The test arena | `World/GBTestArena.cpp` |
| Damage rules (who can hurt whom, head bonus, explosions through walls) | `Combat/GBCombatLibrary.cpp` (one damage path: `DealDamage`; one weak-point rule: `ApplyWeakPointBonus`) |

## How to run the usual things
```
powershell -File Tools/Jam/Build-Jam.ps1                      # Editor target; Editor must be closed
powershell -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound   # game target: catches Editor-only API before packaging
powershell -File Tools/Jam/Make-Materials.ps1                 # only after a fresh clone or when make_materials.py changes
powershell -File Tools/Jam/Test-Jam.ps1 -MinTests 26          # whole suite (26 tests)
powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.QTE.'
powershell -File Tools/Jam/Smoke-Jam.ps1                      # scripted run of the game (add -Packaged for the packaged exe)
powershell -File Tools/Jam/Package-Jam.ps1 -Project C:\GameJam\Glyphbound\Glyphbound.uproject -ZipName TeamTBD_Glyphbound
powershell -File Tools/Devlog/New-DevlogDay.ps1 -Date 2026-11-12
```
Muse Spark (from `C:\GameJam`, in the background):
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Devlog\Invoke-LoggedAgent.ps1 -Agent opencode -Model 'opencode-go/muse-spark-1.3-contributor#high' -PromptFile Prompts\X.md -Purpose '...' -Command "opencode run --auto -m 'opencode-go/muse-spark-1.3-contributor#high' --title '...' (Get-Content Prompts\X.md -Raw)"`
Codex (read-only review):
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Devlog\Invoke-LoggedAgent.ps1 -Agent codex -Model 'gpt-6.1-sol (medium)' -PromptFile Prompts\X.md -Purpose '...' -Shell cmd -Command 'codex exec -m gpt-6.1-sol -c model_reasoning_effort=medium -s read-only -C . - < Prompts\X.md'`
A data-only change (JSON) needs no build: run the tests and the smoke script, then package.

## Right now (end of the third block, 2026-11-12)
- Everything is committed on `main` (last commits: `98dc7fd` play-test 2 code, `311d7ec` third package and docs). Nothing was pushed.
- Editor build and game build green; 26 pure-logic tests pass; the smoke run (31 steps) ends with no errors, also on the packaged exe. Package: `Glyphbound/TeamTBD_Glyphbound.zip`, 245.3 MB.
- Three blocks done today:
  1. The whole code base (ten parallel implementers, integration, Codex review with 9 findings fixed).
  2. The owner's first ten play-test notes, GDD v0.14 (pixel-textured 3D structures, fireball shockwave, higher jumps, owned weapons and the Weapons page, held ice shield on Block, enemy pathing, mouse in menus, kill height, damage numbers, QTE 4 s); Codex review with 4 findings fixed.
  3. The owner's second round, GDD v0.15: jumps commit to their direction (`AGBPlayerCharacter::ComputeJumpHorizontalVelocity`; stats `AirControl` 0, `JumpTakeoffMinSpeedMult` 0.6, `AirJumpMinSpeedMult` 1.0), the sword hits where the player aims (`UGBCombatLibrary::ClosestBodyPointToAim`), one weak-point rule for gun, staff and sword (`ApplyWeakPointBonus`; `HeadshotMult` pistol 2, everything else 1.5, raised by the Marksman perk through `Weapon.All.HeadshotMult`). **Not reviewed by Codex. Not played yet.**
- The owner has played builds 1 and 2. Build 3 (the current zip) is waiting for him.
- Provisional choices waiting for the owner's word (GDD 18 and section 6): Block on Q / L2; guns and the sword follow the staff rule (owned, equipped from the inventory); a "big" damage number is a headshot or a hit of at least 25% of the target's health; staff / sword / rifle head bonus numbers; a standing jump with a direction held leaves at 60% of walking speed.

## What the game does today
Start in the past with the Apprentice staff, 3 spells in the loadout and double jump. The arena has: a movement course (double jump steps, a dash gap), a central portal (G to travel; past = pixel-textured stone, future = plain 3D with neon), a pistol and consumables near the start, four spell books (all 13 spells), a chest (ember staff, sword hilt, gems), two training dummies, cover (low walls, pillars), a past wave encounter and a future one, an era-gated room (shotgun; closed in the past, open in the future) and a future-only ramp that only exists once the past wave is cleared (rifle and armor chest).
Controls are GDD 9. Menus: Tab inventory (pages Items / Weapons / Spells / Skills; Q/E or click a tab), K skill trees, Esc pause; the mouse works in every menu. Block (hold Q, or L2 on a gamepad) raises a summoned ice shield. Console: `gb.ReloadData`, `gb.QTE 0` / `gb.QTE 1`, `gb.Smoke`.
Systems added after the first play-test: owned weapons (a pickup goes to the hands only if its slot is empty), `GBPlaceholder::SetupTexturedShape` for structures, `AGBExplosionEffect` shockwave with knockback, navmesh built at runtime (`AGBTestArena::BuildNavigation`) with local steering as fallback (`AI/GBSteering`), `KillZ`, damage numbers from the global event `UGBCombatLibrary::OnDamageDealt()`.

## Next steps (architect's order; the owner's notes always come first)
1. The owner's next notes on build 3 (he said he will keep sending tweaks).
2. Likely tuning after he plays: the jump course of the arena was computed assuming air steering was possible (it only uses jump height, run speed and dash distance, so it should hold, but nobody has tried it); the standing-jump speed.
3. Gamepad pass (bindings exist, never tried; L2 is both Block and the spell modifier).
4. DLSS plugin for UE 5.9.0 (NVIDIA award): not installed in the engine; needs a download, so ask the owner first.
5. GDD day 2 list: QTE second version, enchant polish, both HUD skins with real art, consumables in play.
6. Real levels: the era system supports sub-levels named `*_Past` / `*_Future` and per-actor `UGBEraComponent`; a placed navmesh works as is (`RuntimeGeneration=Dynamic` is the project default, can be Static per map).
7. End of day (if not done again after the last block): package, `New-DevlogDay.ps1`, commit. On Nov 13 start a new day entry `Devlog/days/2026-11-13.md` from `Devlog/TEMPLATE.md` if there is one, or the shape of today's entry.

## Things a human must check by playing
- Build 3: jumps (standing jump with a direction held, running jump, changing direction with the second jump, no steering in the air; is the jump course still doable?), sword hits on head vs legs, staff basic attack on the head (bigger number, x1.5).
- From build 2, not confirmed by the owner one by one (he played it and only sent the three notes of block 3, so assume it mostly works): clicks in menus while paused, one click on the death screen respawns once, Block only with staff or sword and slower movement, a second staff on the Weapons page, enemies around cover in both eras, falling kills, damage numbers, fireball shockwave.
- Never tried by anyone: gamepad.
- Movement feel, mouse look speed, QTE gesture sensitivity (the mouse-to-pixels conversion assumes the legacy 0.07 scale; the constant is in `Player/GBPlayerCharacter.cpp`).
- Size of the pistol placeholder in view; sizes of sprites that face the camera.

## Known limits (accepted for now)
- Enemies do not attack spell walls on purpose.
- Pixel tiles on a wall placed diagonally stretch up to about 1.4x (world-aligned projection picks one axis).
- Enemies never drop down a ledge higher than `EnemySteerLedgeDrop` (150 cm) while steering locally; on the navmesh they follow the mesh.
- Spell projectiles and explosions get no head bonus (a spell can opt in with a `HeadshotMult` param).
- A dropped gun comes back fully loaded when picked up again. Spell cooldowns persist through death.
- `gb.*` console commands do not exist in Shipping builds (we package Development).
- The warm Editor boot did not get faster with fewer plugins (about 10-11 s either way).
- The Unreal MCP server is only reachable while the Editor is open (`Start-EditorMcp.ps1`); nothing in the current workflow needs it.
