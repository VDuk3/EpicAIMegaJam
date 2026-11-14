# Code review: the 3D enemies of the future (Glyphbound, UE 5.9 C++)

You are reviewing uncommitted work in `C:\GameJam` (read-only). Context: `CLAUDE.md`, `Design/GDD.md` §10 and §19, and the
brief the implementer worked from: `Prompts/enemies3d_code.md`.

Files to review (run `git diff` and `git status` to see them):
- New: `Glyphbound/Source/Glyphbound/AI/GBEnemyAnimInstance.*` (native AnimInstance with a custom FAnimInstanceProxy that
  samples and blends sequences), `GBEnemy3D.*` (asset loading helpers, hitscan + VFX), `GBEngage.*` (pure engage
  decisions), `GBSkeletalEnemy.*` (humanoid/spider enemy: look, FPS ranged AI, melee lunge, ragdoll death),
  `GBFlyerEnemy.*`, `GBDroneEnemy.*`, `GBSentinelEnemy.*`.
- Edited: `AI/GBEnemyCharacter.*`, `Glyphbound.Build.cs`, `Core/GBDevCommands.cpp` (the `Enemies3D` smoke steps only),
  `Tests/GBAITests.cpp`, `Config/DefaultGame.ini`, `Content/Data/Enemies.json`, `Content/Data/Encounters.json`.

Look for real defects, most severe first:
1. Crashes and lifetime bugs: dangling pointers, components created at runtime without registration, GC-unsafe raw
   pointers, null asset paths, anim proxy thread safety (the proxy runs on worker threads: no UObject access there
   beyond what was copied in PreUpdate/Update), division by zero, NaN rotations.
2. Gameplay bugs: enemies that never notice, never stop shooting, shoot through walls, hit other enemies, keep firing
   while dead, ragdolls or corpses that never get removed, encounter accounting broken (an enemy that dies without
   OnDied, or is counted twice), flyers that sink into floors or fly through walls, lunges that hit from too far.
3. Data: keys the code reads that Enemies.json does not set or vice versa, unit mistakes (cm vs m, degrees vs radians).
4. Tests that a wrong implementation would still pass (project rule: each test must be able to fail).
Ignore style. For each finding: file:line, what goes wrong, a concrete scenario, and the smallest fix. End with a
one-line verdict.
