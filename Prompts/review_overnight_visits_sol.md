# Review: map visit smokes, fog debug API, projectile and archer fixes (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario:
a crash or null dereference, an actor or light left alive, a ticker or delegate left bound after the run, a debug path
that changes normal gameplay, a smoke check that cannot fail or measures the wrong thing, a state that gets stuck.
No style notes. If nothing, say so.

The change (`git diff HEAD~1 HEAD -- Glyphbound/Source`):
- `World/GBFluidFog.*`: debug-only `DebugInject`, `DebugForceOff`, `DebugBlast` (FEntry got a Density field; normal
  entries must still be written with density 0) and bodies' divergence scaled by speed.
- `Core/GBDevCommands.cpp`: the `FogVisit` block (namespace FogVisitRun, runs only in a FogVisit-filtered run on a map
  with fog boxes) and the `CastleVisit` block (runs only on the castle map; holds the smoke step clock with
  bHoldClock/HoldElapsed while a visit runs).
- `Combat/GBProjectile.cpp`: the flight clip hides within 1.5 m of the camera.
- `AI/GBSkeletalEnemy.cpp`: an archer with FiringSpotByRadius that stalls within 2 m of its firing spot and sees the
  player shoots from where it stands after 0.8 s.
