# Task: climb ledges in any direction, not only the one the view faces — Glyphbound (UE 5.9, C++)

You are the implementer. Read `C:\GameJam\CLAUDE.md` (rules). Do NOT edit `Design/GDD.md` or the dev log. Do not commit.

## Owner's request (translated from Spanish)
"Make it possible to climb without looking at the wall, for example with your back to it."

## Current state (commit 9bf924a, read it first)
- `Player/GBMantle.h/.cpp`: pure climb math (tested by `Glyphbound.Player.MantleReach` / `MantlePath` in `Tests/GBPlayerTests.cpp`).
- `Player/GBPlayerCharacter.cpp`: `FindMantleLedge(Out, bFromGround)` sweeps the capsule along the view's yaw forward and
  requires `dot(Forward2D, -WallNormal2D) >= MantleFacingMinDot`; `PressJump()` climbs on the ground; `Tick` grabs in the air
  (`Approach >= -50` check); `StartMantle`, `TickMantle`, `CancelMantle`, `ApplyCameraPose` (view offsets via the camera's
  additive offset), `LastMoveInput` = the held movement input (x = right, y = forward), `GetMoveInputWorldDirection`.
- `UI/GBHUD.cpp` `DrawMantleHands`: the hands grip where the ledge projects on screen.
- Smoke block `mantle:` in `Core/GBDevCommands.cpp`.

## Design (architect, provisional)
### Which directions are searched (probe directions)
The wall is no longer looked for along the view. `FindMantleLedge` takes a probe direction (2D unit vector) and checks
`dot(ProbeDir, -WallNormal2D) >= MantleApproachMinDot` (the sweep must meet the wall roughly head-on) instead of the view.
A new pure function picks the ordered list of directions to try:
```
/** Directions to probe for a ledge, in priority order (2D unit vectors).
    - Intent given (non-zero IntentDir): only that direction (the player chose where to go).
    - No intent: 8 directions 45 degrees apart, starting with ViewForward, then +-45, +-90, +-135, 180
      (positive = counterclockwise first), so the nearest to the view wins. */
void GetProbeDirections(const FVector2D& ViewForward, const FVector2D& IntentDir, TArray<FVector2D>& Out);
```
The first direction that finds a valid ledge wins.
- **Jump pressed on the ground:** intent = the held movement input's world direction (`GetMoveInputWorldDirection(LastMoveInput)`).
  So: standing still with the back to a wall + jump = climb it; holding S (backwards) into a wall + jump = climb it; holding
  W away from a wall behind + jump = a normal forward jump (never a climb behind the player).
- **In the air:** intent = the horizontal velocity direction when its speed is at least `MantleAirSearchMinSpeed`
  (jumps commit to their direction, so the velocity is the intent); slower (a vertical jump, or after hitting a wall, which
  stops the velocity into it) = no intent, all 8 directions. The existing "not moving away" check (`Approach >= -50`) stays,
  against the found wall.
- Cost: the 8-direction search runs only when there is no intent; keep the cheap checks first (the wall sweep) and stop at the
  first hit. No per-tick allocations (use a `TArray` with inline allocator or a fixed array).

### The view turns to the wall
When the climb starts, if the wall faces away from the view by more than `MantleTurnMinAngle` degrees (angle between the
view's yaw forward and -WallNormal2D), the view's yaw turns to face the wall during the climb, so the player ends up on
top looking across it and the hands grip a ledge that is on screen:
- Total turn = the signed shortest yaw delta from the view yaw to the wall-facing yaw (-WallNormal2D), computed at start.
  (No turn when the angle is at most `MantleTurnMinAngle`.)
- Pure: `float TurnFraction(float Alpha, float RiseShare)` = smoothstep from 0 at Alpha 0 to 1 at Alpha = RiseShare, 1 after.
- Each mantle tick, add `(TurnFraction(Alpha) - TurnFraction(PrevAlpha)) * TotalTurn` to the control rotation's yaw
  (deltas, so the mouse still works during the climb and both add up). Only yaw; pitch untouched. Cancel stops turning.
- `CanMantleNow`, inputs, cancel paths, camera dip, held item and HUD hands stay as they are.

### Data (`Content/Data/Player.json` "Stats", constants in `Core/GBStatNames.h`)
- Rename `MantleFacingMinDot` -> `MantleApproachMinDot` (value 0.5; comment: the probe direction must meet the wall at most
  60 degrees off its normal). Remove the old name everywhere.
- New `MantleAirSearchMinSpeed` = 150 (cm/s: in the air, slower than this searches all around).
- New `MantleTurnMinAngle` = 60 (degrees: a wall further off the view than this turns the view to it during the climb).

### Tests (pure, `Tests/GBPlayerTests.cpp`; new test `Glyphbound.Player.MantleProbe`)
- With intent: exactly one direction, equal to the normalized intent (also when the intent is not normalized).
- Without intent: 8 directions, all unit length, the first equals ViewForward, the last is -ViewForward, all 8 distinct and
  45 degrees apart as a set (each one's nearest neighbour is 45 degrees away), the second and third are +45 and -45.
- A different ViewForward rotates the whole list (not fixed world axes).
- `TurnFraction`: 0 at 0, 1 at RiseShare and at 1, strictly between at RiseShare / 2, non-decreasing over 50 samples, and
  for two different RiseShares the value at 0.3 differs.
Each assertion must be able to fail for a wrong implementation.

### Smoke (`Core/GBDevCommands.cpp`, names starting with `mantle:`; add before the "back to the arena start" step,
shift later steps and `quit` so nothing overlaps)
1. `mantle: back to the 200 cm wall, standing jump press` - same spot as the existing ground step
   (X -2500, Y = -1100 + R + 5, on the floor) but yaw +90 (back to the wall), velocity zero, `LastMoveInput` empty
   (it is protected: call `ReleaseHeldInputs()` or add nothing; check it is zero), then `PressJump()`. Log
   `SMOKE mantle back started=%d`; fail if not started.
2. `mantle: back climb result` - after 1 simulated second: feet within 10 cm of 200, Y < -1100, not mantling, walking,
   and the control yaw within 15 degrees of -90 (the view turned to the wall). Log and fail.
3. `mantle: back to the platform, air grab` - in the air beside the platform's west face (X = 1800 - R - 5, Y 2600,
   feet 120 cm under the top 600), yaw 180 (back to the platform), MOVE_Falling, velocity zero. After 1 simulated second:
   feet within 10 cm of 600, X > 1800, walking, control yaw within 15 degrees of 0. Log and fail.
4. `mantle: holding away from a wall behind is a jump` - needs a held move input: add a small public test hook on the
   player, `void SetMoveInputForTest(const FVector2D& Input)` (sets `LastMoveInput` and `LastMoveInputFrame = GFrameCounter`),
   used only by the smoke. Spot of step 1, yaw +90 (back to the wall), input (0, 1) = forward (away from the wall),
   `PressJump()`: must NOT be mantling. Log and fail.
Keep the existing `mantle:` steps working (they face the wall; with no input they still find it first).

## Rules
- **Iterate cheaply** (owner's rule): compile only the Editor target (`powershell -File Tools/Jam/Build-Jam.ps1`), run only
  `powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.Player.'` and
  `powershell -File Tools/Jam/Smoke-Jam.ps1 -Only mantle -Offscreen`. Do NOT run the full smoke, the game target or the
  full suite: the architect runs them once at the end. One long command at a time.
- Before any build or game run check `Glyphbound/Saved/machine.lock`: younger than 45 minutes = another chat is building,
  wait. While you build/run write `muse-mantle | build | <UTC>` there and delete it after.
- Never launch Play In Editor, never open or close the Unreal Editor.
- English comments and logs, existing style. Do not commit.
- Report: files changed, build, tests, the `SMOKE mantle` lines.
