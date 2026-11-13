# Code review: the Entity boss (procedural tentacles + 3D presence)

You are reviewing C++ for **Glyphbound** (Unreal Engine 5.9.0), in `Glyphbound/`. This is a read-only review: do not edit files.

## What to review
Two implementer runs on the final boss, the Entity:
1. Commit 1517e72 (Muse Spark), brief `Prompts/feedback13_entity_procedural.md`: procedural tentacle animation, the `AGBEntity` enemy class, and a smoke step.
2. Uncommitted changes in the working tree (Sonnet), brief `Prompts/feedback14_entity_3d.md`: weighted turning, 3D aim, flight with banking and strafe, a shell hit sphere, energy bolts, and an eye light.

See it all with:
`git diff 1517e72~1 -- Glyphbound/Source Glyphbound/Content/Data`

These files are in scope:
- `AI/GBEntity.*`
- `AI/GBTentacleRigComponent.*`
- `AI/GBTentacleMath.*`
- `AI/GBEnemyCharacter.*` (only the hooks)
- `Combat/GBProjectile.*`
- `Core/GBDevCommands.cpp` (the Entity parts only)
- `Tests/GBEntityTests.cpp`
- `Content/Data/Game.json` and `Enemies.json`

Ignore other chats' changes in `Tools/Blender`, `Devlog` and so on.

## Look for, in order of priority
1. **Correctness bugs:**
   - crashes and null dereferences;
   - wrong maths (FABRIK, springs, quaternion/rotator composition on the bone poses, spaces mixed: component vs world vs bone-local);
   - frame-rate dependence or instability at low FPS;
   - state not reset on death, respawn or era switch;
   - timers or lights left running after `Destroy`.
2. **Regressions for other enemies and projectiles.** `GBProjectile` changes must be inert unless `bEnergyBolt` is set. Check the cost of the two new dormant components on every projectile: is the light registered or ticking when unused?
3. **Collision:**
   - The capsule now ignores `ECC_Camera` and `ECC_WorldDynamic`, and a shell `USphereComponent` was added. Can the player's hitscan, projectiles, melee or spells still damage it?
   - Are there double hits (sphere plus capsule)?
   - Does anything else rely on the capsule being hit? Look at AI perception, encounter logic, pickups and nav.
4. **Performance per tick:** 80 bones, poseable mesh updates, lights. Note anything proportional to the number of loaded objects.
5. **Test quality.** Could each of the three tests in `GBEntityTests.cpp` pass with a wrong implementation?
6. Anything in the smoke code that could make the smoke flaky.

## Output
A numbered list of findings, most severe first. For each one:
- file:line;
- what is wrong;
- a concrete failure scenario;
- the fix in one or two sentences.

Say "no findings" for a category you checked and found clean. Keep it under about 60 lines.
