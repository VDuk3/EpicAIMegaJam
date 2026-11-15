# Handoff between architect chats

> Kept by whichever Claude Code chat is the architect. Read `CLAUDE.md`, `Design/GDD.md` (**v0.68**) and `Design/ARCHITECTURE.md` first; this file says where things stand and how the work is run.
> Last update: **2026-11-15** (final day; deadline 20:59 EST). Rewritten from the GDD changelog, `Design/ARCHITECTURE.md`, the block files `Devlog/days/2026-11-13_*_block.md` and `2026-11-14_*_block.md` and the git log up to `f4dc179`. The day-1 version of this file is in git history.

## Standing instructions from the owner
From day 1 (2026-11-12), still valid:
- Do not stop to ask for go-aheads or yes/no questions. Decide, do, log. If the architect stops, the project stops.
- The codebase is the architect's: every code decision is yours, and so is its quality. Nobody on the team is a programmer.
- Everything modular, scalable and with editable parameters, combat above all. Spells must be modular with every parameter adjustable.
- PIE is always run by a human. Agents build, test, run the smoke script and package, then ask a person to play.
- Always commit (English message) and **push every commit to `origin main` right away, without asking** (owner, 2026-11-12: the agents on the other machines must always see the repo). Never force-push. Answers to the team in Spanish; game content, dev log and commits in English.

Added on later days:
- **Implementer routing.** Default implementer: Muse Spark 1.3 Contributor at High through OpenCode, launched only with `Tools/Devlog/Invoke-LoggedAgent.ps1` (one OpenCode session at a time). Exceptions are the architect's call and are logged with the reason: small changes made directly by the architect (example: `AGBTorch`, about forty lines, `f4dc179`), or a Claude Sonnet subagent after a Muse stall or for work that needs many build-and-smoke rounds (the site code needs in `b0102db`, the future enemies in `9bcd8a9`, the squad tactics of 2026-11-14). The owner allowed the Sonnet exception and prefers it after any Muse stall; render bugs are diagnosed by the architect.
- **Code review: Sol 6.1 at medium, read-only, through Codex CLI, launched only through the logged wrapper with `-Shell cmd`** (the command uses `<`). Prompts go in `Prompts/review_*_sol.md` (latest: `Prompts/review_castle_torches_sol.md`). Every block that touches code gets one; record its findings and your decision per finding in the block file.
- **Second opinion before compiling a plan:** GPT-6-Astra through Codex CLI, same logged wrapper (used for the GBPixelVfx plan, `8304005`).
- **Iterate cheaply** (owner, 2026-11-13, `bf321f9`; in `CLAUDE.md`): while working on one part, build only the Editor target and run only that part's smoke step, without a window: `Smoke-Jam.ps1 -Only <Step> -Offscreen` (about 20 to 65 s against 350 s for the full run). If the part has no step, add one to `gb.Smoke`. The full `Smoke-Jam.ps1` and `Build-Jam.ps1 -Target Glyphbound` run **once at the end**, before the commit or the package. **Every implementer brief carries this rule.**
- **Several chats on CENTRAL at once: the shared machine lock.** Before any build, test run, smoke run, package or Editor session, take `Glyphbound/Saved/machine.lock` (one line: who and what), wait if another chat holds it, and delete your line when done. Scripts check the file; chat messages are not enough (`Devlog/days/2026-11-13_pixelvfx_block.md`). Commit only your own hunks: other chats leave uncommitted work in the same checkout (right now: torch light edits in `World/GBTorch.*`, `GBPixelVfxComponent.*`, `Build-PastCastle.py`, `castle_twins.js`).
- **Art is shown before it is imported.** Close-up renders of every new or changed art asset (and of every zone of a map you changed) go to the owner first; import or place only after his OK ("yes, import"). Before sending them, look at each render yourself, list what is wrong and fix it (examples: the orcs' armor refit, four rounds on renders, `70a803d` to `cf8c3b9`; the castle's pixel twins, `82a0352`).
- **Pixel art = plain blocks** (owner, 2026-11-14, `04fdf77`): smooth rectangles, no imperfections on faces, corners or joints, "clean the imperfections without mercy". **Pixel art must also be light** on storage, RAM and GPU (GDD v0.67): simple geometry and small pictures; a heavy sculpted mesh with a pixel texture does not count.
- **No hands with a gun** (owner, 2026-11-14, GDD v0.68, `b377478`): with a gun the screen shows only the 3D gun and its own animations. Staff and sword keep their 2D sprites. Do not restart the first-person hand work.
- **Maps: the place before the assets** (owner, 2026-11-13, GDD v0.47, `c04f05e`): the first site maps were deleted because kit pieces were placed at random. Each space is first described as a place in its world; an asset goes in only because that description asks for it; maps are planned step by step with the owner. When kit sections do not meet, find the kit's own piece for that joint before improvising (the wall stair, `6db7b8b`).
- **The lab kit's figures, holograms and tubes belong to the Valkyrie's fight only** (GDD v0.50, v0.52, v0.55); the lab rooms and plain furniture can be used anywhere.
- **Solve, do not park:** fix what does not fit instead of noting it as "left out"; "we do not want a game that looks cheap".
- Assets that arrive go to `Glyphbound/SourceArt` (one copy per asset; delete the `Incoming` copy, old versions and intermediates). Every third-party asset is listed in the day entry (name, source, licence).

## How the owner works (what to expect)
- He plays the packaged build and sends short notes in Spanish, several in one message, sometimes one more while the work is running. Each note is a design decision: write it into the GDD first (changelog line, the section it belongs to, and section 18 for any detail you had to invent), then implement.
- When a note leaves a detail open, pick a reasonable value, mark it provisional in the GDD, put it in data, and list it in the final answer. Do not ask.
- He walks maps and sends numbered notes ("first walk", "second walk", ...). Each walk is a block; the builder script is fixed, not the map by hand.
- He unzips `Glyphbound/TeamTBD_Glyphbound.zip` next to it (`Glyphbound/TeamTBD_Glyphbound/`, git-ignored). The package starts on the castle; beside the exe: `Play-Castle.bat`, `Play-FutureBase.bat` (skips the castle) and `Play_AllWeapons.bat` (`-AllWeapons`: every weapon from the start). After every block he asks for a package when he wants one.
- The final answer of each block, in Spanish: what changed per note, the provisional choices, what he should check by playing, what was not verified.

## The loop for a round of tweaks
1. GDD: record the notes (bump the version, changelog line). Commit.
2. Read the code involved yourself before writing the brief (the brief must name files, functions and the exact rule).
3. Brief in `Prompts/<block>.md`: the rule per item, new data keys with their values, pure functions to add (so they can be tested), the verification list (Editor-target build, anchored test filter, `Smoke-Jam.ps1 -Only <Step> -Offscreen`), the machine-lock rule, "do not commit", the final report format. `Prompts/implementer_rules.md` is shared by all.
4. Small serial batch -> Muse Spark. Several independent areas -> write the public header declarations yourself, then one Claude subagent per area (each owns its files, none builds), then ONE integration pass that builds, tests and runs the smoke.
5. Look at the result yourself: the verbatim result lines, the diff of the risky parts, the smoke screenshots (`Glyphbound/Saved/Screenshots/Smoke`).
6. Sol 6.1 review (read-only), then a fix batch with your decision per finding.
7. Once at the end: game-target build, full smoke, commit, push. Package if asked or at the end of the day; run the packaged smoke (`-Packaged`). Write the block file `Devlog/days/<date>_<topic>_block.md`, update this file, re-run `New-DevlogDay.ps1`. Commit.

## Rules learned the hard way
- One build or run at a time on this machine (and now: hold `machine.lock`). Long commands in the background, no wait loops.
- While an agent is editing, `git add` explicit paths only (a "commit everything" swept half-edited files once). Do not edit a file an agent is working on.
- Always build the game target too (`-Target Glyphbound`) before packaging: Editor-only API compiles in the Editor and breaks packaging.
- The smoke script (`gb.Smoke`, `Core/GBDevCommands.cpp`) is the only automated check that the game actually runs. When a feature changes what the script does or its timing, update the script in the same batch. Screenshots are captured a few frames late: leave a second after each one.
- On real maps, use the tour: `Smoke-Jam.ps1 -Map <map> -Only tour -Offscreen` (every spot must have a floor under the player). Map builders also move a body of the player's size along every walk (`walks: clear on N walks`): a check at chest height let a 1.3 m doorway through once (GDD v0.66).
- A shooter's animation clips can carry their travel in the pelvis while the root stays still: enemies "teleported" through cover. Clips are played in place (`Devlog/days/2026-11-14_enemy_tactics_block.md`).
- Kit materials authored for Substrate render flat grey here (Substrate is off): rewire them (`fix_htb_materials.py`, `93d0128`).
- A `set_collision_enabled` from Python is overridden by the named collision profile on load (`0bb12e1`). Meshes built from Python data can come out with culling and normals opposite: check both sides (castle twins, `c262e4b`).
- Unreal string and JSON map keys ignore case: never two keys that differ only by case.
- The Cascade plugin must stay enabled (the cook fails without it). The plugin list is generated by `Tools/Jam/trim_plugins.js`.
- One-off scripts go in a file, never inline `node -e` with backticks inside bash; literal replacements with `split(a).join(b)`. `.ps1` files in ASCII or with BOM: a PowerShell rewrite once broke the GDD's UTF-8 (`fb20295`).
- A class member must never be named `Cast`. Files of the module are compiled together (unity build): file-local helper names must be unique across files.
- Skeletal FBX are exported in cm; `unreal.Rotator` is (roll, pitch, yaw); texture parameters need defaults.
- Tests: pure logic only, each must be able to fail, names `Glyphbound.<Area>.<Name>` where no name is a prefix of another.
- The Editor is closed only with `Stop-EditorMcp.ps1`. When the chat's own MCP connection is down, call the Editor with `node Tools/Jam/mcp_call.js`.

## What exists
- **Project:** `Glyphbound/Glyphbound.uproject`, UE 5.9.0. One game module (`Glyphbound`, prefix `GB`), C++ only, no Blueprint graphs. Game default map `/Game/Glyphbound/Maps/PastCastle`; the Editor starts on the engine's `Entry` map, where `AGBGameMode` builds `AGBTestArena` in code (the smoke steps run there).
- **Maps** (`/Game/Glyphbound/Maps`, binary, not in git, on CENTRAL):
  - `PastCastle`: the castle of the past, slice 1 (gatehouse, two wards with the orcs' camp and `Opening_Past`, the author's keep whole, wall walk, crypt stair, cells, the seal's hall). Built by `Tools/Jam/Build-PastCastle.py` (`"look": "pixel"`: every castle-pack mesh replaced by a plain-block pixel twin; 1.68 M triangles, no Nanite, 12 materials, 32 px tiles). 33 torches (`AGBTorch`). The seal sends the player to `FutureBase`. Plan: `Design/Levels/PAST_CASTLE_PLAN.md` (section 10 = as built).
  - `FutureBase`: the military base of the future, slice 1 (control room with the seal, corridor and air plant, transfer bay, long Spaceship Corridor, sub-boss hall with the horde chief, breakable glass bulkhead, flooded deep wing with a hand ladder, the tentacle boss's hall, the medium room's energy gate and Power Cells, the Valkyrie's room). Built by `Tools/Jam/Build-FutureBase.py`. Plan: `Design/Levels/FUTURE_BASE_SLICE1.md`.
  - `Maps/Dev/Review_Castle`, `Review_Enemies`: review stages for renders.
- **Code:** 193 `.h`/`.cpp` files, about 58,600 lines (game module 174 files, about 52,900 lines; plugin 19 files). 69 pure-logic tests in `Source/Glyphbound/Tests`. Layout and conventions: `Design/ARCHITECTURE.md`.
- **Plugin `Glyphbound/Plugins/GBPixelVfx`** (our own, jam-made; `README.md`): bakes a Niagara Fluids simulation into a pixel-art flipbook in the master palette and plays it as a camera-facing sprite with a real light (`UGBPixelVfxComponent`). Clips in `Content/Data/Vfx`: `FluidFire`, `FluidSmoke`, `FluidExplosion`, `FluidArcaneFire`, `TorchFire`. Used by the Fireball in flight and the castle's torches. Bake jobs in `Plugins/GBPixelVfx/Bake/Jobs/*.json`.
- **Data:** `Glyphbound/Content/Data/*.json`, 12 files (`Player`, `QTE`, `Game`, `Spells`, `Weapons`, `StatusEffects`, `Items`, `Enemies`, `Encounters`, `Skills`, `Sprites`, `Cutscenes`), plus the 5 baked clips (PNG + JSON) in `Data/Vfx`. Loose files next to the packaged exe; `gb.ReloadData` reloads in game.
- **Content:** everything in `Content/` except `Content/Data` is binary and **not in git** (team decision): kits, imported models, maps. It lives on CENTRAL; `Tools/Jam/Backup-Content.ps1` copies it. `M_PixelSurface` / `M_PixelDecal` are regenerated by `Make-Materials.ps1` after a fresh clone.
- **Fab / third-party packs in Content** (each listed with source and licence in the block files), among them: Medieval Kingdom (the castle pack; now only two rubble pieces are cooked), Sci-Fi Creatures Research Lab, High Tech Base, Modular SciFi Station, Deep Water Station, Sci-fi Impacts, Muzzle Flash 3D, Scifi Material Pack, Hologram Shader, Smoke Niagara Fluids, Realistic Gun VFX, Kingboars Pistol and Rifle Locomotion Animations (12 clips used), the Spaceship Corridor model. Collect the full list from `Devlog/days/2026-11-13_fab_assets_block.md` and the 2026-11-14 block files before submission.
- **Enemies** (`Enemies.json`): past pixel-art imps and orcs (`Imp_Past`, `Orc_Past`, `_Shaman`, `_Champion`, `_Juggernaut`, `_Brute`); future 3D armored orcs (`Orc_Future`, `_GunCarrier`, `_Enforcer`, `_Siege`, horde chief `_Warlord` with shield and one-handed heavy pistol), `Robot_Future`, `Drone_Future`, `Sentinel_Future`, `Spider_Future`; `Valkyrie_Past` / `Valkyrie_Future`; the Entity (`Entity_Phase1`, 3D, 9.4 m); `Herald`, `TrainingDummy`.
- **Weapons:** `Staff_Apprentice`, `Staff_Ember`, `Pistol`, `Rifle`, `Shotgun` (3D models; shell-by-shell reload and pump), `ElementalSword`. **Spells:** Fireball, Fire Wall, Flamethrower, Lightning Bolt, Lightning Rune, Static Orb, Ice Wall, Ice Shield, Ice Floor, three Air passives, Weapon Enchant (element chosen in its QTE).
- **Day entries:** `Devlog/days/2026-11-12.md` (final). Days 13 and 14 exist only as drafts (`2026-11-13_DRAFT.md`, `2026-11-14_DRAFT.md`, not yet committed) plus their block files; screenshots in `Devlog/media/<date>/`.

## Where to tweak what (most of the owner's notes are one of these)
| The owner says | Look here first |
|---|---|
| Movement, jumps, dash, health, mana, starting gear | `Player.json`; `Player/GBPlayerCharacter.cpp` (`ApplyMovementStats`, `OnJumped_Implementation`, `InputDash`) |
| Ledge climb, vault, hand ladders | `Player.json` (`Vault*`, climb keys); `Player/GBMantle.cpp`; `World/GBLadder.cpp`; `AGBPlayerCharacter::StartLadder` |
| A weapon's numbers (damage, rate, recoil, head bonus, spread, reload) | `Weapons.json`; `Weapons/GBGun.cpp`, `GBStaff.cpp`, `GBSword.cpp` |
| A gun's first-person pose, weapon FOV, bob | `Weapons.json` (pose keys); `Weapons/GBGun.cpp`; console `gb.ViewPose`, `gb.ViewFreeze` |
| A spell's numbers, QTE scaling, look | `Spells.json` (each param `Base / Min / Max / ScaledBy`, optional `Cap`); `Spells/GBSpell_*.cpp` |
| QTE time, keys, gestures, enchant element choice | `QTE.json` (`ChoiceTimeLimit`); `QTE/GBQTELogic.cpp`, `QTE/GBQTEComponent.cpp` |
| Enemy numbers, meshes, clips, armor pieces, drops | `Enemies.json` (per id); `AI/GBEnemyCharacter.cpp` (base), `AI/GBEnemy3D.cpp`, `AI/GBSkeletalEnemy.cpp`, `AI/GBEnemyAnimInstance.cpp` |
| Enemy tactics: squads, zones, patrols, cover, flanking, attack tokens, temperament | `Enemies.json` (`Aggression`, `CoverStay*`, `NoticeTime`, `HearingRange`...), `Game.json`; `AI/GBSquad.cpp`, `AI/GBTactics.cpp`, `AI/GBEngage.cpp`, `AI/GBEncounter.cpp`, `AI/GBEncounterStarter.cpp` |
| Flyers, sentinel, horde chief's shield | `AI/GBFlyerEnemy.cpp`, `AI/GBDroneEnemy.cpp`, `AI/GBSentinelEnemy.cpp`, `AI/GBShieldEnemy.cpp` (arm IK `FGBArmIK` in `GBEnemyAnimInstance`) |
| Bosses: the Entity (tentacles), the Valkyrie in her tube | `Enemies.json` (`Entity_Phase1`, `Valkyrie_Future`: `DormantWakeRange`, `WakeLeap*`); `AI/GBEntity.cpp`, `AI/GBTentacleRigComponent.cpp`, `AI/GBTentacleMath.cpp`; `gb.SpawnEntity`, `gb.OpenValkyrie` |
| Encounters: who is where, waves, reinforcements | `Encounters.json` (`Opening_Past`, `Future_Bay`, `Future_Spiders`, `Future_DeepWing`, `SubBoss_Future`, `Boss_Valkyrie_Future`...); placed by the map builders' `encounters` stage |
| Perks (and the reset `ResetAll`) | `Skills.json` (modifier keys `Weapon.<Id|Slot|All>.<Stat>`, `Spell.<Id|All>.<Param>`, player stats by name); `Progression/GBSkillTreeComponent.cpp`; balance: `Tools/Balance/balance_model.js` |
| Elemental build-up (thresholds, resistances, reactions) | `Enemies.json` per enemy, `Game.json` `ElementalDefault.*`; `Combat/GBElementalStatusComponent.cpp`, `Combat/GBBuildUpMeter.cpp` |
| Slows, Ignite, potions, enchant buffs | `StatusEffects.json` |
| Items, armor, recipes | `Items.json`; `Items/GBInventoryComponent.cpp` |
| HUD (two skins), menus, skill tree pages, menu transitions, damage numbers | `UI/GBHUD.cpp`, `GBHUD_Rpg.cpp`, `GBHUD_Cyber.cpp`, `GBHUD_Menus.cpp`, `GBHUD_Paint.cpp`, `GBHUD_DamageNumbers.cpp`, `GBPixelFont.cpp`; layout values in `Game.json` |
| Cutscene captions (seal, first gun) | `Cutscenes.json`; `World/GBCutsceneSubsystem.cpp`, `World/GBSeal.cpp` (`TravelMapPath`) |
| The castle map (layout, twins, torches, lights) | `Tools/Jam/Build-PastCastle.py` (stages; `torches`, `place_torches`), `castle_twins*.js/.py`, `castle_twins_cfg.json`; `World/GBTorch.cpp`; flame clip `Plugins/GBPixelVfx/Bake/Jobs/TorchFire.json` |
| The future base map | `Tools/Jam/Build-FutureBase.py`; doors, glass, gates, ladders: `World/GBDoor.cpp`, `GBBreakable.cpp`, `GBEnergyGate.cpp` (also `AGBKeySocket`), `GBLadder.cpp` |
| Baked fluid effects (Fireball fire, torch flame) | `Plugins/GBPixelVfx` (`README.md`), jobs `Bake/Jobs/*.json`, `PixelVfx-Jam.ps1 -Action Bake -Id <Id>`; clip sizes in `Spells.json` |
| Kill height, corpse time, global values | `Game.json` |
| Placeholder sprites, tiles, spell icons | `Sprites.json` |
| Keys and buttons | `Player/GBInputSet.cpp` (GDD §9 is the list) |
| The test arena (smoke steps) | `World/GBTestArena.cpp`; steps in `Core/GBDevCommands.cpp` |
| Damage rules | `Combat/GBCombatLibrary.cpp` (`DealDamage`, `ApplyWeakPointBonus`) |

## How to run the usual things
All from `C:\GameJam`. Take `Glyphbound/Saved/machine.lock` first.
```
powershell -File Tools/Jam/Build-Jam.ps1                      # Editor target (default); Editor must be closed
powershell -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound   # game target: once at the end, before packaging
powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.QTE.'          # anchored filter while iterating
powershell -File Tools/Jam/Test-Jam.ps1 -MinTests 69                        # whole suite (default filter '^Glyphbound.')
powershell -File Tools/Jam/Smoke-Jam.ps1 -Only <Step> -Offscreen            # while iterating (steps: Entity, mantle, shotgun, ui, seal, tour, ...)
powershell -File Tools/Jam/Smoke-Jam.ps1 -Map /Game/Glyphbound/Maps/PastCastle -Only tour -Offscreen   # a real map's tour
powershell -File Tools/Jam/Smoke-Jam.ps1 -Offscreen                         # full run (about 350 s), once at the end
powershell -File Tools/Jam/Smoke-Jam.ps1 -Packaged -Offscreen [-ResX 1920 -ResY 1080]                  # on the packaged exe
powershell -File Tools/Jam/Package-Jam.ps1 -Project C:\GameJam\Glyphbound\Glyphbound.uproject -ZipName TeamTBD_Glyphbound
powershell -File Tools/Jam/Make-Materials.ps1                 # only after a fresh clone or when make_materials.py changes
powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Bake -Id TorchFire      # actions: Build, Test, Materials, Bake, Compare, Showcase
powershell -File Tools/Jam/Start-EditorMcp.ps1 -Project C:\GameJam\Glyphbound\Glyphbound.uproject -Port 8059
powershell -File Tools/Jam/Test-UnrealMcp.ps1                 # probe the Editor's MCP
powershell -File Tools/Jam/Stop-EditorMcp.ps1                 # the only way to close the Editor (collects its log)
node Tools/Jam/mcp_call.js py Tools/Jam/Build-PastCastle.py   # run a builder in the open Editor (same for Build-FutureBase.py)
powershell -File Tools/Devlog/New-DevlogDay.ps1 -Date 2026-11-15
```
Muse Spark (in the background):
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Devlog\Invoke-LoggedAgent.ps1 -Agent opencode -Model 'opencode-go/muse-spark-1.3-contributor#high' -PromptFile Prompts\X.md -Purpose '...' -Command "opencode run --auto -m 'opencode-go/muse-spark-1.3-contributor#high' --title '...' (Get-Content Prompts\X.md -Raw)"`
Sol 6.1 review (read-only):
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Devlog\Invoke-LoggedAgent.ps1 -Agent codex -Model 'gpt-6.1-sol (medium)' -PromptFile Prompts\X.md -Purpose '...' -Shell cmd -Command 'codex exec -m gpt-6.1-sol -c model_reasoning_effort=medium -s read-only -C . - < Prompts\X.md'`
A data-only change (JSON) needs no build: run the affected smoke step, then package.
Console (Development builds only): `gb.ReloadData`, `gb.QTE 0|1`, `gb.God`, `gb.OpenDoors`, `gb.OpenValkyrie`, `gb.SpawnEntity`, `gb.ShowElemental 1`, `gb.BuildUp`, `gb.ViewPose`, `gb.ViewFreeze`, `gb.Smoke`.

## State on the final day (2026-11-15)
### Done (in the last package or committed after it)
- Combat core: staff + 13 spells with the QTE, the weapon enchant with the element chosen in its QTE, three 3D guns (pistol, rifle, shotgun with pump and shell-by-shell reload), elemental build-up, perks in two skill trees (spell tablet; weapon hub with gun pages), skill-point confirmation, menu transitions, two full HUD skins.
- Movement: double jump, dash, ledge climb in any direction, vault, hand ladders.
- `PastCastle` slice 1 in plain-block pixel art (GDD v0.68), with torches whose flame is baked from Niagara Fluids (`f4dc179`, after the last package); the seal's cutscene opens `FutureBase`.
- `FutureBase` slice 1 with its encounters; enemies with squad tactics (zones, patrols, cover, flanking, attack tokens), the horde chief with shield and one-handed pistol, the Valkyrie sealed in her tube until the glass breaks, glass that shatters pane by pane.
- The Entity's phase 1 as a 3D floating boss (`gb.SpawnEntity`; not placed on a map).
- Last package (`58af4a1`): BUILD SUCCESSFUL, packaged castle tour 42 spots, 0 without floor, 0 engine errors.

### Open (GDD §11, §15, §17 and the latest commits)
- **Zip size.** `58af4a1`: `TeamTBD_Glyphbound.zip` **2,024.9 MB**; the jam page asks for desktop zips under 1 GB and `CLAUDE.md`'s checklist says under 1 GB. GDD §0.1 notes bigger builds are allowed through butler, and the owner said on 2026-11-14 that the limit "does not matter for now", but it must be settled before submission. Known facts: the castle pack now cooks 84 MB, all from two rubble pieces `FutureBase.umap` still uses (`SM_Rubble_02`, `SM_PillarTop_Broken`); "the rest is mostly the future base's kits". Next step: measure the cooked size per content folder, then replace or drop what the maps do not need. Ask the owner once whether to aim for 1 GB or upload the bigger zip with butler.
- **Torch work in progress** in another chat (uncommitted light offset in `World/GBTorch.*` and the plugin component). Do not commit those hunks as yours.
- **Day entries:** 2026-11-13 and 2026-11-14 are drafts only; `New-DevlogDay.ps1` and the human side are still to do, plus 2026-11-15. The Glass Box link must be submitted with the game.
- **Before submission:** remove or hide the "+1 skill point" test button (`AGBHUD::AddTestSkillPoint`, GDD v0.18) and the free skill reset (owner, v0.29: "will not be in the final version"); game title and team name are still TBD (GDD §17); "Tools you used" list kept current.
- **Not built:** the past Valkyrie's place and fight, the elemental zones of the castle, the sword's hilt + gem crafting in play, the final portal map and the Entity's arena, the Entity's tentacle melee (pending, owner 2026-11-13), which boss the tentacle boss is, which enemies drop the Power Cells, the second castle slice. Audio is not mentioned in any block.
- **NVIDIA RTX Award:** DLSS is not installed (GDD §17, item 1). Without it the game does not qualify.
- **Not walked by hand:** the pixel castle and its torches; the shadowed wards read dark.
- Video (`Design/VideoScript.md`) and the itch.io upload are for a human.

## Known limits (accepted for now)
- Enemies do not attack spell walls on purpose.
- Pixel tiles on a wall placed diagonally stretch up to about 1.4x (world-aligned projection picks one axis).
- Spell projectiles and explosions get no head bonus (a spell can opt in with a `HeadshotMult` param).
- A dropped gun comes back fully loaded when picked up again. Spell cooldowns persist through death.
- `gb.*` console commands do not exist in Shipping builds (we package Development).
- Castle twins: a step where a piece's own origin is off the 2 m tile; some 25 cm crenellations read ragged from below.
- The warm Editor boot did not get faster with fewer plugins (about 10-11 s either way).
