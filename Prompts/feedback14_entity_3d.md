# Task: the Entity must read as a 3D AAA cyberpunk boss, not a sprite

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first. Hard rules for this task:
- One build or run at a time.
- Do not open the Unreal Editor and do not launch PIE.
- Do not commit.
- Do not run `Tools/Jam/Import-*.ps1`, and do not touch `Content/` except `Content/Data/*.json`.
- **If a problem's cause is outside the files you may edit, stop and report it.** Do not loop on it. The previous run on this enemy spent over an hour on such a problem.
- Other chats share this working tree. If the build fails in a file you did not touch, wait a minute and retry. Do not edit their files.

## Context
The Entity (`AI/GBEntity.h/.cpp`, `AI/GBTentacleRigComponent.*`, `AI/GBTentacleMath.*`, commit 1517e72) is the final boss: a floating metal shell with a big glowing eye and ten green hologram tentacles, animated procedurally. It is a cyberpunk machine from the future.

The owner played it (`gb.SpawnEntity`) and said, translated: "It works like a 2D entity, not at all like 3D. It has to work like a typical triple-A made in Unreal Engine... zero sprite, zero 2D." They confirmed three faults:
1. **Flat shots.** The bolts are small glowing spheres, and they look like 2D sprites.
2. **It always faces you.** It snaps its yaw to the player every frame, like a sprite (`AGBEnemyCharacter::FaceTowards` calls `SetActorRotation`). It never pitches.
3. **It moves flat, with no flight.** It slides at a fixed height and never tilts. Today the capsule stays on the ground and the visual is raised, with a 4 cm bob.

## What to build (all in the Entity's files unless stated)

### A. Turning with weight, and a 3D aim
- **No yaw snapping.** Turn the actor (or the visual root) towards the player at a limited rate with easing: a spring, or a max rate of about 90 deg/s.
  - You may make `FaceTowards` virtual in `GBEnemyCharacter.h` and override it. That one-word change is the only edit allowed in that file.
- **The whole body aims in 3D.** Pitch the visual so the eye points at the player's head, clamped to about ±30 degrees and smoothed.
- **The eye tracks first, the body follows:** the shell lags behind the aim a little.

### B. Real 3D flight
- **Altitude changes.** The shell's height above the floor drifts between about 2.5 m and 4.5 m, using smooth noise or sines, not a constant bob. It also rises before an attack and dips after one. Keep the tentacle tips off the floor.
- **It banks and pitches into its motion,** like a drone or helicopter:
  - it rolls into lateral velocity and turns;
  - it pitches forward when accelerating and back when braking;
  - all of it is spring-smoothed and clamped to about 20 degrees.
- **It moves sideways like a hunter.** When it is in attack range and has line of sight, it strafes or orbits around the player and changes direction now and then, instead of standing still. Reuse the existing movement and pathing; you choose how.
- **Hits knock it back.** Each hit pushes the visual back and tilts it on a spring, scaled by damage. A weak-point hit is a bigger stagger.
- **Collision must follow the visual.** If the shell floats above the capsule, shots must still hit the shell and the eye where they are drawn. Today the capsule is the only collision, and the weak point is an eye sphere tested on hit location. Make sure:
  - a shot at the drawn eye is a weak-point hit;
  - a shot at the drawn shell is a body hit;
  - shots through empty space under it miss.
  Pick the simplest correct way, for example a sphere on the shell that blocks the weapon trace channel. Say in the report what you chose.

### C. 3D energy bolts
- The Entity's shots must read as **3D energy bolts**, not dots:
  - an elongated glowing core oriented along the velocity, about 60–90 cm long;
  - a real `UPointLightComponent` on the bolt, in the projectile colour, lighting the walls and floor as it flies;
  - a short fading trail, or a stretched second mesh.
- Each shot also gets a muzzle flash at the tentacle tip: a brief point light and a flash on the tentacle.
- `Combat/GBProjectile.h/.cpp` may get **additive** options for this, for example a config flag for an energy bolt with stretch and light. Every existing projectile must look and behave exactly as before.

### D. The eye is a real light
- Put a `UPointLightComponent` at the eye, in the eye's colour, with shadows off for cost. It lights the environment in front of the Entity and pulses with the body glow.
- It flares on the windup and on weak-point hits, and fades out on death.

### Numbers
Every tuning number goes in `Game.json` "Values", with a default in code and the prefix `Entity`, as the tentacle values already are. List them in the report.

## Smoke (extend the existing Entity step in `gb.Smoke`)
- Log `SMOKE entity turn max_rate=%.0f`. Put the player behind the Entity, then measure the largest yaw change per second over 0.5 s. It must be ≤ 120.
- Log `SMOKE entity flight z_range=%.0f roll_max=%.1f`. Over 2 s, the shell's height range must be > 20 cm. While moving the Entity sideways (set a velocity or a move target), the largest roll must be > 3 degrees.
- Log `SMOKE entity bolt light=%d length=%.0f` for a fired bolt: light 1, length ≥ 50.
- Log `SMOKE entity hit eye=%d shell=%d below=%d`, from line traces on the weapon channel:
  - at the drawn eye, 1 (weak point);
  - at the drawn shell top, 1 (body hit, not weak point);
  - 1.5 m below the shell, between the tentacles, 0.
- Take two screenshots:
  - `15a_entity`: front three-quarter view, the whole Entity in frame;
  - `15b_entity_side`: from the side, during a strafe, so the bank is visible, with a bolt in flight if possible.
- Keep the existing `SMOKE entity` lines passing. The step still destroys the Entity at the end.

## Tests
No new tests are required. If you add a pure function (for example a bank or flight helper), add at most one test for it, named under `Glyphbound.Entity.` and not a prefix of an existing name.

## Verification (all must be true when you finish)
1. Build the Editor target: `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` gives `Result: Succeeded`.
2. Build the game target: `... Build-Jam.ps1 -Target Glyphbound` gives `Result: Succeeded`.
3. `... Test-Jam.ps1 -MinTests 39` passes.
4. `... Smoke-Jam.ps1` ends with `done=True crashed=False LogGB errors=0 timedOut=False`. It must have:
   - every earlier line;
   - the new lines with passing values;
   - no `LogGB` warning;
   - no `missing usage flag` warning.
5. Open both screenshots and look at them. Does it read as a heavy 3D machine, tilted, lit, and casting light? Say what you saw.

## Final report
As in `implementer_rules.md`, plus:
- the new values;
- how flight, collision and turning work;
- what you could not do and why.
