# Code review: ledge climb in any direction (Glyphbound, UE 5.9 C++)

Review the uncommitted changes (`git diff`, plus untracked files under `Glyphbound/Source/Glyphbound`). Read-only.
Spec: `Prompts/mantle_any_direction.md`; previous climb spec: `Prompts/mantle_climb.md`; rule: `Design/GDD.md` §18 "Ledge climb".

Look for concrete defects, each with a failure scenario:
- Probe direction choice: a held direction away from a wall behind must never climb backwards; standing still with the back
  to a wall must climb; in the air, intent from velocity vs the all-around search; a wall brushed sideways while moving fast.
- The approach dot now uses the probe direction: false positives on corners, pillars, slopes; picking a worse wall first.
- The view turn: signed shortest delta, wrap-around at +-180, deltas applied per tick (mouse still works), no turn after
  cancel, no residue, pitch untouched, aim consistent.
- Performance: the 8-direction search per tick in the air, allocations per tick.
- Data rename (`MantleApproachMinDot`, new `MantleAirSearchMinSpeed`, `MantleTurnMinAngle`): old name fully gone, read with ReadStat.
- The new test `Glyphbound.Player.MantleProbe`: could a wrong implementation pass it?
- Smoke `mantle:` steps (including the new test hook `SetMoveInputForTest`): do they check what they claim; ordering/timing.

Report findings ranked: [P1/P2/P3] title — file:line — failure scenario — fix. If none, say so.
