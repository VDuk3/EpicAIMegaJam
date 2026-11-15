# Overnight block A: natural deaths for the 3D enemies (owner's overnight points 2 and 3)

Read first: `Prompts/implementer_rules.md` (its "Hard rules" apply, except the build rule, see "Phases" below),
`Design/ARCHITECTURE.md`, `Design/GDD.md` §10 ("The future's 3D enemies", "How the future's enemies fight").

## The owner's words (Spanish, translated)
- Point 2: "Today the drone and the sentinel, when they die, stay still for a second and then disappear, which is very
  unnatural. They must fall to the ground and, when they touch it, throw sparks, explode or come apart (or something
  like that). It has to look much more natural."
- Point 3: "No ragdoll on ANY 3D enemy. Each one has its death animation, if possible a different one for each enemy."

## What exists (read these)
- `AI/GBEnemyCharacter.cpp` `HandleDeath`: state Dead, squad leave, movement disabled, `DetachFromControllerPendingDestroy`,
  `SetActorEnableCollision(false)`, drops, `OnDied`, `SetLifeSpan(Game.json CorpseTime, 1.5 s)`.
- `AI/GBSkeletalEnemy.cpp` `HandleDeath` / `StartRagdoll`: ragdoll whenever the mesh has a physics asset (every humanoid);
  the death clip only when it has none (the spider). `UGBEnemyAnimInstance::PlayDeath` (`AI/GBEnemyAnimInstance.cpp`)
  plays the `Anim.Death` clip once and holds its last frame, blended in over `Tuning.BlendTime`.
- `AI/GBShieldEnemy.cpp` `HandleDeath` (the horde chief's shield).
- `AI/GBFlyerEnemy.cpp` `HandleDeath` / `OnDeathFall` / `StartCapsuleFall`, `AI/GBDroneEnemy.cpp` `OnDeathFall`,
  `AI/GBSentinelEnemy.cpp` `OnDeathFall`. **Why the owner sees "frozen, then gone":** the simulated-body branch runs
  only when the part's static mesh has simple collision (the drone and sentinel meshes most likely have none), so the
  capsule branch runs; it sets the CharacterMovement to Falling, but the controller was just detached and a
  CharacterMovementComponent without a controller does not move (`bRunPhysicsWithNoController` is false), so the
  corpse hangs in the air until its 1.5 s life span ends. Confirm this reading in the engine source before you build on it.
- The parts: drone = `BodyPart` (body) with the eye and four rotors attached; sentinel = `ShellPart` + the core.
  Their FX keys come from `Enemies.json` Strings `Fx.*` through `GBEnemy3D` (`AI/GBEnemy3D.cpp`, `SpawnFx`).
- Smoke: `Core/GBDevCommands.cpp`, the `Enemies3D` block (search `E3D`, `Enemies3D`): it checks ragdolls
  ("has a physics asset but did not ragdoll") and that corpses are removed (`E3DCheckCorpses`, threshold on
  `CorpseTime <= 4.5`).

## What to build

### A1. Humanoids and the spider: a death clip, never a ragdoll
- `AGBSkeletalEnemy::HandleDeath`: never ragdoll. Delete `StartRagdoll`, `bRagdoll`, `IsRagdolling` and the ragdoll
  profile constant (update every caller, the smoke included). Always: aim reset, `AnimInst->PlayDeath()`.
- The mesh stays attached to the actor; capsule collision stays off; the body and armor do not collide.
- **The corpse lies on the floor:** at death, trace down from the capsule (WorldStatic) and, if the capsule's bottom
  is above the floor (killed mid-lunge, on a step), move the actor down to it over about 0.2 s (a simple per-tick
  ease, the actor's tick keeps running while dead). Over a pit (no floor within 3 m) let it fall at gravity.
- **The weapon falls from the hand** a moment into the clip (data `WeaponDropTime`, default 0.35 s): if its mesh has
  simple collision, detach and simulate as the ragdoll code did with the weapon (keep that code, moved); if not, it
  stays in the hand. The horde chief's shield likewise in `AGBShieldEnemy::HandleDeath` (same rule).
- **Leaving:** the body lies for `CorpseTime` (per enemy, already in data), then sinks into the floor over
  `CorpseSinkTime` (default 1.5 s) by `CorpseSinkDepth` (default 60 cm), then the actor is destroyed. The life span
  must cover clip + lie + sink. No pop-out.
- **One clip per enemy, each different.** New clips exist now (retargeted tonight, same names on each skeleton):
  `/Game/Glyphbound/Characters/<Orc|Robot|Valkyrie>/Anims/A_<C>_Death_<Front_01|Front_02|Front_03|Back_01|Left_01|Right_01>`.
  Set `Anim.Death` in `Content/Data/Enemies.json`:
  Orc_Future `A_Orc_Death_Front_01`, Orc_Future_GunCarrier `A_Orc_Death_Back_01`, Orc_Future_Enforcer `A_Orc_Death_Left_01`,
  Orc_Future_Siege `A_Orc_Death_Front_03`, Orc_Future_Warlord `A_Orc_Death_Front_02`, Robot_Future `A_Robot_Death_Right_01`,
  Valkyrie_Future `A_Valkyrie_Death_Back_01`; Spider_Future keeps `A_Spider_Death`.
- Check `UGBEnemyAnimInstance`: the death pose must really be the clip's last frame held (no blend back to idle, no
  aim offset, no hit overlay on top after death) and the clip must play in place the way the other clips do (see how
  the locomotion clips cancel pelvis travel; a death clip that carries the pelvis forward is fine visually, but the
  mesh must not snap back at the end).

### A2. Drone and sentinel: fall, hit the ground, come apart
Write a small debris simulation of our own (no physics engine: the meshes have no collision, and simulated bodies were
the branch that never ran). New files `AI/GBDebris.h/.cpp`:
- A pure step function (testable): `GBDebris::Step(FGBDebrisState&, float Dt, const FGBDebrisParams&, const FHitLike* Hit)`
  or similar: velocity += gravity * dt; on a hit, reflect the velocity on the hit normal with restitution (data,
  default 0.35) and tangential friction (default 0.6); spin damped on each bounce; at rest when the speed after a bounce
  is under a threshold (default 60 cm/s) and the normal points up.
- A component or plain helper the flyers tick: each piece = a scene component (static mesh) moved by a sphere sweep
  (`World->SweepSingleByChannel`, ECC_WorldStatic, ignore pawns and the enemy itself) from its position to the next.
- **Drone:** on death the body keeps the drone's velocity plus the killing blow's push (data `DeathImpulse`), tumbles
  (`DeathSpin`), the rotors spin down. **First ground contact:** a burst of sparks and a small explosion with a flash
  (FX from `Enemies.json` Strings, new keys `Fx.DeathSparks`, `Fx.DeathExplosion`; pick fitting systems from
  `/Game/SciFi_Impacts/FX/` and `/Game/GunFX/` (look at what is there: `ls Glyphbound/Content/SciFi_Impacts/FX`) and a
  short point light), and it **comes apart**: the four rotors and the eye detach and fly off as their own debris pieces
  (random outward and up velocities, data `BreakApartSpeed`), the body bounces once or twice and rests. While it lies
  there it spits a few sparks (repeat `Fx.DeathSparks` small, every 0.4 to 1 s) for `WreckTime` (default 6 s), then
  every piece sinks 30 cm over 1 s and the actor is destroyed.
- **Sentinel:** same fall; at the first contact the shell cracks open: the core pops out (it is its own mesh) and
  rolls/bounces away with its glow dimming to off over 1.5 s (it already has a glow parameter: `Fx.GlowParam`), sparks
  and an energy burst (`Fx.DeathExplosion` for it can be a blue energy impact), the shell bounces and rests.
- No ground within `DeathMaxFall` (default 4 s of falling): destroy (fell into a pit).
- Remove the old simulated-mesh branches and `StartCapsuleFall`/`bTumbling` if nothing else needs them.
- The actor's life span: no fixed 1.5 s while the debris runs; it destroys itself when the wreck time ends.
- Data: every new number in `Enemies.json` on `Drone_Future` and `Sentinel_Future` Stats (names above), with defaults in code.

### A3. Tests and smoke
- One test, `Glyphbound.Debris.Bounce` in `Tests/GBDebrisTests.cpp`: a piece dropped on a flat floor bounces lower
  each time and comes to rest on the floor (not under it, not floating), within a bounded number of steps; a piece
  thrown sideways at a wall comes back. Make sure a wrong reflection (sign error, no restitution) fails it.
- `gb.Smoke Enemies3D` (`Core/GBDevCommands.cpp`): replace the ragdoll check with: the death clip is playing on every
  skeletal enemy that has one, and after the clip the mesh's pelvis bone is within 60 cm of the floor under the actor
  (corpse on the floor). For flyers: after death, the debris reached the ground (a log line per flyer with its fall
  time, bounce count, pieces) and the wreck is gone after its wreck time. Update `E3DCheckCorpses` and the step times
  so the waits match the new corpse times (clip + lie + sink, wreck time + sink). Keep the block's total time sane.
- **Two flaky steps you must fix (CLAUDE.md: a red test is fixed or deleted the same day):** in this morning's full
  smoke, `Enemies3D` failed twice and passed when rerun alone: (1) flyers: `Drone_Future` 2 cm above the ground
  (wanted 236) at the hover check, also red on Nov 14; (2) tactics: two members within 200 cm for 4.2 s (limit 3.5).
  Logs: `Glyphbound/Saved/Logs/smoke_20261115-074654.log` (red) and `smoke_20261115-075750.log` (alone, green).
  Find the cause (for example: in the full run the drone starts before the navmesh or the floor is ready, or the
  earlier steps leave something on the floor; the tactics squad spawns from too few points) and fix the cause or make
  the check measure what it means. Do not just loosen a limit without saying why it is right.

## Files you own
`AI/GBSkeletalEnemy.*`, `AI/GBShieldEnemy.*`, `AI/GBEnemyAnimInstance.*` (death only), `AI/GBFlyerEnemy.*`,
`AI/GBDroneEnemy.*`, `AI/GBSentinelEnemy.*`, new `AI/GBDebris.*`, new `Tests/GBDebrisTests.cpp`,
`Core/GBDevCommands.cpp` (only the Enemies3D block), `Content/Data/Enemies.json` (only the 3D future enemies' death
and debris keys). Another implementer may soon work on the Valkyrie's fight in a NEW class derived from
`AGBSkeletalEnemy`: keep `AGBSkeletalEnemy`'s public and protected API stable apart from the ragdoll removal.

## Phases
1. **Now: write the code, do not build.** The Unreal Editor is open (the architect is importing assets), so a build
   would fail. Check every engine API in `C:\Program Files\Epic Games\UE_5.9\Engine\Source`. Then send your report
   and stop.
2. **When the architect messages you "build now":** take the machine lock (`Glyphbound/Saved/machine.lock`, one line:
   who, what, UTC time; delete it when done), then iterate cheaply (owner's rule, in every brief): Editor target only,
   `powershell -File Tools/Jam/Build-Jam.ps1`; tests `powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.Debris.'`;
   smoke `powershell -File Tools/Jam/Smoke-Jam.ps1 -Only Enemies3D -Offscreen` (about a minute). No full smoke, no
   game-target build, no package: the architect runs those once at the end. Look at the smoke screenshots of the deaths
   (`Glyphbound/Saved/Screenshots/Smoke`) yourself. Run the Enemies3D step three times in a row at the end to show
   the flaky steps are gone. Do not commit.
