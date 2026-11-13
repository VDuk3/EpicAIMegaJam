# GBPixelVfx

Bakes a realistic Niagara or Niagara Fluids effect into a pixel-art flipbook in the game's palette, and plays that
flipbook in the game on a camera-facing quad with a real point light that follows the effect's brightness.
Made during the Epic AI MegaJam 2026 for Glyphbound. Nothing here comes from an earlier project.

- The game ships only the flipbook (a small PNG and a JSON file per effect) and the Runtime module. Niagara Fluids
  is needed only on the machine that bakes; it is never enabled in the game's `.uproject`.
- "Pixel art" here is not a screen filter: each frame is downsampled, mapped to one of the palette ramps of
  `Design/VisualStyleGuide.md` with hard alpha, steadied over time (hysteresis) and cleaned (islands, holes, colour
  specks). The result is a sprite an artist can open and retouch.
- The effect still lights the scene: the bake measures how bright each frame is and where its hot centre sits, and
  the runtime drives a point light with that track. The light casts shadows and shows in Lumen reflections.

## Modules

| Module | Type | What it does |
|---|---|---|
| `GBPixelVfx` | Runtime | Loads `Content/Data/Vfx/<Id>.json` + `.png`, builds a transient nearest-filtered texture, plays the clip (`UGBPixelVfxComponent`, `AGBPixelVfxActor::SpawnPixelVfx`, console command `pvfx.Spawn <Id> [SizeScale] [DistanceCm]`, not in Shipping builds). No dependency on Niagara. |
| `GBPixelVfxBaker` | UncookedOnly | The bake: a game mode that runs the source system in an offscreen `-game` process, captures it, converts it on the CPU (`GBPixelVfxConvert`, pure and unit-tested) and writes the clip. Also the showcase and compare stages. Inert unless a `-Pvfx...` switch is on the command line. |

## How to use it

Everything goes through `Tools/Jam/PixelVfx-Jam.ps1` (it takes the machine lock, so it never overlaps a game build):

```
powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Build        # Editor target with the plugin
powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Materials    # creates M_PixelVfxSprite, M_PixelVfxSpriteLit, M_PixelVfxStage
powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Test         # the conversion tests (15, about 15 s)
powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Bake -Id All # bakes every job in Bake/Jobs (about 50 s for four)
powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Showcase     # screenshots of every clip on a lit stage
powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Compare -Id FluidFire   # frame time: 8 source systems vs 8 baked clips
node Tools/Jam/pixelvfx_previews.js                              # an animated GIF per clip (needs ffmpeg)
```

`-HardwareRT` on Showcase or Compare runs that one process with hardware ray tracing (the project's settings stay
as they are). The materials are `.uasset` files, which this repository does not track: run `-Action Materials`
once on each machine that packages the game.

Output of a bake: `Glyphbound/Content/Data/Vfx/<Id>.png` and `<Id>.json` (tracked in git, staged as loose files),
and review images in `Glyphbound/Saved/PixelVfx/<Id>/` (`sheet.png`, `source.png`, `frame_big.png`).

## A bake job

One JSON file per effect in `Bake/Jobs/`. The main keys:

- `System`: the Niagara system to bake, plus optional `UserFloats`, `UserVectors`, `UserColors`.
- `Capture`: `Resolution`, `AutoFrame`, `ViewYawDeg`, `ViewPitchDeg`, `FovDeg`, `WarmupSeconds`, `DurationSeconds`,
  `Exposure`, `KeyLight`.
- `Pixel`: `FrameWidth`, `FrameHeight`, `Fps`, `Loop`, `LoopBlendFrames`, `AlphaMode` (Opacity, Luminance, Max),
  `AlphaThreshold`, `ColorMode` (Ramp, Split hot and cold, Nearest), `Ramps`, `LevelsPercentile`, `Gamma`,
  `Hysteresis`, `MinIslandPixels`, `FillHoles`, `MinColorClusterPixels`, `Outline`, and for one-shots
  `FadeOutFrames` with `FadeOutMode` (Dissolve or Erode).
- `Light`: `Enabled`, `Color`, `Intensity` (candelas), `RadiusCm`, `CastShadows`, `TrackMode`.
- `World`: `Facing` (Upright, Billboard, Ground, Fixed), `Glow`, `Lit`.

The four jobs in the repository bake Epic's Niagara Fluids gas templates (fire, a smoke column with fire at its
base, an explosion) and a fourth clip, arcane fire, from the same fire simulation with the arcane ramp.

## Limits (known and accepted)

- A baked clip is one view of the effect: it does not react to wind, collisions or the player the way the live
  simulation does. What carries over is the look, the timing and the light.
- The light is one point light per clip, placed at the clip's bright centre: a stylised approximation of the
  volume's own lighting, not a simulation of it.
- Clip frames here are 48 x 64 or 64 x 64 at 12 fps, larger and faster than the 6 fps the style guide gives for
  character sprites, because fluid motion reads badly below that.
