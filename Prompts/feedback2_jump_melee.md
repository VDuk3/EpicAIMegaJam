# Task: committed jumps, aimed sword hits, head bonus for staff and sword

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

The owner played again and asked for three changes (GDD v0.15: changelog at the top of `Design/GDD.md`, and the lines about jumps in section 18, "Melee" and "Headshots on sprites"). The architect decided the design below; implement exactly this.

## 1. Jumps commit to their direction
Owner: "When jumping, the direction should not be changeable in the air. The second jump can choose where it goes, but once in the air the direction cannot be changed. Like a jump in real life."

- `Glyphbound/Content/Data/Player.json`: `AirControl` becomes `0` (the stat stays, so a modifier could raise it later). Add two stats: `JumpTakeoffMinSpeedMult` `0.6` and `AirJumpMinSpeedMult` `1.0`. Add their names to `Core/GBStatNames.h` next to `AirControl`, with a comment.
- In `Player/GBPlayerCharacter.*`, when a jump actually starts (override `OnJumped_Implementation`; check in the engine's `ACharacter::CheckJumpInput` how `JumpCurrentCount` reads at that moment, and whether the jump started from the ground or in the air), set the horizontal velocity with a new **pure static function** (so it can be tested):
  `static FVector2D ComputeJumpHorizontalVelocity(const FVector2D& CurrentVelocity, const FVector2D& InputDirection, float MaxWalkSpeed, float MinSpeedMult);`
  - `InputDirection` is the world-space direction of the movement input held at takeoff (zero when none; the character already keeps the last move input, see `LastMoveInput` and how the dash turns it into a world direction: reuse that conversion, do not duplicate it).
  - No input: returns `CurrentVelocity` unchanged (a jump keeps its drift).
  - With input: direction = `InputDirection` normalized; speed = max(length of `CurrentVelocity`, `MaxWalkSpeed * MinSpeedMult`).
  - First jump from the ground uses `JumpTakeoffMinSpeedMult`; a jump that starts in the air (the double jump, or the first jump after walking off a ledge) uses `AirJumpMinSpeedMult`.
  - The vertical velocity set by the engine's jump is not touched.
- Make sure nothing else steers in the air: with `AirControl` 0 the engine applies no input acceleration while falling; check that the character's own code does not add velocity from input while falling (the dash is the exception and stays exactly as it is, in the air too), and that `BrakingDecelerationFalling` and `FallingLateralFriction` are 0 so the jump keeps its horizontal speed.
- Do not change the jump course of the test arena unless its math used `AirControl` (it should not).

## 2. One rule for the head / weak point bonus, used by every weapon attack
Today the gun multiplies by its `HeadshotMult` itself, the projectile only sets `Info.bHeadshot` without any bonus, and the sword does neither.

- Add to `Combat/GBCombatLibrary.h/.cpp`:
  `static bool ApplyWeakPointBonus(const AActor* Target, FGBDamageInfo& Info, float WeakPointMult);`
  If `WeakPointMult > 1` and `Info.HitLocation` is on the target's weak point (today: an `AGBEnemyCharacter` whose `IsHeadshotLocation(Info.HitLocation)` is true), multiply `Info.Amount` by `WeakPointMult`, set `Info.bHeadshot = true` and return true. Otherwise leave `Info` alone (and `bHeadshot` false) and return false. The comment must say this is the single place to extend when enemies get other weak points.
- `Weapons/GBGun.cpp` (`FireHitscan`): use it instead of its own check. Same numbers as before (falloff multiplier still applies; set `Info.HitLocation` before calling).
- `Combat/GBProjectile.h/.cpp`: add `float HeadshotMult = 1.f;` to `FGBProjectileConfig` (comment: weak point multiplier; 1 = no bonus). In `HitTarget` (direct hit, not the explosion path) replace the bare `Info.bHeadshot = ...` with the helper. Any other place that sets `bHeadshot` by hand: use the helper too (search for `bHeadshot`).
- `Weapons/GBStaff.cpp`: `Config.HeadshotMult = GetStat(GBStat::HeadshotMult, 1.f);`.
- Spell projectiles (`Spells/GBSpell_Projectile.*`): read an optional param `HeadshotMult` (default 1 = no bonus) into the config and document it in the header's param list. Do not add it to any spell in `Spells.json`.
- `Glyphbound/Content/Data/Weapons.json`: add `"HeadshotMult": 1.5` to `Staff_Apprentice`, `Staff_Ember` and `ElementalSword`. If the gun header comment lists the stats per weapon type, add `HeadshotMult` to the staff and sword headers' lists too.

## 3. The sword hits where the player aims
Owner: "The sword attack should be able to decide where to attack, not always the center of the target."

`Weapons/GBSword.cpp`, `PerformAttack`: the arc and range selection stays, but the hit location of each target is no longer its center.
- New **pure static function** in `UGBCombatLibrary` (so it can be tested):
  `static FVector ClosestBodyPointToAim(const FVector& AimOrigin, const FVector& AimDirection, float AimLength, const FVector& BodyBottom, const FVector& BodyTop);`
  It returns the point of the segment `BodyBottom..BodyTop` (the target's vertical axis, feet to top of head) that is closest to the aim segment `AimOrigin .. AimOrigin + AimDirection * AimLength` (use `FMath::SegmentDistToSegmentSafe`).
- In the sword: for a target with a capsule (an `ACharacter`, or any actor whose root is a capsule), the body segment is the capsule's axis from the feet to the top; for other targets (ice wall, dummies without a capsule) use the actor's bounds (center X/Y, bottom to top Z). `Info.HitLocation` = that point; `Info.HitDirection` stays the direction from the player to the target. Then call `ApplyWeakPointBonus(Target, Info, GetStat(GBStat::HeadshotMult, 1.f))` before `DealDamage`.
- The "not through walls" trace goes to that hit location instead of the target's center.
- The range check must still reach a target whose body is inside the range sphere; do not make the sword miss targets it hit before.

## 4. Tests (pure logic only, in `Source/Glyphbound/Tests/`, existing files where they fit)
- `Glyphbound.Player.JumpDirection`: no input keeps the velocity; input with a slow current velocity gives the input direction at `MaxWalkSpeed * Mult`; input with a fast current velocity keeps the speed but takes the input direction (also opposite to the current velocity).
- `Glyphbound.Combat.MeleeAimPoint`: aiming level at head height returns a point at head height; aiming down at the feet returns a point near the bottom; aiming far above returns the top (clamped); an aim that passes beside the body still returns the height where it passes closest.
Each test must be able to fail (ask yourself which wrong implementation would still pass). Follow the naming rule (a test name is never a prefix of another).

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 26` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed` and no `LogGB` warning.
Never make an error disappear by deleting behavior. All JSON files stay valid. No gameplay numbers in code.

## Final report (plain text)
1. The four result lines, copied verbatim.
2. What you changed per item, with file names; for item 1, say what `JumpCurrentCount` is inside `OnJumped` for a ground jump and for an air jump, and how you tell them apart.
3. Anything you could not do as described, and why.
4. What a person should check by playing.
