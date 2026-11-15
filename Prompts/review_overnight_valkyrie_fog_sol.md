# Review: the Valkyrie's fight and the tunnel smoke (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario:
a crash or null dereference, an actor or timer left alive after destroy or level change, a tick that keeps running or
writing forever, state that can get stuck (a fight phase never left, an action never ended), math that goes wrong
(NaN, division by zero, a sphere or box of size 0), a Niagara data-channel write that cannot reach the GPU simulation,
smoke checks that cannot fail or measure the wrong thing. No style notes. If nothing, say so.

1. The cyberpunk Valkyrie's fight: new files `Glyphbound/Source/Glyphbound/AI/GBValkyrieEnemy.h/.cpp` (`AGBValkyrieEnemy`
   from `AGBSkeletalEnemy`, plus `AGBJudgementMark`), the Action0..5 clip slots and the grip curl in
   `AI/GBEnemyAnimInstance.*`, her data in `Content/Data/Enemies.json` (`Valkyrie_Future`), the smoke block "Valkyrie:" in
   `Core/GBDevCommands.cpp`. She wakes sealed in her tube, leaps out, fights near (combo, dash, hop) and far (gun bursts,
   a fan of lances), casts Judgement (rings on the floor that strike after a telegraph) and enters a phase two.
2. The tunnel smoke: new `World/GBFluidFog.h/.cpp` (`AGBFluidFog`, a box with a Niagara Fluids Grid 3D gas system; it
   switches the simulation on near the player and writes the player, enemies, shots and projectiles each frame into the
   global data channel `/NiagaraFluids/NDC_Grid3D_Gas` as velocity sources with divergence so the gas parts around them),
   `Tests/GBFluidFogTests.cpp`, the static event `UGBCombatLibrary::OnShotTraced` broadcast from `Weapons/GBGun.cpp` and
   `AI/GBEnemy3D.cpp`, `Game.json` `FluidFog.*`, the smoke block "Fog:" in `Core/GBDevCommands.cpp`, and the two fog
   boxes placed by `Tools/Jam/Build-FutureBase.py` `fluid_fog()`.
See `git diff HEAD` for the changed files and read the new files whole.
