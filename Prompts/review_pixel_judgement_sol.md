# Review: pixel Judgement marks, smoke filter changes, sprite outline option (read-only)

Unreal Engine 5.9 C++ game (Glyphbound) plus Blender/numpy sprite tools. Report only real defects with file, line and a
concrete failure scenario. No style notes. If nothing, say so.

The change: `git show 0762d66 -- Glyphbound/Source Tools/Blender/sprite_pixel.py Tools/Blender/sprite_bake.py Tools/Jam/Verify-Jam.ps1`.
- `AI/GBValkyrieEnemy.cpp` AGBJudgementMark: when the caster HasSpriteLook, flat pixel sprites (SetupFlatSprite) for the
  disc and a sigil component added to RingParts, early return from Init, a baked pixel blast (AGBPixelVfxActor) on
  Strike instead of the pillar and Niagara impact, a quarter of the light.
- `Core/GBDevCommands.cpp`: the gb.Smoke filter compares names with spaces removed; in a filtered run the quit step
  moves to 5 s after the last kept step (FogVisit keeps its far safety net). Could a filtered block now quit before
  work it does after its last step (tickers, state machines that do not hold the step clock)?
- `sprite_pixel.py` outer_outline / fill_pinholes take a `bare` mask (regions with outline false); `sprite_bake.py`
  builds it from the job's regions.
- `Tools/Jam/Verify-Jam.ps1`: does its green/red logic hold (a stage that crashes, a timeout, a missing line)?
