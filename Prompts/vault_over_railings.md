# Task: the player vaults over a railing he cannot jump (owner, 2026-11-14, after the test package)

Read first: `Prompts/implementer_rules.md` (with the "Machine rules" below instead of its "do not build"),
`Glyphbound/Source/Glyphbound/Player/GBMantle.h/.cpp`, `Player/GBPlayerCharacter.h/.cpp` (the "Ledge climb (mantle)" parts),
the mantle block of the smoke run in `Core/GBDevCommands.cpp` (`gb.Smoke mantle`), `Content/Data/Player.json`,
`Tests/GBPlayerTests.cpp`, and the earlier briefs `Prompts/mantle_climb.md` and `Prompts/mantle_any_direction.md`.

## What the owner said (translated)
"That railing should be jumpable with the first jump; if it cannot be, make it so that when the player is right against
it, he climbs it and passes to the other side, with an animation of going up and coming down."

## What the architect measured
The railing is the corridor's (`SM_Railing_03`, High Tech Base kit): 240 cm long, 56 cm deep at its feet but a thin frame
(a few cm of bar), its top 127 cm above the floor. The player's jump reaches 204 cm, but that corridor's ceiling is 300 cm
above the floor and the player's capsule is 180 cm tall: his feet top out at 120 cm, 7 cm short. So the jump can never
clear it there, and the existing ledge climb does not take it either (it needs a top to stand on; a railing has none).
Behind this particular railing the floor is 290 cm lower (a pit he can also reach by steps): a vault must work with a
drop on the far side. The map's other railings are the catwalk's (`SM_railing_200_*`, 100 cm high, 12 cm thin).

## What to build
A **vault**: a second traversal move next to the ledge climb, sharing its conditions (`CanMantleNow`), its camera pose
layer and its style.
- **When**: the player presses Jump on the ground (or is in the air and pressing forward) within reach of a **thin
  obstacle** in the direction he intends to go (the same probe directions as the mantle: intent first), where
  - the obstacle's top is between `VaultMinHeight` and `VaultMaxHeight` above his feet (stats in Player.json, 50 and 150),
  - it is at most `VaultMaxDepth` deep along the crossing (60 cm): beyond it, at capsule height, there is free room for
    the capsule (sweep), within `VaultReach` of him (same idea as the mantle's reach),
  - there is something to land on beyond it within `VaultMaxDrop` below his feet (stat, 450 cm; a floor found by a
    trace; no floor = no vault, he must never vault into a void), and the landing spot is not inside geometry,
  - and the ledge climb does NOT apply (a real ledge with standing room is climbed as today: the mantle is tried first).
  Also **when he just pushes against it**: holding forward into such an obstacle on the ground for `VaultPushTime`
  (0.35 s) starts the vault without a jump press (the owner's "if the player is right against it").
- **The move**: takes `VaultTime` (0.55 s). The capsule follows an arc: up to just above the obstacle's top, across,
  and down to its own height on the far side at the height of the near floor (or a little below the top when the far
  floor is lower), then control returns and he falls or stands normally (keep some forward speed, `VaultExitSpeed`).
  Collision is handled the way the mantle handles it (no pushing through other geometry: the path is checked before it
  starts, and the move aborts cleanly if something moving gets in the way).
- **The look ("an animation of going up and coming down")**: first person. The camera rises and tips forward on the way
  up, dips and rolls a few degrees to one side over the top (the side of the supporting hand), and settles on the way
  down; the held weapon lowers out of the way during the move and comes back (reuse whatever the mantle does to the held
  item and the camera: `GBMantle::FMantlePose` or a sibling). All amounts from Player.json.
- Enemies shooting, QTE, dash, ladders, freeze: the same rules as the mantle (no vault during them; a vault cannot be
  started while a mantle runs and the other way round).
- Pure rules (does this obstacle qualify, the arc position at a given alpha, the camera pose at a given alpha) go in
  `GBMantle` (or a new `GBVault` beside it) as pure functions with tests in `Tests/GBPlayerTests.cpp`: each test must be
  able to fail; names must not be prefixes of one another.
- Smoke: extend the mantle block (`gb.Smoke mantle`) in the test arena with a railing-like obstacle (a thin placeholder
  wall 120 cm high and 10 cm deep, one with a floor 250 cm lower behind it, and one under a ceiling 300 cm high so a plain
  jump cannot clear it): the player placed against each and given the input must end on the far side, on his feet, in
  under 1.5 s; a 200 cm high thin wall must NOT be vaulted; a thin wall with no floor beyond within the drop limit must
  NOT be vaulted. Say in the report which wrong code each check catches.

## Machine rules
- Another implementer is changing the enemies' AI in `AI/*`, `Core/GBDevCommands.cpp` (the `Enemies3D` blocks and dev
  commands) and the data files at the same time. You own `Player/*`, the mantle block of `GBDevCommands.cpp`,
  `Tests/GBPlayerTests.cpp` and your stats in `Player.json`. Make small targeted edits in shared files; never reformat.
- You build and check yourself, one build or game run at a time on this machine: before every build, test or smoke run,
  wait while `C:\GameJam\Glyphbound\Saved\machine.lock` exists and is younger than 45 minutes or an `UnrealEditor.exe`,
  `UnrealBuildTool` or `cl.exe` process is running (check every 60 s; keep writing meanwhile); then write the lock
  (`Sonnet-vault | <what> | <UTC time>`), run, and delete it.
- **If the build fails in files you do not own, do not touch them**: the other implementer is in the middle of an
  edit. Wait two minutes and build again.
- Build: `powershell -File Tools/Jam/Build-Jam.ps1`. Tests: `powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.Player.'`.
  Smoke: `powershell -File Tools/Jam/Smoke-Jam.ps1 -Only mantle -Offscreen`.
- Iterate cheaply: only these filtered checks; never the full smoke, the full suite, the game target, the packager, PIE
  or the Editor. Do not commit. Do not launch other agents. English, ASCII, no gameplay numbers in code.
- UE 5.9: check engine APIs in `C:\Program Files\Epic Games\UE_5.9\Engine\Source` before using them.

## Final report
Files changed; stats added with values; what each new test and smoke check catches; the exact last results of build,
tests and the filtered smoke; what you did not do or could not verify.
