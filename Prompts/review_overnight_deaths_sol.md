# Review: natural deaths for the 3D enemies (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario:
a crash or null dereference, an actor that is never destroyed (leak) or destroyed too early, a tick that keeps running
forever, a physics or sweep call that cannot do what the comment says, math that goes wrong (debris through the floor,
NaN, division by zero), a timer or delegate left bound after destroy, smoke checks that cannot fail or that measure the
wrong thing. No style notes. If nothing, say so.

The change (see `git diff HEAD -- Glyphbound/Source/Glyphbound/AI Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp Glyphbound/Content/Data/Enemies.json`
and the new files `AI/GBDebris.h/.cpp`, `Tests/GBDebrisTests.cpp`):
- `AGBSkeletalEnemy` (humanoids, spider): no ragdoll any more. On death the enemy's own Death clip plays once and holds
  its last frame (`UGBEnemyAnimInstance::PlayDeath`); `TickDeath` eases the body down to the floor (or lets it fall over
  a pit), drops the weapon at `WeaponDropTime` (`OnWeaponDropTime`, `DropStaticPiece`; the shield chief overrides it),
  lets it lie for `CorpseTime`, sinks it and destroys the actor.
- `AGBFlyerEnemy` / `AGBDroneEnemy` / `AGBSentinelEnemy`: a debris simulation of our own (`GBDebris::Step`, pure;
  `FGBDebrisField` moves scene components by sphere sweeps) replaces the simulated-body and capsule-fall branches: the
  wreck falls, at the first ground contact sparks and an explosion, the drone's rotors and eye and the sentinel's core
  fly off as pieces, the wreck sparks for `WreckTime`, sinks, and the actor is destroyed; no ground within
  `DeathMaxFall` destroys it.
- `gb.Smoke Enemies3D`: the ragdoll check replaced by death-clip, pelvis-on-floor, wreck-on-ground and removal checks;
  the flyers' hover check ignores a floor that changed under the flyer in the last 3 s; the tactics group spawns on
  spread bearings after the navmesh is built.
