# Task: fixes after the review of the play-test 1 changes

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

A reviewer read the last change and found four defects; the architect decided each fix below and added a fifth item. Make exactly these changes, nothing else.

## 1. Line of sight must not pass through an allied enemy into cover
`AI/GBEnemyCharacter.cpp`, `HasLineOfSight` (around line 655): today, when the trace hits another enemy, it counts as a clear line. Wrong: a wall may be behind that enemy.
Fix: when the trace hits a pawn that is not the goal (another enemy), add it to the ignored actors and trace again from the same start, until the trace reaches the goal (clear), hits nothing (clear) or hits something that is not a pawn (blocked). Cap the loop at 8 repeats; if the cap is reached, the line is blocked.

## 2. Local steering must not walk off ledges
`AI/GBEnemyCharacter.cpp`, `SteerTowards` / `IsDirectionBlocked` (around lines 470-530): candidate directions are only checked for obstacles, never for ground, and the "boxed in" fallback steps backward unchecked.
Fix:
- A direction also counts as blocked when there is no walkable ground ahead: from the point `EnemySteerLedgeProbeDistance` cm along the direction (at the capsule's base height plus the step height), trace down `EnemySteerLedgeDrop` cm plus the step height against the same collision channel the capsule uses for the world, ignoring pawns; no hit, or a hit on a slope steeper than the walkable floor angle of the movement component, means no ground.
- New Game.json "Values" keys with those names (add them to `GBEnemyKey` like the others): `EnemySteerLedgeProbeDistance` 80, `EnemySteerLedgeDrop` 150.
- The backward fallback uses the same check (obstacle and ground); when it fails too, the enemy stays in place this tick (`StopMoving`), it does not move blind.
- Keep the pure function `GBSteering::ChooseOffset` unchanged (ledges just feed its `Blocked` array), so the existing test stays valid.
- The goal's own position never counts as a ledge when the goal is within the probe distance.

## 3. The Weapons page must show the weapon's real element
`UI/GBHUD_Menus.cpp` (around lines 1109 and 1126): the row and the details use `Def->Element`. Use `Equipment->GetWeaponElement(WeaponId)` in both places (a forged elemental sword must show its element).

## 4. Spell visuals that data defines must come from data
- **Raised shield in the HUD** (`UI/GBHUD.cpp`, `DrawRaisedShield`, around line 614): the sprite id is hardcoded to `ice_shield`. Read it from the active shield's spell definition: its "Strings" value `Sprite` (fallback `ice_shield`). Tint the sprite with the definition's `Color` string only through a NEW optional string `HudTint` ("#RRGGBB"; absent = white), so today's look does not change. Use the spellbook's `GetShieldActor()` and the spell actor's existing accessors for its definition / strings; do not add a second data lookup path if one exists.
- **Textured walls** (`Spells/GBSpell_Wall.cpp`, around line 65): the tint passed to `SetupTexturedShape` is always white. Read a NEW optional string `Tint` ("#RRGGBB"; absent = white). Do not use `Color` there (it is the plain-shape color and would darken the tile).
- In `Spells.json`, entry `IceShield`: remove the params / strings that nothing reads any more since the shield stopped being a world object (`SpriteSize`, `Distance` if no code reads them: check with a search first; keep `Sprite` and `Color`). Document `HudTint` and `Tint` in the header comments of `Spells/GBSpell_Shield.h` and `Spells/GBSpell_Wall.h` next to the other strings.

## 5. Smoke script: the shield screenshot is taken before the shield is on screen
`Core/GBDevCommands.cpp`: the step "summon the ice shield, raise it, screenshot" raises the shield and asks for the screenshot in the same step, so the picture shows the first frame of the raise animation (shield still below the HUD panel). Split it: one step summons and raises (and logs `SMOKE shield cast=.. hasShield=.. raised=..`), a new step 0.7 s later takes the screenshot `9_shield`, and the "lower the shield" step runs at least 1.0 s after the screenshot. Shift the following steps accordingly and update the step count.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 24` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, the line `SMOKE path check passed` is there, and no `LogGB` warning appears.
Never make an error disappear by deleting behavior. All JSON files stay valid.

## Final report (plain text)
1. The four result lines, copied verbatim, plus the `SMOKE path check end:` line.
2. What you changed per item, with file names.
3. Anything you could not do as described, and why.
