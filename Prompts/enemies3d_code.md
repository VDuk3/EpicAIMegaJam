# Brief: 3D enemies of the future (code) — skeletal shooters, drone, sentinel, spider

You are the implementer for Glyphbound (Unreal Engine 5.9.0, C++ module `Glyphbound`, repo `C:\GameJam`). Read first:
`C:\GameJam\CLAUDE.md`, `Design/GDD.md` §10, §11 and §19 "Enemy templates", and the code in
`Glyphbound/Source/Glyphbound/AI/` (`GBEnemyCharacter.*`, `GBEntity.*` is the example of a 3D enemy subclass that loads
its meshes by path), `Combat/GBCombatLibrary.h`, `Combat/GBProjectile.h`, `Data/GBDataTypes.h` (FGBEnemyDefinition),
`Content/Data/Enemies.json`, and how `Core/GBDevCommands.cpp` builds the smoke steps (`gb.Smoke <Prefix>`).

The art (meshes, skeletons, retargeted animations, VFX) is being imported in parallel by the architect. Your code must
**read every asset path from Enemies.json `Strings`** and degrade gracefully when an asset is missing (log once, fall back
to the existing placeholder look), so it compiles and passes its smoke before the art lands.

## What to build (new files; do not rewrite the existing enemy)

### 1. `UGBEnemyAnimInstance` (AI/GBEnemyAnimInstance.*) — animation without AnimBlueprint assets
A native `UAnimInstance` with its own `FAnimInstanceProxy` (override `CreateAnimInstanceProxy` / `DestroyAnimInstanceProxy`;
the proxy's `Evaluate(FPoseContext&)` samples `UAnimSequence`s with `GetAnimationPose` + `FAnimExtractContext` and blends
them with `FAnimationRuntime::BlendTwoPosesTogether` / `BlendPosesTogether`). The skeletal mesh component gets
`SetAnimInstanceClass(UGBEnemyAnimInstance::StaticClass())`. No AnimBP asset may be required.
- Clips (all optional): Idle, Walk, Run, StrafeLeft, StrafeRight, WalkBack, Attack, Hit, Death.
- Locomotion: blend by ground speed (Idle -> Walk at WalkSpeed -> Run at RunSpeed; the speeds are stats so the feet
  match) and by local move direction (forward / back / left / right), clips time-synced by normalized phase.
- Upper body aim: spread the aim pitch (and a small yaw offset) over spine_01..spine_05 / neck (bones that do not exist
  are skipped). Procedural kicks (decaying offsets on spine/clavicle_r/hand_r) for a shot's recoil and a hit flinch,
  used when no Attack/Hit clip exists.
- One-shot overlay (Attack or Hit clip) blended in over 0.1 s and out, full body or upper body from spine_01 up.
- Short crossfade (0.15 s) whenever the dominant locomotion clip changes.
- Fallback if the proxy route proves impossible in 5.9: `UAnimSingleNodeInstance` playing the locomotion clip; say so.

### 2. `AGBSkeletalEnemy : AGBEnemyCharacter` (AI/GBSkeletalEnemy.*) — humanoid FPS shooters and the spider
Look (all from `Strings`):
- `Mesh` (USkeletalMesh path), `MeshScale` stat (default 1), `MeshYaw` stat (default -90), mesh placed so its feet
  touch the capsule bottom.
- `Armor`: `;`-separated USkeletalMesh paths on the same skeleton, each a component using `SetLeaderPoseComponent`
  to the body. `ArmorHideBody` stat 0/1 (body hidden where fully armored is not needed now).
- `Weapon`: UStaticMesh path attached to socket/bone `WeaponSocket` (default `hand_r`), relative transform from
  `WeaponOffset` = "x y z pitch yaw roll scale" (default identity). Muzzle = the weapon mesh socket `Muzzle` if it has
  one, else its bounds' +X end.
- `Anim.Idle`, `Anim.Walk`, `Anim.Run`, `Anim.StrafeLeft`, `Anim.StrafeRight`, `Anim.WalkBack`, `Anim.Attack`,
  `Anim.Hit`, `Anim.Death`: UAnimSequence paths. Stats `WalkAnimSpeed`, `RunAnimSpeed` (cm/s at which the clip's feet match).
- Death: if the mesh has a physics asset, ragdoll (profile `Ragdoll`, impulse from the killing blow's direction), the
  weapon detaches and falls (simulate physics on the weapon component); otherwise the Death clip; otherwise the parent's
  behaviour. Corpses are removed after `CorpseTime` as today.
- `Fx.HitImpact`: Niagara system spawned at the hit location when the player hurts it (robots: sparks, not blood; the
  game is Teen).

Basic first-person-shooter AI for `AttackMode` "Ranged" (replaces the parent's Chase/Windup/Recover flow for this class):
- Perception: sees the player inside `SightAngle` (stat, default 140°) and `AggroRange` with line of sight; always aware
  once provoked (hit) or alerted. When it first notices the player it alerts other enemies within `AlertRadius`
  (default 1800) that have line of sight to it or are within half that radius.
- Engage: keep a distance band `PreferredRangeMin` .. `PreferredRangeMax` (defaults 600 / 1400): close in on the navmesh
  when too far or without sight, back off when too close (if the navmesh allows), otherwise strafe sideways to random
  reachable points 300–600 cm away every 2–4 s (projected on the navmesh). Always face the player while engaged
  (controller desired rotation / actor yaw) so it strafes and walks backwards facing him.
- Shooting: bursts of `BurstCount` (default 3) shots, `BurstShotInterval` (0.12 s) between shots, `AttackInterval`
  between bursts and a short `AttackWindup` telegraph before each burst (aim pose + the existing telegraph tint if any).
  Each shot is **hitscan** from the muzzle towards the player's chest with spread `SpreadDegrees` (default 4) plus
  `MoveSpreadDegrees` (default 3) when the player moves at full speed (use `UGBCombatLibrary::ApplySpread`), damage
  `AttackDamage` per shot through `UGBCombatLibrary::DealDamage` (keep `FGBDamageInfo` as `PerformAttack` fills it).
  Needs line of sight to start a burst; stops a burst when it loses it.
- VFX of each shot (paths from Strings, with these defaults):
  - `Fx.Muzzle` = `/Game/MuzzleFlash3D/FX/Futuristic/NS_MuzzleFlash_Single_Futuristic` at the muzzle, along the shot;
  - `Fx.Tracer` = `/Game/SciFi_Impacts/FX/NS_Tracer_Red` from muzzle to the hit point (look up the system's user
    parameter name is not known yet: spawn the system at the muzzle rotated along the shot, and put the end point and
    the length in a small helper that sets the user parameter names listed in Strings `Fx.TracerEndParam` (vector) and
    `Fx.TracerLengthParam` (float), both optional; the architect fills the real names after looking at the asset);
  - `Fx.Impact` = `/Game/GunFX/NiagaraSystem/ImpactFX/Concrete/NS_Impact_Concrete_Low` where a shot hits the world.
  Add `"Niagara"` to `Glyphbound.Build.cs` (hunk only).
- Melee mode ("Melee", for the spider and the heavy orcs): chase as the parent does, plus a lunge: within `LungeRange`
  (stat; 0 = none) and with line of sight, after the windup, `LaunchCharacter` towards the player (`LungeSpeed`,
  `LungeUpSpeed`), the hit lands if it reaches him during the lunge; then recover.
- Movement uses the navmesh (`AAIController` MoveTo, as the parent does) with the parent's local steering as fallback.
  Reuse, do not duplicate, the parent's helpers (make them protected/virtual where needed, small hunks).

### 3. `AGBDroneEnemy : AGBEnemyCharacter` (AI/GBDroneEnemy.*) — a flying laser drone
- Static mesh parts from Strings: `Body` (with sockets `Rotor_0`..`Rotor_3` and `Eye`), `Rotor_0`..`Rotor_3`
  (pivot at their hub, spin axis +Z), `Eye` (pivot at the eye's centre, lens facing +X). Missing parts = placeholder.
  `MeshScale` stat.
- Rotors spin around their local Z (`RotorSpeed` deg/s, faster while moving, opposite directions on diagonals).
- The eye turns (yaw and pitch, clamped, smoothed) to look at the player when it has a target, scans slowly left-right
  when idle; its material's emissive (`Fx` parameter `Glow`, or a dynamic material scalar named `EmissiveBoost` if the
  material has it) brightens during the windup.
- Flies: `MOVE_Flying`, keeps `HoverHeight` (default 250 cm) above the ground under it, follows the player within
  `PreferredRangeMin..Max`, circles him (orbit direction flips now and then), bobs a little, banks into its motion
  (body roll/pitch lerped, collision capsule upright). It must not fly through walls (sweep moves; simple obstacle
  avoidance: the parent's steering idea on the horizontal plane, plus climbing over low obstacles).
- Attack: after the windup a short red laser (hitscan, `Fx.Tracer`/`Fx.Impact` like the shooters, default
  `/Game/SciFi_Impacts/FX/NS_Tracer_Red`) from the eye. On death it falls with physics (drop the body as a simulated
  static mesh, rotors stop) and is removed after `CorpseTime`.

### 4. `AGBSentinelEnemy : AGBEnemyCharacter` (AI/GBSentinelEnemy.*) — a floating sphere turret with an eye
- Parts: `Shell` (sphere, front = +X), `Core` (the blue orb/eye, pivot at its centre, sitting in the shell's front
  opening). The whole sentinel turns slowly (`TurnRate`) to face the player; the core turns faster inside it (clamped
  to the opening, ±25°) so the eye visibly tracks him; the core pulses (emissive scalar, idle slow pulse, fast
  during the charge).
- Floats `HoverHeight` (default 220) above the ground with a slow bob, drifts very slowly to keep line of sight
  (navmesh or flying sweep), never chases fast.
- Attack: a charge of `AttackWindup` seconds with a thin aiming line from the core to where it is aiming (it tracks the
  player with a lag, so moving dodges it), then a heavy beam (hitscan, damage `AttackDamage`, `Fx.Tracer` default
  `/Game/SciFi_Impacts/FX/NS_Tracer_Blue`, `Fx.Impact` default `/Game/SciFi_Impacts/FX/NS_Energy_Impact_1`).
  Weak point: the core (hits within `CoreRadius` of it are weak-point hits; see `UGBCombatLibrary::IsWeakPointHit`
  and how the Entity's eye does it).

### 5. Data, smoke, tests
- Enemies.json: give `Drone_Future`, `Orc_Future`, `Orc_Future_GunCarrier`, `Orc_Future_Enforcer`, `Orc_Future_Siege`,
  `Orc_Future_Warlord` their `ActorClass` and the new keys, and add `Robot_Future` (Soldier template: 60 HP, ranged
  rifle), `Sentinel_Future` (Elite: 180 HP, ranged beam), `Spider_Future` (Swarm: 30 HP, 600 speed, melee lunge).
  Keep §19's numbers for the existing ids; new ids follow their template row. Asset paths: use these (the architect
  creates them; missing ones must not break anything):
  - Robot: `/Game/Glyphbound/Characters/Robot/SK_Robot`, anims `/Game/Glyphbound/Characters/Robot/Anims/A_Robot_<Clip>`
  - Orc: `/Game/Glyphbound/Characters/Orc/SK_Orc`, anims `.../Orc/Anims/A_Orc_<Clip>`, armor
    `/Game/Glyphbound/Characters/Orc/Armor/SK_OrcArmor_<Set>_<Slot>` (Set A..D; slots Helmet, Chest, Pelvis, Shoulders,
    Arms, Hands, Thighs, Legs, Feet) — leave `Armor` lists empty for now, the architect fills them.
  - Enemy rifle: `/Game/Glyphbound/Weapons/EnemyRifle/SM_EnemyRifle` (origin at the grip, barrel +X, socket `Muzzle`).
  - Drone: `/Game/Glyphbound/Characters/Drone/SM_Drone_Body`, `SM_Drone_Rotor_0..3`, `SM_Drone_Eye`.
  - Sentinel: `/Game/Glyphbound/Characters/Sentinel/SM_Sentinel_Shell`, `SM_Sentinel_Core`.
  - Spider: `/Game/Glyphbound/Characters/Spider/SK_Spider`, anims `.../Spider/Anims/A_Spider_<Clip>`.
  Tuning numbers you add go to the JSON, not the code (the project rule).
- Smoke: a block of steps named `Enemies3D...` (filter `gb.Smoke Enemies3D`) in the test arena: spawn one of each new
  enemy id near the player; check each one (a) moves (navmesh or flight) towards a sensible place, (b) attacks and the
  player's health drops (or a hitscan shot is logged), (c) a drone and a sentinel keep their hover height, (d) dies to
  damage and its corpse is removed; log each enemy's anim clip in use and a screenshot of the group.
- At most one or two automation tests of pure logic that is easy to break and hard to see playing (for example the
  range-band decision: advance / hold-strafe / retreat from distance, sight and navmesh result). Follow CLAUDE.md
  "Tests y verificación" (anchored filters, each test must be able to fail).

## Rules
- **Iterate cheaply** (owner's rule): while iterating build only the Editor target
  (`powershell -File Tools/Jam/Build-Jam.ps1` — read it for the target switch) and run only your filtered smoke without a
  window: `powershell -File Tools/Jam/Smoke-Jam.ps1 -Only Enemies3D -Offscreen` (~20 s). The full smoke and the game
  target build run **once at the end**. Never launch PIE.
- **Machine lock**: other chats share this machine. Before any build or game run, check
  `Glyphbound/Saved/machine.lock` (a lock younger than 45 min held by someone else = wait; sleep-poll at most every
  2 min) and that no `UnrealEditor.exe` / `UnrealEditor-Cmd.exe` is running; then write your own lock line
  ("enemies3d-code | build | <UTC time>") and delete it when done.
- Other chats are editing: stay out of `World/GBBreakable.*`, `GBEnergyGate.*`, `GBLadder.*`, `GBDoor.*`, the player's
  mantle/ladder code, the `TourRun` namespace in `GBDevCommands.cpp`, and `Tools/Jam/Build-FutureBase.py`. In shared files
  (`GBDevCommands.cpp`, `Glyphbound.Build.cs`, `GBEnemyCharacter.*`, `Enemies.json`) make small hunks.
- Do **not** commit; the architect reviews and commits. English in code, comments and logs.
- Report at the end: files changed, what each class does, smoke output summary (pass/fail lines), anything you could not do.
