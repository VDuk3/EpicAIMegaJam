# Code review: squad tactics, zones, patrols and the shielded chief (Glyphbound, UE 5.9 C++)

You are reviewing uncommitted work in `C:\GameJam` (read-only). Context: `CLAUDE.md`, `Design/GDD.md` section 10 (the
v0.57 block "How the future's enemies fight") and the brief the implementer worked from: `Prompts/enemy_tactics_ai.md`.
Run `git status` and `git diff` to see the change.

Files:
- New: `Glyphbound/Source/Glyphbound/AI/GBShieldEnemy.*` (the horde chief: shield arc on the incoming-damage hook, aim / open / bash phases, stagger), `Glyphbound/Source/Glyphbound/AI/GBTactics.*` (pure decisions: zone clamp, roles, attack tokens, position
  score), `AI/GBSquad.*` (a world subsystem: one squad per encounter, last known position, roles, reserved positions,
  tokens, the tactical position query on the navmesh).
- Edited: `AI/GBEnemyAnimInstance.*` (clips played in place: the pelvis' linear drift is subtracted; clip speeds read
  from the drift; crouch / sprint / patrol slots; a data-driven pose layer for the shield arm), `AI/GBEnemyCharacter.*`
  (home zone, noticing, patrol / guard / dormant, melee slots), `AI/GBSkeletalEnemy.*` (the ranged member's states:
  move to position, in cover, peek, relocate, search; the shielded chief), `AI/GBFlyerEnemy.*` and the drone and
  sentinel (zone, separation, orbit slots, acceleration), `AI/GBEncounter.*` (zone, prespawned first wave,
  reinforcements out of sight), `Data/GBDataTypes.h` and the data loader, `Weapons/GBGun.cpp` (gunfire heard),
  `World/GBBreakable.*` (the glass wall shatters into pieces moved by hand), `Player/GBPlayerCharacter.cpp` (-AllWeapons test start), `Core/GBDevCommands.cpp` (smoke blocks
  `Enemies3D` and `Enemies3DBase`, the tour's wall check, `gb.God`, `gb.OpenValkyrie`, `gb.OpenDoors`),
  `Tests/GBAITests.cpp`, `Content/Data/Enemies.json`, `Encounters.json`, `Game.json`.

Look for real defects, most severe first:
1. Crashes and lifetime: squad members that die, are destroyed by an encounter reset or leave the era while the squad
   still holds them (raw pointers, stale reservations, tokens never returned: a dead token holder must not silence the
   squad forever); the subsystem across map changes; anim proxy thread safety (worker thread: only data copied on the
   game thread); components created at runtime; division by zero and NaN (zero-length clips, zero drift, empty squads,
   a player exactly on a member).
2. Behaviour bugs the owner would see: an enemy leaving its zone by any path (path following overshoot, a lunge, a
   knockback, a flyer); enemies that all pick the same position or stand in a clump; a member stuck forever in a state
   (waiting for a token that never comes, a position it cannot reach, cover that no longer exists); everyone hiding
   and nobody shooting; enemies that know where a hidden player is; prespawned enemies of the other era visible or
   acting; an encounter that cannot be cleared (a prespawned enemy never counted, reinforcements that never arrive,
   a reset after the player's death that leaves two first waves); the shield blocking damage from behind, or never
   lowering; the mesh still leaving its capsule on some clip (attack overlays, the new clips).
3. Cost: anything that runs per tick per enemy and should not (navmesh queries, full squad scans, traces), or that
   grows with everything loaded.
4. Data: keys the code reads that the JSON does not set and the other way round; units.
5. Tests and smoke checks that wrong code would still pass (project rule: every check must be able to fail).
Ignore style. For each finding: file:line, what goes wrong, a concrete scenario, the smallest fix. End with a one-line
verdict.
