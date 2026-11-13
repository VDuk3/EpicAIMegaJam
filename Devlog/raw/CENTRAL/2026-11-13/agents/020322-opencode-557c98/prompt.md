# Task: the Entity's tentacles, animated procedurally (and the Entity as a real enemy class)

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first. In this task you DO build and run things, and the rest of "Hard rules" still applies:
- One build or run at a time.
- Do not open the Unreal Editor and do not launch PIE.
- Do not commit.
- Do not run `Tools/Jam/Import-*.ps1`, and do not touch `Content/` except `Content/Data/*.json`.

Owner (Spanish, translated): "Let's start the rigging work. Since it has tentacles, it would be good to use a procedural animation system like the ones used for creatures such as snakes or octopuses."

The Entity is the final boss (GDD §4 and §11). Its phase 1, "the machine", is a floating metal shell with a big central eye and about ten tentacles made of green hologram light. The architect already did the art and the rig. Your job is the code that brings it to life.

## The assets (already in `/Game/Glyphbound/Enemies/Entity/`)
All share one frame: origin at the centre of the shell, the eye looking to +X, up +Z, 1 unit = 1 cm. The model is about 290 cm tall: the shell spans roughly z -89..83 cm, and the tentacles hang down to z -224 cm.

| Asset | What | Material |
|---|---|---|
| `SM_Entity_Body` | the metal shell and the central machinery, rigid | `M_EntityBody`: `Glow`, `GlowColor`, `Flash` (0..1), `FlashGlow` |
| `SM_Entity_Eye` | the big central eye, rigid: **the weak point** | `MI_EntityEye` (same parameters) |
| `SK_Entity_Tentacles` | skeletal mesh, skeleton `SK_Entity_Tentacles_Skeleton`. 81 bones: `root` at the origin, then 10 chains `T00_00`..`T00_07` ... `T09_00`..`T09_07`, each of 8 bones from the root of a tentacle (`_00`, at the shell) to its tip (`_07`), every `_00` parented to `root`. Two-bone linear skinning along each tentacle. | `MI_EntityTentacles` (translucent hologram). Its parameter `SwayAmount` (cm) is a material sway done with world position offset: **set it to 0 on a dynamic instance**, because the bones now do the motion. |

Rig facts (from `Glyphbound/SourceArt/Enemies/Entity/export_report.json`; read the reference pose at runtime, do not hardcode them):
- The chain roots are on the lower half of the shell.
- Chain lengths are about 60..230 cm. Chain `T04` is a short stub of about 60 cm under the body.
- The weak point is a sphere: centre (66.4, 0.1, -13.5) cm, radius 27 cm, in the actor's visual frame. Provisional constants, named.

## 1. Pure math: `AI/GBTentacleMath.h/.cpp` (new, you own them)
Small, testable functions in a namespace `GBTentacle`, with no UObjects. Points are `FVector` in component space.
- `void SolveFabrik(TArray<FVector>& Joints, const FVector& Target, int32 Iterations, float Tolerance)`.
  - Joints[0] is the fixed root, and segment lengths are kept.
  - The usual forward/backward passes.
  - If the target is out of reach, the chain points straight at it.
- `float SpringStep(float& Value, float& Velocity, float Target, float Stiffness, float Damping, float Dt)`: a semi-implicit damped spring, stable for Dt up to 0.1 s (sub-step if needed). It returns the new value.
- `float WaveAngle(float Time, float Frequency, float Phase, float BoneIndex, float WaveLength, float Amplitude, int32 BoneCount)`: a travelling wave, `Amplitude * ((BoneIndex + 1) / BoneCount) * sin(2*pi*(Frequency*Time + Phase) - BoneIndex / WaveLength)`. The angle grows towards the tip.

## 2. The rig driver: `AI/GBTentacleRigComponent.h/.cpp` (new, you own them)
A `UActorComponent` that drives a `UPoseableMeshComponent` showing `SK_Entity_Tentacles`. A poseable mesh is enough here: no AnimBP and no Control Rig assets.
- On setup it reads the reference pose and finds the chains by bone name (`T<cc>_<bb>`), so it does not depend on 10 x 8.
- Every tick it composes four layers into one pose per chain, as rotations on top of each bone's reference local rotation. Keep it cheap: 80 bones per tick.
  1. **Idle wave:** two perpendicular `WaveAngle` bends per bone (pitch and yaw in the bone's frame), with a different random phase per chain, so the tentacles undulate out of step like an octopus floating.
  2. **Inertia / lag:** the actor's velocity and yaw rate, expressed in the actor frame, push a per-chain bend that follows `SpringStep`. When the Entity moves or turns, the tentacles trail behind and then settle. The tips bend most.
  3. **Reach / aim:** `SetReachTarget(FVector WorldTarget, float Weight)` and `ClearReach()`. The chain whose tip is closest to the target, or a chosen one, solves `SolveFabrik` on its joint positions towards the target. The other layers are blended out of that chain by `Weight`, and FABRIK positions are turned back into bone rotations. Used for the attack (below).
  4. **Hit ripple:** `AddRipple(float Strength)` starts a pulse that runs from root to tip along every chain over about 0.5 s and decays.
- `SetLimp(bool)`: on death the waves fade out over about 1 s and the chains droop towards -Z, as if the hologram loses power.
- **Do not let a chain pass through the shell:** the bend of bones `_00` and `_01` is limited. Pick a limit from looking at the result (provisional, named).
- **Every tuning number** goes in `Game.json` "Values" with a default in code, prefixed `EntityTentacle`:
  - wave: frequency, amplitude, wave length;
  - lag: strength, stiffness, damping;
  - root limit;
  - reach: blend in and out times;
  - ripple: strength and duration;
  - limp: droop.
  Start values from looking at the screenshot, and list them in your report.

## 3. The enemy: `AI/GBEntity.h/.cpp` (new, you own them)
`AGBEntity : public AGBEnemyCharacter`. Set `"ActorClass": "GBEntity"` on `Entity_Phase1` in `Enemies.json`, the only data change there. Check how `SpawnEnemy` resolves `ActorClass`, and use the form it expects.
- **InitFromDefinition:** call the base, then build the visual.
  - A scene root offset from the capsule centre, so the tentacle tips clear the floor by about 20 cm.
  - The body and the eye as `UStaticMeshComponent`s (no collision: the capsule is the collision).
  - The tentacles as a `UPoseableMeshComponent`.
  - The rig component.
  - Hide the base class placeholder look.
  - Load the assets by path, as `Weapons/GBGun.cpp` loads the gun meshes. If any is missing, log one warning and keep the placeholder.
- **The Entity floats.** It should hover at its height without gravity: flying movement mode, or whatever is simplest with the existing chase code. It must still reach the player around obstacles as other enemies do. Say in the report what you chose.
- **Body MIDs:** `Flash` 1 on a weak-point hit, fading over 0.3 s. `GlowColor` stays as authored.
- **Weak point:** a hit inside the eye sphere counts as a weak-point hit. `IsHeadshotLocation` is not virtual today. **You may change `AI/GBEnemyCharacter.h/.cpp` only to make `IsHeadshotLocation` virtual, and to give subclasses a protected virtual hook called from `HandleDamaged`** (for example `OnDamagedVisual(const FGBDamageInfo&)`). Nothing else in that file.
- **Attack** (`Entity_Phase1` is ranged):
  - during the windup, the nearest tentacle reaches towards the player (`SetReachTarget` with Weight ramping 0 -> 1);
  - on the shot, it whips back (Weight -> 0 with a short ripple);
  - if the base class lets a subclass choose the projectile's start point cleanly, start the shot at that tentacle's tip; otherwise leave it and say so.
- **Damage:** `AddRipple` on every hit, scaled by the share of max health.
- **Death:** `SetLimp(true)`.

## 4. Dev command and smoke
- `gb.SpawnEntity`, in `Core/GBDevCommands.cpp` (you may edit it for this): spawns `Entity_Phase1` 12 m in front of the player, in the player's current era. This is how the owner will look at it while playing.
- In the smoke run (`gb.Smoke`), add one Entity step at the end, before quitting:
  - spawn it in front of the player and let it idle for 1 s;
  - log `SMOKE entity bones=%d chains=%d tip_moved=%.1f`: the largest movement in cm of any `_07` bone between two moments 0.5 s apart. It must be > 3 (it is alive);
  - set a reach target 60 cm in front of the tip of chain 0, wait 0.6 s, log `SMOKE entity reach error=%.1f` (cm from that tip to the target, which must be < 15), then clear it;
  - log `SMOKE entity weakpoint eye=%d shell=%d`: `IsHeadshotLocation` at the eye centre must be 1, and at the top of the shell 0;
  - screenshot `15a_entity`, framed so the whole Entity is visible;
  - destroy it after the step, so the existing smoke steps are not affected.

## 5. Tests (`Tests/GBEntityTests.cpp`)
Exactly three, pure logic, none of whose names is a prefix of another:
- `Glyphbound.Entity.FabrikReach`: a 5-joint chain with a reachable target. The tip ends within the tolerance, the root does not move, and every segment length is kept within 0.1 cm.
- `Glyphbound.Entity.FabrikTooFar`: an unreachable target. The lengths are kept, and the chain is straight and points at the target (each joint on the root-target line within 0.1 cm).
- `Glyphbound.Entity.SpringSettles`: from 0 to a target of 10 with critical-ish damping, at dt 1/60. Within 1 s it is within 0.1 of the target and never overshoots by more than 5%; with dt 0.1 it stays finite and settles too.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests <the count before your change + 3>` passes. Run it once first to learn the count.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`. It must have:
   - every line it had before;
   - the three new `SMOKE entity` lines with passing values;
   - no `LogGB` warning.

Then open `Saved/Screenshots/Smoke/15a_entity.png` and look at it:
- Is the whole Entity visible, floating, tentacles below and green?
- Do the tentacles look like tentacles, with no chain twisted, collapsed into the body, or exploded?

Say what you saw in your report. If something looks wrong, fix it before you finish.

## Final report
As in `implementer_rules.md`, plus:
- the `EntityTentacle*` values you set;
- how the Entity floats;
- whether the shot starts at a tentacle tip.
