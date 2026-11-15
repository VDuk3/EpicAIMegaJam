# Pixel-art character sprite sheets: the format (overnight 2026-11-15, architect)

Shared contract between the baker (`Tools/Blender/sprite_bake.py`, writes the sheets) and the game
(`UGBSpriteAnimComponent`, reads them). Castle (past) characters are pixel-art sprites that face the camera with 8 view
directions and animated clips, like the sprites of 90s first-person fantasy games.

## Files
`Glyphbound/Content/Data/Sprites/<SheetId>.png` (RGBA, 8-bit, transparent background, hard alpha: every pixel is
0 or 255) and `Glyphbound/Content/Data/Sprites/<SheetId>.json`. They ship as loose files next to the other data
(`Content/Data` is already staged as loose files; check `DirectoriesToAlwaysStageAsNonUFS` or how `Data/Vfx` ships).

## Layout
- A grid of cells, all the same size: `CellWidth` x `CellHeight` px.
- **8 columns = the 8 view directions.** Column `d` is the character seen from the angle `d * 45` degrees, measured
  from the character's forward direction, turning **counter-clockwise seen from above** (d=0: the viewer stands in front
  of the character and sees its face; d=2: the viewer stands at the character's LEFT side and sees its left flank;
  d=4: the back; d=6: its right flank).
- **Rows = frames.** Each clip occupies `Frames` consecutive rows starting at its `Row`.
- The character's ground point (between the feet, or under the body for a quadruped) is at pixel (`PivotX`, `PivotY`)
  of every cell (x right, y down, from the cell's top-left).

## JSON
```json
{
  "Id": "OrcWarrior",
  "CellWidth": 96, "CellHeight": 112,
  "PivotX": 48, "PivotY": 108,
  "PixelsPerMeter": 44,
  "Columns": 8,
  "Clips": {
    "Idle":   { "Row": 0,  "Frames": 4, "Fps": 6,  "Loop": true },
    "Walk":   { "Row": 4,  "Frames": 8, "Fps": 8,  "Loop": true },
    "Run":    { "Row": 12, "Frames": 6, "Fps": 10, "Loop": true },
    "Attack": { "Row": 18, "Frames": 6, "Fps": 10, "Loop": false, "HitFrame": 3 },
    "Hurt":   { "Row": 24, "Frames": 2, "Fps": 8,  "Loop": false },
    "Death":  { "Row": 26, "Frames": 6, "Fps": 8,  "Loop": false }
  },
  "Source": "what it was rendered from (mesh, clips, weapon), for the dev log"
}
```
- `PixelsPerMeter`: the sprite's world size is cell size / PixelsPerMeter metres (the same for every sheet, 44, so all
  characters share one pixel size on screen at equal distance, unless a sheet says otherwise).
- `HitFrame` (attack-like clips): the frame at which the blow lands or the projectile leaves (0-based).
- Optional per clip: `"Origin": [x, y]` in cell pixels where a projectile leaves (staff tip, bow), in the d=0 column.
- Clip names the game knows (others are allowed and ignored unless the enemy's data names them):
  `Idle`, `Walk`, `Run`, `Attack`, `Attack2`, `Cast`, `Shoot`, `Leap`, `Hurt`, `Death`, `Dormant`.
- `Death` is played once and its last frame is held.
