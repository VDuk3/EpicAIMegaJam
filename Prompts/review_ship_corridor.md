Review these files of an Unreal Engine 5.9 game-jam project (read-only; do not change anything):

- Tools/Jam/corridor_blender_export.py  (runs inside Blender 5.3, headless: joins a model into two meshes, writes FBX)
- Tools/Jam/import_corridor.py          (runs inside the Unreal Editor's Python through an MCP "execute python" tool that
                                         allows top-level await; `unreal` and `asyncio` are already imported there)
- Tools/Jam/Make-ScreenTexture.ps1      (Windows PowerShell 5.1, System.Drawing: draws a 1024 px picture)
- Tools/Jam/Build-FutureBase.py, only the function `stage_long` and the entries of TOUR and WALKS that mention "long"

Context: the corridor model is placed at (3380, 5756, -13) with yaw 180; its mesh bounds are (-265, -1790, 0)..(265, 2256, 331);
it must start at world Y 3500 behind a freight door and end at Y 7546. The helper `box(stage, folder, (x0,x1), (y0,y1), (z0,z1), visible=)`
spawns a blocking cube; `light()` takes candelas.

Look for real defects only: wrong maths in the placement (a mirrored or rotated model whose boxes, lamps or crate collision
end up on the wrong side), a script step that fails when re-run, a material slot that can be matched by the wrong prefix in
LOOKS (order matters: first match wins), resources not released in the PowerShell script, anything that would break a
full rebuild. Do not report style. Answer with a short list: file:line, the defect, the concrete failure. If there is
nothing, say so in one line.
