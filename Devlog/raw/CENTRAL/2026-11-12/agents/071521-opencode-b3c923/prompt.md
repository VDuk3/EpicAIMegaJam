# Task: fix the defects found by the code review and the smoke run

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run tests in this task; everything else in "Hard rules" applies). The code builds and its 20 tests pass. A reviewer (Codex) and an automatic smoke run found the defects below. The architect has decided how each one is fixed: implement exactly these decisions, with the smallest correct change.

After the changes:
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` (Editor target) must print `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` (game target: catches editor-only API) must print `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 20` must pass.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` must end with `Smoke: done=True crashed=False LogGB errors=0`. It runs the game by itself for about 25 seconds (console command `gb.Smoke`, code in `Core/GBDevCommands.cpp`) and prints what happened. Read its output: every `SMOKE` line should make sense.
One build or run at a time. Do not open the Unreal Editor yourself. Do not commit.

## Fixes

### 1. Dying must never lose what the player collected
Today a death restores the whole checkpoint snapshot (inventory, weapons, spells, skills) while collected pickups stay gone and opened chests stay open, so rewards taken after the last checkpoint are lost for good.
**Decision:** death no longer rolls back what the player owns. On respawn the player goes back to the checkpoint's place and era with full health, full mana and full ammo, and keeps everything else as it is now (items, weapons, spells, skill points, armor).
- Add `void AGBPlayerCharacter::RespawnAt(const FGBPlayerSnapshot& Snapshot)`: end any dash, QTE and sustained spell; teleport to `Snapshot.Transform`, set `Snapshot.ControlRotation`, zero velocity; `UGBEraSubsystem::SetEraImmediate(Snapshot.Era)`; remove all status effects; `RestoreAll()`; `SetControlEnabled(true)`; close the death menu if open.
- `UGBProgressSubsystem::RestoreCheckpoint` calls `RespawnAt` instead of `LoadFromSnapshot`. Update its header comment and the comment of `FGBPlayerSnapshot` usage accordingly.
- Keep `SaveToSnapshot` / `LoadFromSnapshot` on every component as they are (they will carry the player to the final-boss map later).

### 2. Projectiles fly through the ice wall
`Spells/GBSpell_Wall.cpp` disables overlap events on its box, and projectiles only overlap `WorldDynamic`. Enable overlap generation on the wall's collision box when the wall is blocking, and make sure `AGBProjectile` then damages the wall and stops (it already treats Neutral actors with attributes as targets that stop it). Check both paths: a projectile fired from outside, and one fired while already touching the wall.

### 3. A cancelled QTE can still cast, even from a dead player
- `UGBQTEComponent::CancelQTE()` must finish with an unsuccessful result: `bSuccess = false` (keep `bUsedQTE = true`), whatever the score so far.
- `UGBSpellbookComponent::FinishCast` must not spawn anything when the owner is dead (still unlock weapon switching and clear its pending state).

### 4. Era travel while dying
`World/GBEraSubsystem.cpp`: if the traveller is dead when the screen is fully covered, do not teleport, restore or save a checkpoint: just undo the travel (stay in the current era and fade back in). Never call `RestoreAll()` or `SaveCheckpoint` for a dead player.

### 5. Enemies of the other era keep simulating
`World/GBEraComponent.cpp`: when the owner is not present, also stop the tick of every component of the owner that ticks (attributes, status effects...), remembering which ones were enabled, and re-enable exactly those when it is present again. Result: a burning enemy left in the other era neither burns nor regenerates until the player comes back.

### 6. Explosions hit through walls
`UGBCombatLibrary::DealRadialDamage`: skip a target when something solid stands between the explosion origin and the target. Use one line trace on `ECC_Visibility` from the origin to the target's location, ignoring the target itself and every actor in `IgnoreActors`; a blocking hit on another actor means "obstructed" unless that actor is the target. Lift the trace start a few centimeters if the origin is on the floor so the ground itself does not block it.

### 7. Enemy melee hits through walls
`AI/GBEnemyCharacter.cpp`, melee branch of `PerformAttack`: require line of sight to the target (reuse the enemy's existing `HasLineOfSight`) before dealing the hit.

### 8. The gun enchant also buffs staffs and swords
- Add a slot-wide modifier key: in `Core/GBStatNames.h` add `GBStat::WeaponSlotKey(EGBHeldSlot Slot, FName Stat)` returning `"Weapon.Staff.<Stat>"`, `"Weapon.Gun.<Stat>"` or `"Weapon.Sword.<Stat>"` (include `Core/GBTypes.h` there).
- `AGBHeldItem::GetStat` evaluates three keys: `Weapon.<Id>.<Stat>`, `Weapon.<Slot>.<Stat>`, `Weapon.All.<Stat>`.
- `Content/Data/StatusEffects.json`: the three `Enchant_*` effects use `"Weapon.Gun.Damage"` instead of `"Weapon.All.Damage"`.
- Mention the new key in the modifier-key lists of `Design/ARCHITECTURE.md` and in the comment of `FGBModifierDef::Stat` in `Data/GBDataTypes.h`.

### 9. Ammo pickups vanish when only part of them fits
`UGBInventoryComponent::AddItem`, type Ammo: return how many ammo items were really used (magazines added divided by magazines per item, rounded up), not the whole requested count, so the pickup keeps the remainder.

### 10. Problems seen in the smoke run
See the section "Smoke run findings" appended below by the architect (if it is empty or missing, there are none).

## Tests
Add or extend a pure-logic test only where one of these fixes has pure logic worth pinning (for example `GBStat::WeaponSlotKey` naming, or a small pure helper you extract for the ammo count). Do not write tests that need a world. Keep the rules: each test must be able to fail; no test name that is a prefix of another.

## Final report (plain text)
1. The four result lines (two builds, tests, smoke), copied verbatim.
2. What you changed, per numbered fix, with file names.
3. Anything you could not do or are unsure about.

## Smoke run findings (architect, 2026-11-12)
The first smoke run did not crash, but:

### 10a. The test arena is never built, so the player falls forever
`AGBGameMode::InitGame` builds `AGBTestArena` only when the map has no `APlayerStart`, and the engine's `/Engine/Maps/Entry` map (our default map) has one. Result: no floor, no enemies, nothing to play.
**Decision:** build the arena when the world has no `AGBTestArena` yet AND (the map has no `APlayerStart` OR the map's package name starts with `/Engine/`). When the arena was built, `ChoosePlayerStart_Implementation` must return the `APlayerStart` the arena created (keep a pointer to it; add a getter on the arena if needed), not the one that came with the map. Use `UWorld::RemovePIEPrefix` on the package name so it also works in the Editor.
The smoke script now logs an error when the player falls below Z = -5000 or when the level has no enemies at the start, so this shows up as `LogGB errors` in the summary.

### 10b. Look at the first-person gun placeholder
In the screenshots the pistol placeholder looks like one long thin dark wedge going from the middle of the screen to the lower right. Check `Weapons/GBGun.cpp` (part sizes, relative transforms, whether the parts inherit an unwanted scale from `HeldItemAnchor`, and the anchor offset in `Player/GBPlayerCharacter.cpp`): it should read as a small pistol in the lower right of the view, roughly 25-35 cm long, pointing forward. Sizes and offsets of a placeholder viewmodel are visual constants, so code constants are fine. Do not spend long on looks; just make it sane.

### 10c. After the fixes, read the smoke output critically
Besides `done=True` and `LogGB errors=0`, check these lines make sense: enemies > 0 at the start; the player's Z stays near the floor; `hit enemy ... dead=1` appears; era is `Future` in the "after travel" line and the player's mana is full there (era travel restores everything); after the respawn the player is alive, with the pistol and the learned spells still owned (fix 1).
