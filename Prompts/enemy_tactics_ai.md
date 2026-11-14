# Task: the future's enemies must fight like a good shooter's enemies (owner's review, 2026-11-14)

Read first: `Prompts/implementer_rules.md` (all of it applies, with the exceptions listed under "Machine rules" below),
`Design/GDD.md` sections 5 and 10, and the code you will change: `AI/GBEnemyCharacter.*`, `AI/GBSkeletalEnemy.*`,
`AI/GBEnemyAnimInstance.*`, `AI/GBFlyerEnemy.*`, `AI/GBDroneEnemy.*`, `AI/GBSentinelEnemy.*`, `AI/GBEncounter.*`,
`AI/GBEngage.*`, `AI/GBEnemy3D.*`, `Data/GBDataTypes.h`, `Data/GBDataSubsystem.*`, `Core/GBDevCommands.cpp` (the blocks
`Enemies3D` and `Enemies3DBase`), `Tests/GBAITests.cpp`, `Content/Data/Enemies.json`, `Encounters.json`, `Game.json`.

## What the owner said after playing (translated)
- The rifle robots move erratically, extremely fast, and go through objects: "they suddenly travel from where they are
  towards me when I take cover, about 3 m in half a second, through everything; but they do not really move, because
  they return to their starting position; it can happen many times in a couple of seconds".
- Their only intelligence seems to be firing bursts.
- Enemies chased him across the whole map: they must stay in their zones.
- They must be intelligent: take cover, use different parts of the arena, not stand all together, split up and try to
  surround the player. "I know shooters already have techniques for that, use them."
- Drones a little slower. The map must not be only robots with guns: put the orcs in too.
- Enemies must not appear from nowhere: they are already in their zone, patrolling.
- "Have taste: we do not want a game whose enemies look like a cheap game's."
- Add the Valkyrie with her sword. Add console commands: player invincibility, and one that opens the way to the
  Valkyrie's zone.

## What the architect already measured (facts, do not re-diagnose)
1. **The "teleport" is animation.** Every locomotion clip (retargeted from the UE template mannequin) carries its travel
   in the **pelvis** bone; the root bone never moves. Local pelvis translation, last frame minus first frame:
   `A_Robot_Rifle_Jog_Fwd` (1.53 s) +1061 cm X, `Walk_Fwd` (1.53 s) +518, `Walk_Bwd` (1.2 s) -400, `Walk_Left` (1.23 s)
   -411 Y, `Walk_Right` (1.5 s) +500 Y; the orc's are the same shape (Jog 1114, Unarmed Jog 1237, walks 420 to 750).
   Mid-clip values are half of these: the travel is linear. So the mesh walks up to 10 m away from its capsule and
   snaps back at the loop. Also `A_Orc_Attack_01/02/03` travel 104 to 223 cm and `A_Orc_ChargedAttack` 166 cm (mid
   -17: it steps back, then lunges). Idle, fire, reload and hit clips do not travel. Death clips travel about 160 to
   200 cm (they are only used when there is no physics asset).
2. **One-sided collision** in FutureBase (51 kit meshes blocked line traces from one side only): the architect fixes
   this in the map's assets (double-sided collision geometry). Nothing for you to do, but do not "fix" it in code.
3. The navmesh is generated at runtime (`RuntimeGeneration=Dynamic`).
4. Player: MoveSpeed 750, capsule 38 x 90, eye height 70 above the capsule centre.

## Machine rules (these replace "do not build" in implementer_rules.md)
- You build and run the checks yourself, **one build or game run at a time on this machine**. Before every build,
  test or smoke run: if `C:\GameJam\Glyphbound\Saved\machine.lock` exists and is younger than 45 minutes, or an
  `UnrealEditor.exe`, `UnrealBuildTool` or `cl.exe` process is running, wait (check again every 60 to 90 s; keep
  writing code meanwhile). The architect holds the lock while he has the Editor open for map work. When free, write
  the lock (`<who> | <what> | <UTC time>`, one line), run, and delete it when the run ends.
- Build: `powershell -File Tools/Jam/Build-Jam.ps1` (Editor target only while iterating).
- Tests: `powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.AI.'`.
- Smoke, filtered and without a window (run through PowerShell, not Git Bash, because of the `/Game/...` argument):
  `powershell -File Tools/Jam/Smoke-Jam.ps1 -Only Enemies3D -Offscreen` (arena) and
  `powershell -File Tools/Jam/Smoke-Jam.ps1 -Map /Game/Glyphbound/Maps/FutureBase -Only Enemies3DBase -Offscreen`.
- **Iterate cheaply:** while working on one part, run only the checks for that part (filtered smoke and tests, no game
  window). Do NOT run the full smoke, the full suite or the game-target build: the architect runs them once at the end.
- Never launch PIE, never open the Editor, never kill an Editor process, do not commit, do not launch other agents.
- Do not touch `Tools/Jam/Build-FutureBase.py`, the map, or any binary asset. The architect places things in the map.
- A check that cannot fail is worth nothing: for every smoke check you add, say in the report which wrong code it catches.
- Read screenshots of the smoke run yourself when a check is about what is seen (`Glyphbound/Saved/Screenshots/Smoke`).
- Everything in English (code, comments, logs, data, on-screen text). ASCII only. No gameplay numbers in code: stats in
  `Enemies.json` (per enemy), values in `Game.json` (global), with defaults in code.
- UE 5.9: check every engine API you are not sure of in `C:\Program Files\Epic Games\UE_5.9\Engine\Source`.

## Milestone 1: the bugs and the feel (finish, build, check, then go on)

### 1.1 Clips in place (the teleport)
In `UGBEnemyAnimInstance` (game thread, at Configure): for every clip, sample the pelvis at time 0 and at the end and
keep the drift (the bone named by an optional string `Anim.TravelBone`, default `pelvis`; a skeleton without it, like
the spider, has no drift). In the proxy's Evaluate, after sampling a locomotion clip at time T, subtract
`Drift * (T / Length)` from the pelvis' translation, so the body keeps its bob and sway but stays over the capsule.
Same for the one-shot Attack overlay when it is not additive (the orc's swings). Not for Death.
Also use the drift to know each clip's **authored ground speed** (drift length in component space times the mesh
component's scale, divided by the clip length) and use that as the clip's reference speed for the play rate instead of
the `WalkAnimSpeed` / `RunAnimSpeed` guesses (keep those stats as the fallback for clips without drift, like the
spider's). Clamp the locomotion play rate between `AnimRateMin` and `AnimRateMax` (stats, defaults 0.7 and 1.35): a
little foot slide is better than slow motion. The walk/run blend thresholds come from the authored speeds too.
Smoke check (arena block): while a skeletal humanoid moves, the horizontal distance between its pelvis bone (world)
and its capsule centre never exceeds 60 cm; log the maximum per enemy. The old code fails it (500 to 1000 cm).

### 1.2 How they move
- Path following must use acceleration (`UCharacterMovementComponent`: look for `bRequestedMoveUseAcceleration`),
  with `MaxAcceleration`, braking and a smoothed turn from stats (`Acceleration`, `BrakingDeceleration`, `TurnRate`):
  no instant direction changes.
- Three gaits, from stats: `PatrolSpeed` (unaware), `CombatSpeed` (repositioning under fire, short moves, peeking),
  `MoveSpeed` (running to a far position, flanking, falling back). Apply them through `MaxWalkSpeed` in a way that
  keeps slows working (the stat modifiers on MoveSpeed must still scale all three).
- Facing: while running a long move it faces where it runs (forward run clip); it faces the player when it walks in
  combat with a line of sight, peeks or fires. Never a full-speed backwards or sideways run.
- Ground enemies avoid each other (RVO avoidance of the movement component, or a simple separation push; check which
  works with path following) so they do not walk through or stack on one another.
- Delete the random 2 to 4 s strafe hop of `TickRanged`: the tactical positions of Milestone 2 replace it.

### 1.3 Zones (leash)
- `AGBEncounter` owns a **zone**: a world-space box = the bounds of its trigger box and its spawn points, grown by
  `EncounterLeashPadding` (Game.json, default 400 cm; optional per-encounter `LeashPadding` in Encounters.json), full
  height of the trigger plus the spawn points with a generous Z margin. `GetZone()`.
- Every enemy an encounter spawns gets the zone (`AGBEnemyCharacter::SetHomeZone(FBox, FVector Post)`) and **never
  leaves it**: every move goal (ground, melee, flyer) is clamped into the zone and projected to the navmesh for
  walkers. An enemy without an encounter (arena spawns, dev commands) has no zone and behaves as before.
- Player outside the zone: enemies with a line of sight and range still shoot from inside; the others take the best
  position inside the zone towards him and wait; melee enemies stop at the edge. Nobody follows him out.
- When nobody of the encounter has seen him for `EncounterLoseTargetTime` (Game.json, default 10 s) they go back to
  their posts and patrol again (unaware, but see 1.5 for how fast they notice the second time).
- Pure function for the clamp and "is inside" in the new `AI/GBTactics.h`, tested.

### 1.4 Already there, patrolling (no pop-in)
- An encounter whose definition says `"Prespawn": true` (new optional field; set it on every `Future_*`,
  `SubBoss_Future` and the new `Boss_Valkyrie_Future`; the old arena and test encounters keep today's behaviour)
  spawns its **first wave at BeginPlay** (when its required flags are met, it is not cleared, and again after a reset
  on the player's death), at its spawn points, unaware. The trigger box no longer starts it: the fight starts when one
  of its enemies notices the player (1.5); the encounter then becomes Active (`OnStarted`) and alerts its squad.
  Era presence keeps working as today (an enemy of the other era neither shows nor ticks).
- Unaware behaviour per spawn, optional `"Behavior"` on a spawn entry: `"Patrol"` (default for walkers and drones),
  `"Guard"` (stands at its post and looks around: turns to a new heading every few seconds), `"Dormant"` (does not
  move or look; wakes on damage, an alert or the player within `DormantWakeRange`: the corridor spiders, the Valkyrie).
  Patrol: a loop of 2 to 4 points inside the zone (its post, plus other spawn points of the encounter and reachable
  navmesh points), walked at `PatrolSpeed` with a pause at each (`PatrolPauseMin/Max`), looking along the path. Two
  patrollers must not share a route in step (offset their start). Drones patrol the same way in the air.
- Later waves are **reinforcements**: no "Wave N / M" message for prespawn encounters. They appear at spawn points the
  player cannot see (no line of sight from his eyes) and that are furthest from him; if every point is visible, the
  furthest ones. They arrive already alerted and run in.
- The smoke block `Enemies3DBase` must be updated to this flow (the player stands in view of them; they notice; the
  rest as before) and must also check: before the player comes near, the first wave already exists and at least one
  patroller moved along its route while unaware; nobody is outside its zone at any check; with the player teleported
  out of the zone (50 m away, same floor) for 6 s, no enemy leaves the zone.

### 1.5 Noticing
- Unaware enemies notice by: sight (cone `SightAngle`, range `AggroRange`, line of sight) held for `NoticeTime`
  seconds (stat, default 0.6; shorter the closer he is; 0.25 s when the squad was alerted in the last 60 s); damage;
  the player's gunfire within `HearingRange` (stat, default 2500 cm; add the hook where the player's guns fire); an
  alert from the squad.
- First notice plays a short beat: stop, turn to him, `AlertPause` (0.5 s) before acting; then the whole squad of the
  encounter is alerted with staggered delays (0.2 to 1.0 s each), not all in the same frame.
- Once aware they do **not** know where the player is when they cannot see him: they know his **last known position**
  (shared by the squad, refreshed by anyone who sees him).

### 1.6 Drones, sentinel
- `Drone_Future`: MoveSpeed 600 -> 400, OrbitStrength 0.6 -> 0.35, and acceleration-limited flight (no instant
  velocity changes). Flyers keep at least `FlyerSeparation` (stat, 250 cm) from each other, respect the zone, and
  patrol when unaware.

### 1.7 Console commands (in GBDevCommands.cpp, next to gb.SpawnEntity)
- `gb.God [0|1]`: player invulnerable (toggle without argument); a HUD message says the new state.
- `gb.OpenValkyrie`: sets every progress flag required by every `AGBEnergyGate` of the map (the gate goes out as if
  the cells had been placed). `gb.OpenDoors`: sets the required flag of every flagged `AGBDoor` too.

### 1.8 Data
- `Encounters.json`: `Future_Bay` wave 1 = Robot_Future x2 (Patrol), Orc_Future x2 (one Patrol, one Guard),
  Drone_Future x1; wave 2 = Orc_Future_GunCarrier x1, Orc_Future x1, Spider_Future x2; wave 3 = Sentinel_Future x1,
  Orc_Future_Enforcer x1. `Future_Spiders` = Spider_Future x4 Dormant. `Future_DeepWing` wave 1 = Orc_Future x2,
  Robot_Future x1, Spider_Future x2; wave 2 = Sentinel_Future x1, Drone_Future x2. `SubBoss_Future` unchanged
  (Warlord Guard, Enforcers Patrol, drones). New `Boss_Valkyrie_Future`: Valkyrie_Future x1, Dormant, Prespawn,
  SkillPoints 3, cleared flag `FutureValkyrieDefeated`; the architect places it in her room.
- Give every new stat a value in `Enemies.json` for the ranged humanoids (Robot_Future, Orc_Future,
  Orc_Future_GunCarrier, Orc_Future_Enforcer, Valkyrie_Future) and what applies for the melee ones and the flyers.
  Starting values: robot Patrol 150 / Combat 260 / Move 430; orc 140 / 240 / 400; Enforcer 130 / 220 / 360.

## Milestone 2: squad tactics (the "intelligence")
Known techniques, to be used on purpose: tactical position selection by scoring sampled points (what Killzone and
UE's EQS do), cover with a firing position next to it, attack tokens (F.E.A.R., Halo, DOOM 2016: only a few may fire
at a time, the others move), squad roles (base of fire and flankers), last known position and search, the melee
"circle" (slots around the player). No behaviour tree or EQS assets: C++ only, data from JSON.

### 2.1 The squad (`AI/GBSquad.h/.cpp`: a `UWorldSubsystem`, ticked a few times per second)
One squad per encounter (its living, aware, ground and flying enemies); enemies without an encounter that are within
`AlertRadius` of each other form an ad-hoc squad. The squad holds: the last known position and when anyone last saw
the player; each member's **role**; each member's **reserved position**; the **attack tokens**.
- Roles for ranged ground members, reassigned when someone dies or every few seconds: nearest to the player's front =
  `Suppress` (base of fire: holds cover with a view, fires the most, fires at the last known position when he hides:
  suppressive bursts that hit his cover); then `FlankLeft` and `FlankRight` (positions 60 to 120 degrees round the
  player from the base of fire's line, reached by paths that stay out of his view when possible); extra members
  alternate `Suppress` and `Assault` (bounds forward from cover to cover to `PreferredRangeMin`). One member alone is
  `Suppress`. Per-enemy stat `Aggression` (0 to 1) shifts the choice: orcs push (Assault / flank close, shorter stays
  in cover, closer range), robots are disciplined (hold cover, flank wide). The Enforcer suppresses.
- Attack tokens: at most `SquadMaxShooters` (Game.json, default 2; +1 when the squad has 5 or more) ranged members
  may be in a burst at once; a member asks for a token before the telegraph and returns it when the burst ends, with
  a minimum time before the same member gets one again when others are waiting. Flyers use the same pool (they count
  as half). Members without a token use the time to move or stay hidden: that is what makes the fire-and-move rhythm.
- Pure functions in `GBTactics` (tested): role assignment from (count, distances, aggression), token grant order,
  the position score below.

### 2.2 Tactical positions
A member asks the squad for a position; at most one query per squad tick (stagger them). The query:
1. Candidates: 30 to 40 points, rings and arcs around the last known position at the member's range band, plus points
   around the member itself; projected to the navmesh; inside the zone; not within `SquadSpacing` (stat, 350 cm) of a
   position reserved by another member or of another member.
2. For each: is it **covered** (traces from the candidate at chest and head height to the player's eyes are blocked),
   and does it have a **firing spot** within `PeekDistance` (120 cm, to either side, perpendicular to the line to the
   player) with a clear line to him? Low cover counts only when the enemy has crouch clips (2.4): then the test is
   blocked at crouch height (`CrouchEyeHeight`) and clear at standing height, and the firing spot is the same point.
3. Score (weights in Game.json, `Tac*`): cover with a firing spot >> open spot with a line of fire > cover without
   one; distance to the player inside the role's band; angle away from the other members' positions as seen from the
   player (spreading and surrounding him; flankers want 60 to 120 degrees from the base of fire); short path, and
   penalise a path that crosses his line of fire closely; penalise positions in his current view cone at close range
   for flankers; never a point he is within `PreferredRangeMin * 0.5` of.
4. Path length through the navmesh only for the best few. Reserve the winner. If nothing passes, hold in place and
   fire from the open (today's behaviour without the hop).
Keep the cost bounded and flat: no per-tick queries, no full-squad recomputation in one frame.

### 2.3 What a ranged member does (replaces the Advance / Hold / Retreat band in `TickRanged`)
States: `Unaware` (1.4) -> `Alert` beat -> `MoveToPosition` (run or combat walk by distance) -> `InCover` (hidden;
waits for a token and its burst cooldown; with crouch clips it crouches) -> `Peek` (steps to the firing spot or stands
up, telegraph, burst, one or two bursts by `BurstsPerPeek`) -> back to `InCover`.
It **relocates** (asks for a new position) when: the cover is compromised (the player has had a clear line to the
cover point itself for more than `CoverCompromisedTime`, 1.2 s: he flanked it); the player is nearer than
`PreferredRangeMin` (fall back, away from him, towards its own side); it has been there longer than a random
`CoverStayMin/Max` (6 to 12 s; shorter with Aggression); it lost 30 % of its health in 2 s; its role changed; or its
firing spot has had no line of fire for `NoLineTime` (3 s) while the squad knows where he is.
When nobody has seen the player for `SearchDelay` (4 s): the Assault / flank members advance in bounds to positions
that see the last known position; Suppress members keep watching it. After `EncounterLoseTargetTime` everyone returns
to patrol (1.3).
Hit while moving in the open with no token: finish the move (do not stop to shoot in the open unless it has no
position). A member that has no cover at all behaves like the open-spot case: stands, fires with tokens, sidesteps
at combat walk every few seconds to a neighbouring free point (short, slow, deliberate, not the old hop).
The shot telegraph, bursts, spread, tracers and effects stay as they are.

### 2.4 Crouch and sprint clips (optional, data)
New optional clip keys: `Anim.CrouchIdle`, `Anim.CrouchWalk`, `Anim.Sprint`, `Anim.PatrolWalk` (relaxed walk).
When `CrouchIdle` exists the enemy may use low cover and crouches in it (visual pose and a lower chest / muzzle point;
the capsule stays; the head-shot zone follows the pose if that is cheap). `Sprint` replaces Run above its authored
speed. `PatrolWalk` replaces Walk while unaware. Missing keys change nothing. The architect is importing these clips
from the Kingboars locomotion pack in parallel and will add the keys to `Enemies.json`; build the slots now.

### 2.5 Melee and flyers
- Melee (spiders, Siege orc, Warlord, the Valkyrie's close game): each gets a **slot** on a circle round the player
  (angle spread over the living melee members of the squad, radius a bit over its attack range) and paths to its slot,
  then closes in; at most `SquadMaxMeleeAttackers` (default 2; the Warlord always may) are inside attack range at
  once, the others circle at `MeleeWaitRadius`. They respect the zone.
- Flyers take orbit slots (angles spread among the squad's flyers) instead of each picking a random side.
- The Valkyrie (`Valkyrie_Future`, ranged bursts from her sword today): keep her class, give her `Aggression` 1, both
  the burst and a melee swing when the player is within `MeleeRange` (stat; use the Attack clip and her
  `AttackDamage * MeleeDamageMult`), and let her use the squad logic alone (she relocates, does not hide for long:
  `CoverStayMax` short). She is Dormant in her room until the player comes in.

### 2.6 The horde chief: an elite orc with a shield and a heavy pistol (owner, same day)
"One of the armors goes to an orc you make bigger, 1.5 to 1.8 times. There should be a shield and a special orc
pistol: they are for that enemy, the chief of the horde. He has to use the shield to cover himself; give the shield a
hit box as big as it looks, or bigger, so it blocks the player's shots well. The pistol looks heavy: a low rate of
fire and a lot of power. The elite orc needs a somewhat different AI, since he has a shield and another weapon."
- `Orc_Future_Warlord` becomes him (he is the sub-boss of `SubBoss_Future`): `MeshScale` 1.65, capsule to match,
  ranged, still `AGBSkeletalEnemy` (a subclass `AGBShieldEnemy` if that is cleaner). All of it from data, usable by
  any enemy that names a shield.
- Look: strings `Shield` (static mesh; the art has NOT arrived yet: when the key is empty or the asset is missing use
  a `GBPlaceholder` slab of `ShieldWidth` x `ShieldHeight` so everything can be built and checked now),
  `ShieldSocket` (default `lowerarm_l`), `ShieldOffset` (same format as `WeaponOffset`). The weapon stays the enemy
  rifle until the orc pistol arrives (the architect swaps the asset path).
- The shield blocks: while it is **raised**, damage whose direction comes from inside the frontal arc `ShieldArc`
  (stat, 130 degrees, measured from where the shield faces) is multiplied by `ShieldDamageTaken` (0): bullets,
  projectiles and spells alike; show the block (the project's metal / technology impact effect at the hit point, on a
  box component the size of `ShieldWidth` x `ShieldHeight` x a few cm that blocks the player's shot traces, so the
  shot visibly stops on the shield) and no hit reaction. Find the cleanest hook in the damage path
  (`UGBAttributeComponent` / `UGBCombatLibrary`); do not special-case weapons. Hits from the side, the back, or while
  the shield is lowered hurt normally. Area damage centred behind him ignores the shield.
- His AI (a role of its own, `Anchor`; he never uses cover: he is the cover): walks at the player with the shield
  raised at `CombatSpeed`, always facing him but turning slowly (`TurnRate` 110), so a player who runs round him or a
  second threat gets his side. In range with a line of fire: **lowers the shield** to aim (`AttackWindup`, 1.3 s,
  telegraphed: the weapon's glow / a muzzle charge effect), fires ONE heavy shot (`BurstCount` 1, `AttackDamage` 30,
  `SpreadDegrees` 1, a thick tracer and a big impact), stays open for `ShieldRecover` (0.8 s), raises it again and
  waits `AttackInterval` (3.5 s). He does not need an attack token and does not count against the others'.
  `ShieldStaggerDamage` (stat) blocked within 3 s, a lightning stun or a freeze knocks the shield aside for
  `StaggerTime` (2.5 s): the opening the player earns. Player within `MeleeRange` (300): shield bash (the Attack
  clip, `AttackDamage * MeleeDamageMult`, a knockback on the player if the player class already has a way to be
  pushed; otherwise damage only). He respects the zone like everyone.
- The squad treats him as the base of fire: the other members prefer flank roles when an `Anchor` is alive.
- Pose: no shield clips exist. Add to the anim proxy an optional, data-driven pose layer: string `Pose.ShieldRaised`
  = `bone:pitch,yaw,roll;bone:...` (local rotation offsets applied after everything else, blended by a 0..1 alpha that
  follows the shield state with `ShieldPoseBlend` seconds). Give it starting values that bring the left forearm up in
  front of the chest (upperarm_l, lowerarm_l); the architect tunes them on renders. Add `gb.EnemyPose <id>` only if
  you need it to see the pose in a smoke screenshot; a closeup screenshot of him with the shield raised and one while
  he aims belong in the arena smoke block.
- Smoke (arena): the chief takes no damage from 5 hits to the front while the shield is raised and takes it from
  behind; he fires single shots no faster than `AttackInterval`; the shield is lowered at the moment of each shot.

### 2.7 Checks
- Tests in `Tests/GBAITests.cpp` (pure logic only; each must be able to fail; names must not prefix one another):
  zone clamp, role assignment, token order, position score ordering (cover with a firing spot beats an open spot
  beats cover without one; spacing and flank angle change the winner).
- Arena smoke (`Enemies3D`): add a tactics group: an arena with a few blocking boxes (use the arena builder's
  existing placeholder walls or spawn some) and 4 Robot_Future + 2 Orc_Future in an ad-hoc squad with the player
  invulnerable and still. After 12 s check and log: never more shooters in a burst at once than the token limit;
  the members' positions seen from the player span at least 70 degrees; no two members within 200 cm for more than
  2 s; at least one member spent time `InCover` behind something (when the arena has cover) and at least one burst was
  fired from a `Peek`; pelvis-to-capsule distance check of 1.1. Take a screenshot from above
  (`enemies3d_tactics_top`) and one from the player's eyes.
- `Enemies3DBase`: as in 1.4, on the real map.
- Log one `TACTICS` line per squad every few seconds in smoke runs (roles, states, tokens, positions) so the
  architect can read what happened.

## Final report
The list of implementer_rules.md, plus: every stat and Game.json value you added with its value; what each new smoke
check catches; the last filtered smoke and test results (numbers, not adjectives); anything of this brief you did not
do, and why. Do not claim what you did not run.
