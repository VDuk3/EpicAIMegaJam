Review these files of an Unreal Engine 5.9 game-jam project (read-only; do not change anything):

- Glyphbound/Source/Glyphbound/World/GBSeal.h and GBSeal.cpp: only what is new, the property `TravelMapPath`, the
  function `HasTravelMap()` and its use at the top of `AGBSeal::Tick` (a seal that opens another map instead of
  switching the era in place).
- Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp: only the new block for tour spots whose name contains "sealmap".
- Tools/Jam/Build-PastCastle.py (runs inside the Unreal Editor's Python through an MCP "execute python" tool that allows
  top-level await; `unreal` and `asyncio` are already imported there). It builds the map /Game/Glyphbound/Maps/PastCastle.
- Tools/Jam/Restore-CastlePack.ps1 (Windows PowerShell 5.1: moves files back from the Recycle Bin by reading its $I records).

Context for the builder. Frame: +X is the way the player advances, the lower ward is Z = 0. Pieces of the castle pack are
placed again from JSON records of the author's levels (`place_level`: a level's origin lands on `to`, turned `yaw`).
Measured piece facts the code relies on:
- SM_Curtain_Wall_01: 4 m along its own +X from its pivot, 4.3 m thick centred on its pivot line, crenels on its own -Y side.
- SM_Wall_1x5: 4 m along +X from its pivot, 1.25 m thick (Y -16..110), 5 m high.
- SM_Stairs_4M: occupies X -811..9, Y 0..297 from its pivot and rises 2.1 m towards +Y.
- SM_CastleTower_Floor_A_01, SM_Herringbone_4M, SM_Planks_4M: 4 x 4 m floor tiles occupying +X, +Y from their pivot.
- SM_FloorCorridorStair_01: occupies X 0..800, Y 0..800 from its pivot and rises 3 m towards +X.
- SM_CorridorWallStair_01: X 0..800, Y 1..33. SM_CeilingVaultedStair_01: X -18..800, Y -800..0, rising towards +X.
- SM_FloorCorridor and SM_CeilingVaulted_01: X 0..800, Y -800..0. SM_CorridorWall_01: X 0..800, Y -32..0, 7.5 m high.
- SM_CorridorCellWall_01: X -800..0, Y -72..72, with its door opening at X -600..-200.

Look for real defects only:
- wrong maths in a placement (a rotated piece that lands outside the cell the comment says it fills, a wall on the wrong
  side, `wall_run` producing a gap or an overlap at a corner, `over_pit` leaving out the wrong floor tiles);
- a stage that fails or leaves actors behind when it is run again (every actor must carry the stage tag);
- the seal: any way the new path can open the map twice, leave `bTravelPending` set, or travel when the player is dead;
  whether `FPackageName::DoesPackageExist` is the right check for a cooked, IoStore-packaged build;
- the tour check: can it pass when it should fail?
- Restore-CastlePack.ps1: a parsing mistake of the $I record (header 8 bytes, size 8, time 8, path length 4, UTF-16 path),
  or a case in which it could overwrite or lose a file.

Do not report style. Answer with a short list: file:line, the defect, the concrete failure. If there is nothing, say so in one line.
