# Task: ledge climb (mantle) for the player — Glyphbound (UE 5.9, C++)

You are the implementer. The architect (Claude Code) designed this; implement it exactly, in C++, in the project at
`C:\GameJam\Glyphbound` (module `Glyphbound/Source/Glyphbound`). Read `C:\GameJam\CLAUDE.md` first (rules), but you
do NOT edit `Design/GDD.md` or the dev log: the architect does that.

## What the owner asked (translated from Spanish)
"When the character jumps and ends up at the edge of a structure, it should play the climb animation and climb up
that structure. Also, when the character is right against a block/structure and presses jump, if the jump is high
enough, it should play the climb animation and climb that structure/block instead of jumping."

The game is first person (camera only, no body mesh). The player's hands are 2D pixel-art sprites drawn by the HUD
(`UI/GBHUD.cpp`, `AGBHUD::DrawHands`), guns are 3D models attached to `HeldItemAnchor` under the camera. So the
"climb animation" is a first-person one: camera motion + the pixel hands gripping the ledge + the held item lowered.

## Design (architect, provisional; numbers are data)

### New stats in `Glyphbound/Content/Data/Player.json` "Stats" (add constants in `Core/GBStatNames.h`, `GBStat::`)
| Stat | Value | Meaning |
|---|---|---|
| MantleMaxReach | 160 | cm: the highest a ledge top can be above the feet for the hands to reach it |
| MantleMinHeight | 50 | cm: from the ground, a ledge lower than this above the feet is not climbed (it is jumped / stepped) |
| MantleAirMinHeight | 15 | cm: in the air, a ledge top less than this above the feet is not climbed (the player just lands) |
| MantleProbeDistance | 40 | cm: how far in front of the capsule a wall is looked for |
| MantleFacingMinDot | 0.5 | the view's yaw forward must face the wall: dot(forward2D, -wallNormal2D) >= this |
| MantleTopInset | 10 | cm: the capsule ends this far past the ledge edge (its edge, not its centre: centre = edge + radius + inset) |
| MantleBaseDuration | 0.22 | s |
| MantleDurationPerMeter | 0.18 | s per metre of rise (duration = base + per metre * rise / 100) |
| MantleRiseShare | 0.6 | share of the duration spent rising straight up; the rest moves forward onto the top |

Read them with `ReadStat` like every other movement stat (they then work with modifiers and missing-stat warnings).

### Pure logic (new files `Player/GBMantle.h` / `.cpp`, namespace `GBMantle`, no UObject, tested)
```
struct FMantleParams { float MaxReach, MinHeight, AirMinHeight, BaseDuration, DurationPerMeter, RiseShare; };
/** One jump's rise: v^2 / (2 g), g = |GravityZ| * GravityScale (guard g <= 0 -> 0). */
float JumpRise(float JumpZVelocity, float Gravity);
/** From the ground, on a jump press: MinHeight <= LedgeHeight <= JumpRise + MaxReach. */
bool CanMantleFromGround(float LedgeHeight, float JumpRiseCm, const FMantleParams&);
/** In the air: AirMinHeight <= LedgeHeight <= MaxReach. */
bool CanMantleFromAir(float LedgeHeight, const FMantleParams&);
float Duration(float RiseCm, const FMantleParams&);   // >= 0.05 s
/** Actor location along the mantle at Alpha in [0,1]: first (0..RiseShare) straight up from Start to (Start.XY, End.Z)
    with an ease-out, then (RiseShare..1) horizontally to End with a smoothstep. Alpha clamped. Exactly Start at 0, End at 1. */
FVector EvaluatePath(const FVector& Start, const FVector& End, float Alpha, float RiseShare);
/** View pose weights at Alpha (all exactly 0 at Alpha 0 and at Alpha 1):
    ItemLower01: 0 -> 1 over the first 20%, 1 until 75%, 1 -> 0 by 100% (smoothstep each ramp).
    Grip01 (pixel hands on the ledge): 0 -> 1 over 0..15%, 1 until 70%, 1 -> 0 by 90%, 0 after.
    CameraDip01: a smooth bump, 0 at 0, peak 1 at RiseShare (going over the edge), 0 at 1 (e.g. sin-based or two smoothsteps). */
struct FMantlePose { float ItemLower01, Grip01, CameraDip01; };
FMantlePose EvaluatePose(float Alpha, float RiseShare);
```

### Ledge detection (in `AGBPlayerCharacter`, e.g. `bool FindMantleLedge(FMantleLedge& Out) const`)
Using the capsule's world radius R and half height HH, Feet = location.Z - HH, Forward = yaw-only control forward:
1. **Wall:** sweep the player capsule (shrunk by 2 cm in radius) forward from the actor location by MantleProbeDistance
   on `ECC_Pawn` with the capsule's collision response params, ignoring self. Need a blocking hit, not start-penetrating,
   whose actor is not an `APawn` (never climb enemies), and whose `ImpactNormal` is near-horizontal (`|Normal.Z| < 0.3`).
   WallNormal2D = normalized XY of ImpactNormal. Require dot(Forward, -WallNormal2D) >= MantleFacingMinDot.
2. **Top:** sphere sweep (radius 8 cm) straight down at XY = ImpactPoint.XY - WallNormal2D * (MantleTopInset + 8),
   from Z = Feet + (max allowed height for this case) + 20 down to Z = Feet + (min height for this case) - 5. Need a
   blocking non-pawn hit, not start-penetrating, with `ImpactNormal.Z >= GetCharacterMovement()->GetWalkableFloorZ()`.
   LedgeTopZ = hit ImpactPoint.Z; LedgeHeight = LedgeTopZ - Feet. It must pass `CanMantleFromGround` / `CanMantleFromAir`.
3. **Room:** End = (ImpactPoint.XY - WallNormal2D * (R + MantleTopInset), LedgeTopZ + HH + 2). The capsule must fit at End
   and at Mid = (Start.XY, End.Z) (head room above the player): two `OverlapBlockingTestByChannel` with the capsule shape
   (same channel and response params), ignoring self. Either blocked -> no mantle.
4. Output: Start (actor location), End, the ledge edge point (ImpactPoint.XY, LedgeTopZ) for the HUD, the rise (End.Z - Start.Z).

### When it triggers
- **Jump pressed on the ground** (`IsMovingOnGround()`), in `InputJump` before `Jump()`: if a ledge is found with the
  ground limits (max = `JumpRise(JumpZVelocity, |GravityZ|*GravityScale)` + MantleMaxReach), start the mantle and do NOT jump.
  Otherwise the normal jump. (A ledge too high for one jump is a normal jump; the air rule below may still catch it,
  e.g. with the double jump.)
- **In the air** (`MOVE_Falling`, not dashing): every Tick, if a ledge is found with the air limits and the horizontal
  velocity is not moving away from the wall (dot(Velocity2D, -WallNormal2D) >= -50 cm/s), start the mantle. This is the
  "jumps and ends up at the edge" case, rising or falling.
- Never while: dead, control disabled, QTE active, dashing, movement frozen, already mantling.
- Make the jump press reachable by the smoke run: move the body of `InputJump` into a new public `void PressJump()` and
  have `InputJump` call it (same gates as today, plus "not mantling").

### During the mantle
- Start: `ReleaseHeldInputs()`, end any held primary/secondary as that already does; `StopMovementImmediately()`;
  movement mode `MOVE_Flying` keeping `JumpCurrentCount` (same trick as the dash). Store Start, End, ledge point, Duration, Elapsed.
- Tick: Elapsed += DeltaSeconds; `SetActorLocation(GBMantle::EvaluatePath(...))` (no sweep: the path was validated);
  velocity kept at zero. At Alpha >= 1: place at End, `SetMovementMode(MOVE_Walking)`, velocity zero, end.
- Blocked during the mantle: movement input (`ApplyMoveInput` / the QTE held-move path), jump, dash, primary, secondary,
  spell slots, reload, enchant, quick use, block, interact. Look (mouse) stays free.
- `CancelMantle()` (to Falling, everything visual back to rest) from: `HandleDeath`, `FreezeMovement`, `RespawnAt`,
  `LoadFromSnapshot`, `SetControlEnabled(false)`, `EndPlay`.
- Public read-only API for the HUD and the smoke run: `bool IsMantling() const`, `float GetMantleAlpha() const`,
  `FVector GetMantleLedgePoint() const`, `GBMantle::FMantlePose GetMantlePose() const` (zero pose when not mantling).

### The first-person animation
1. **Camera (view only, aim untouched):** pitch down by `CameraDip01 * 10` degrees and roll by `CameraDip01 * 3` degrees,
   plus a relative location drop of `Grip01 * 6` cm (the pull on the arms). Compose it with the
   existing view shake: `TickViewShake` must not overwrite it, and neither may leave a residue. Do it with one place that
   sets `Camera->SetRelativeRotation(Shake + Mantle)` and `Camera->SetRelativeLocation(Base eye + mantle offset)`, and
   returns exactly to zero / the base eye height when both are done (the base eye height comes from `BaseEyeHeight`).
   Keep these three numbers as named constants in the file's anonymous namespace ("feel and visual constants").
2. **Held item:** `HeldItemAnchor` relative location = `HeldItemOffset + FVector(-10, 0, -35) * ItemLower01` (the gun
   drops out of view), exactly `HeldItemOffset` at rest.
3. **Pixel hands (`AGBHUD::DrawHands`):** while mantling,
   - the normal pose (staff / gun hands / sword / bare hand / left hand / cast hand / shield) slides down off screen:
     add `ItemLower01 * Canvas->ClipY * 0.7` to the baseline `H` they are all placed from, so all of them go down together;
   - the two climbing hands: `hand_left` and `hand_right` sprites (existing) gripping the ledge. Project the ledge point
     to the screen (`Canvas->Project` or the player controller's `ProjectWorldLocationToScreen`); grip line Y = projected Y
     clamped to [0.12, 0.85] * ClipY (if the projection fails, 0.45 * ClipY). Hand centres at X = 0.38 W and 0.62 W. The
     knuckles (top of the sprite) sit on the grip line when Grip01 = 1; with Grip01 < 1 the hands are lowered by
     `(1 - Grip01) * (ClipY - GripY + sprite height)` (they come up from below the screen and go back down).
   - **Sleeves:** below each climbing hand, draw the sprite's cuff rows (the bottom 8 rows of the 20-row sprite: the
     purple `m` sleeve) repeated downward to the bottom of the screen, as a tile of the same texture with a UV sub-rect
     (nearest filtering, same pixel size P), so the arms reach up from the bottom edge. Look at how `DrawSprite` draws and
     add a small helper for the sub-rect tile.
   - Skip the cast hand and the raised shield while mantling.

### Test arena (`World/GBTestArena.cpp`): keep its gates honest with the new reach
The course and the platform were sized for jumps only; climbing adds `MantleMaxReach` (read it from Player.json like the
other stats there, default 160).
- The last course step (today `DoubleJumpStep = 1.5` jump rises) must need the double jump even with a climb:
  its rise = `1.5 * JumpRise + MantleMaxReach` (halfway between "one jump + climb" and "double jump + climb").
- `WallHeight`: 1100 -> 1400, and the warning check becomes `Top + 2 * JumpRise + MantleMaxReach > WallHeight - WallClearance`.
- New warning: if `2 * JumpRise + MantleMaxReach > PlatformTop - 20`, log that the past-era raised platform (future-only ramp) can be reached by climbing.
- Log the climb reach in the existing course `Display` line.

### Tests (pure logic only; file `Tests/GBPlayerTests.cpp`, flags like the existing ones)
- `Glyphbound.Player.MantleReach`: `CanMantleFromGround` true at MinHeight and at JumpRise + MaxReach, false just below
  MinHeight and just above JumpRise + MaxReach; `CanMantleFromAir` limits likewise; `JumpRise(800, 1568)` ~ 204.08.
- `Glyphbound.Player.MantlePath`: `EvaluatePath` exactly Start at 0 and End at 1; at RiseShare XY == Start.XY and Z == End.Z;
  Z never decreases over 50 samples; XY does not move before RiseShare; all `EvaluatePose` weights are 0 at 0 and at 1,
  `ItemLower01` is 1 at 0.5, `CameraDip01` peaks (max over samples) at RiseShare +- 0.05.
Each assertion must be able to fail for a wrong implementation (no constant copies). Do not name a test so that it is
also a folder of another test.

### Smoke steps (`Core/GBDevCommands.cpp`, names starting with `mantle:` so `gb.Smoke mantle` runs only them)
Add a block before the `quit` step (move later steps / quit as needed so nothing overlaps; look at how other blocks use
`MarkSim` and `MinSimSinceMark`). Use the arena's path-test wall (centre (-2500, -1150, 100), size (600, 100, 200):
its north face is at Y = -1100, top at Z = 200) and the raised platform (centre (2300, 2300), 1000 x 1000, top 600,
west face at X = 1800).
1. `mantle: ground press next to the 200 cm wall` - teleport the player on the floor at (-2500, -1100 + R + 5), yaw -90
   (facing the wall), velocity zero, then `PressJump()`. Log `SMOKE mantle ground started=%d` (IsMantling right after);
   fail (`UE_LOG(LogGB, Error, ...)`) if not started.
2. `mantle: screenshot mid-climb` - about 35% into the climb: `Shot(TEXT("mantle_grip"))` (the hands on the ledge).
3. `mantle: ground result` - 1 simulated second later: feet Z within 10 cm of 200 and Y < -1100 (on top of the wall),
   not mantling, camera relative rotation ~0 and `HeldItemAnchor` back at rest. Log and fail like above.
4. `mantle: wall too high is a normal jump` - teleport next to the platform's west face on the floor (X = 1800 - R - 5,
   Y = 2600, yaw 0), `PressJump()`: must NOT be mantling right after (600 cm is out of one jump + reach) and must be
   falling/jumping. Log and fail.
5. `mantle: air grab at the platform edge` - teleport into the air beside the platform's west face with the feet 120 cm
   below its top (centre Z = 600 - 120 + HH), yaw 0, `MOVE_Falling`, velocity zero. After 1 simulated second: feet within
   10 cm of 600 and X > 1800 (on the platform). Log and fail.
6. Put the player back where the next steps expect it (look at what the step after your block assumes), or at the
   arena start if nothing after depends on it.

## Rules
- **Iterate cheaply** (owner's rule, put in every brief): while working, compile only the Editor target
  (`powershell -File Tools/Jam/Build-Jam.ps1`), run only the anchored tests you touched
  (`powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.Player.'`) and only your smoke steps without a window
  (`powershell -File Tools/Jam/Smoke-Jam.ps1 -Only mantle -Offscreen`). Do NOT run the full smoke, the game target or the
  full suite: the architect runs them once at the end. Long commands one at a time.
- Before any build or game run, check `Glyphbound/Saved/machine.lock` ("who | what | UTC time"): if it exists and is
  younger than 45 minutes, another chat is building: wait and retry. While you build / run, write your own lock
  (`muse-mantle | build | <UTC>`) and delete it after.
- Never launch Play In Editor. Never open or close the Unreal Editor.
- Code comments and log text in English. Keep the existing style (tabs, `UE_LOG(LogGB, ...)`, `GB` prefixes).
- Do not commit; the architect reviews and commits.
- When done, report: files changed, build result, test result, the `SMOKE mantle` lines from the filtered smoke, and the
  path of the `mantle_grip` screenshot.
