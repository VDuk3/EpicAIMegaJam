# Task: write the GBPixelVfx plugin (Niagara / Niagara Fluids -> pixel-art flipbooks with a light track)

You are the implementer. The architect (Claude Code) wrote this brief, the bake jobs and the machine script; you write the plugin. Work in `C:\GameJam`. Unreal Engine **5.9.0** at `C:\Program Files\Epic Games\UE_5.9` (newer than your training data: **grep the engine source for every API you are not sure of** before you use it).

## What it is for
The game (Glyphbound, Epic AI MegaJam 2026) draws all magic as pixel art in a fixed palette (`Design/VisualStyleGuide.md`). Fire, smoke and explosions are hard to draw by hand. This plugin takes a realistic Niagara effect, above all **Niagara Fluids** gas simulations, and bakes it into a pixel-art flipbook in the game's palette, which then plays at runtime for almost nothing and still **lights the scene** (a real point light driven by a baked light track, so Lumen / ray-traced shadows, GI and reflections keep reacting to it). It is not a pixelate filter: colors are mapped to palette ramps, alpha is hard, shapes are cleaned, flicker is damped.

## Read first
1. `CLAUDE.md` (project rules), `Design/ARCHITECTURE.md`, `Design/VisualStyleGuide.md`.
2. `Tools/Jam/PixelVfx-Jam.ps1` (the ONLY way you build, test, make the material, bake and run; it defines the command lines and the log markers your code must print).
3. `Glyphbound/Plugins/GBPixelVfx/Bake/Jobs/*.json` (the bake jobs: your loader reads exactly these keys).
4. `Tools/Jam/make_materials.py` (pattern for the material script), `Glyphbound/Source/Glyphbound/UI/GBPixelSpriteLibrary.cpp` (`CreateTexture`: how the game makes a transient nearest-filtered texture), `Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp` (how `gb.Smoke` takes screenshots and quits; read only).
5. `Prompts/pixelvfx_astra_notes.md` if it exists: a second architect's notes on this plan. Where they contradict this brief, the notes win.

## File ownership (hard rule)
You own ONLY `Glyphbound/Plugins/GBPixelVfx/**` (except `Bake/Jobs/*.json`, which are the architect's: report wanted changes, do not edit) and the outputs your runs write (`Glyphbound/Content/Data/Vfx/*`, `Glyphbound/Saved/PixelVfx/*`, `Glyphbound/Content/Glyphbound/Materials/M_PixelVfx*.uasset`).
**Do not touch anything else**: not `Glyphbound/Source/Glyphbound/**` (another chat has uncommitted work there), not `Glyphbound.uproject`, not `Config/`, not `Tools/`, not `Design/`. If a build fails in a file outside the plugin, stop and report it; do not fix it. Do not commit, do not push, do not launch other agents.

## Iteration rule (owner's rule, applies to you)
While you work on a part, run only the checks of that part, without a game window: `PixelVfx-Jam.ps1 -Action Build` (Editor target only), `-Action Test`, `-Action Bake -Id <one job>`. Never run the full smoke (`Smoke-Jam.ps1`), never the game target build, never the whole test suite, never the interactive Editor, never PIE: the architect runs the full checks once at the end. One build or run at a time (the script takes the machine lock by itself; if it says it is waiting for the machine, let it wait). Long commands: give them time (first bake compiles shaders and can take 10+ minutes).

## Plugin layout
```
Glyphbound/Plugins/GBPixelVfx/
  GBPixelVfx.uplugin
  Bake/Jobs/*.json                      (architect's)
  Tools/make_pixelvfx_material.py
  Source/GBPixelVfx/                    Runtime module (ships with the game). NO Niagara dependency.
    GBPixelVfx.Build.cs                 Core, CoreUObject, Engine, Json, ImageWrapper (+ what you verify you need)
    Public/GBPixelVfxTypes.h            clip data + enums + JSON <-> struct
    Public/GBPixelVfxSubsystem.h        UGBPixelVfxSubsystem
    Public/GBPixelVfxComponent.h        UGBPixelVfxComponent
    Public/GBPixelVfxActor.h            AGBPixelVfxActor + static spawn helper
    Private/...
  Source/GBPixelVfxBaker/               UncookedOnly module (Editor and uncooked -game only; never packaged)
    GBPixelVfxBaker.Build.cs            + Niagara, GBPixelVfx, ImageWrapper, Json, RenderCore, RHI as needed
    Public/GBPixelVfxConvert.h          pure conversion functions (namespace GBPixelVfxConvert), no UObject
    Public/GBPixelVfxBakeJob.h          FGBPixelVfxBakeJob + JSON loader
    Public/GBPixelVfxBakeGameMode.h     AGBPixelVfxBakeGameMode
    Private/... , Private/Tests/GBPixelVfxConvertTests.cpp
```
`.uplugin`: FileVersion 3, `"EnabledByDefault": false`, `"CanContainContent": false`, modules `GBPixelVfx` (Runtime, Default) and `GBPixelVfxBaker` (UncookedOnly, Default), plugin dependency `Niagara`. Log category `LogGBPixelVfx` (defined in the runtime module, exported). ASCII only, comments in English, class prefix `GBPixelVfx`. Module files are unity-built: file-local helper names must be unique across files of a module. Never name a member `Cast`.

## Data: the baked clip (written by the baker, read by the runtime)
`Glyphbound/Content/Data/Vfx/<Id>.png` (RGBA8 atlas, frames left to right, top to bottom, alpha 0 or 255 only) and `<Id>.json`:
```json
{
  "Id": "FluidFire", "Atlas": "FluidFire.png",
  "FrameWidth": 48, "FrameHeight": 64, "Columns": 8, "FrameCount": 24, "Fps": 12, "Loop": true,
  "WorldWidthCm": 310.0, "WorldHeightCm": 413.3,
  "PivotU": 0.5, "PivotV": 0.94,
  "Facing": "Upright", "Glow": 1.5,
  "Light": { "Enabled": true, "Color": "#F3610B", "Intensity": 12.0, "RadiusCm": 900, "CastShadows": true,
             "AnchorU": 0.5, "AnchorV": 0.6, "Track": [0.82, 0.9, 1.0] },
  "Source": { "System": "/NiagaraFluids/...", "DurationSeconds": 2.0, "SimFps": 60, "EngineVersion": "5.9.0", "BakedUtc": "..." }
}
```
- `WorldWidthCm/HeightCm`: the real size of the captured area, so the baked sprite fills the same space as the source effect.
- `PivotU/V` (0..1, V down): where the source system's origin falls inside the frame. The component places the quad so that this point sits at the component's location: a clip spawned where the Niagara system was spawned overlaps it.
- `Light.Track`: one value 0..1 per frame (1 = the brightest frame of the clip). `AnchorU/V`: average emissive centroid, where the light sits inside the frame. `Intensity` is candelas at track value 1.
At runtime the folder is `FPaths::ProjectContentDir() / "Data/Vfx"` (it is staged as loose files in the packaged game, see `Glyphbound/Config/DefaultGame.ini`).

## Runtime module `GBPixelVfx`
- `FGBPixelVfxClip` (plain struct, `GBPixelVfxTypes.h`) with the fields above + `bool LoadFromJsonString / SaveToJsonString` + pure helpers that tests can call: `static FVector4f FrameRect(const FGBPixelVfxClip&, int32 FrameIndex)` (UV offset.xy, scale.zw of a frame in the atlas) and `static int32 FrameAtTime(const FGBPixelVfxClip&, float Seconds, bool& bOutFinished)` (stepped: floor(Seconds * Fps); loops wrap; one-shots clamp to the last frame and set bOutFinished after it has been shown for one frame duration).
- `EGBPixelVfxFacing`: `Billboard` (quad faces the camera fully), `Upright` (rotates around world Z only: fire, smoke columns), `Ground` (flat on the floor, facing +Z), `Fixed` (keeps the component's rotation).
- `UGBPixelVfxSubsystem : UGameInstanceSubsystem`: `const FGBPixelVfxClip* FindClip(FName Id)` and `UTexture2D* GetAtlas(FName Id)`; loads `<Id>.json` and decodes the PNG with `IImageWrapperModule` into a **transient** `UTexture2D` (PF_B8G8R8A8, SRGB true, `TF_Nearest`, no mips, `TA_Clamp`, `NeverStream`), cached; `ListClipIds()` (scans the folder); `ClearCache()`. Missing or broken clip: one warning, return null, never crash.
- `UGBPixelVfxComponent : USceneComponent`:
  - `UFUNCTION(BlueprintCallable) bool Play(FName ClipId, bool bRestart = true)`, `Stop()`, `IsPlaying()`, properties `bAutoDestroyOwnerWhenFinished`, `PlayRate`, `SizeScale`, `LightIntensityScale`, `bEnableLight` (default true), `bLightCastsShadows` (default from the clip), `FacingOverride` (optional), `StartTimeOffset`.
  - Creates at runtime (in `Play`, not in the constructor) a `UStaticMeshComponent` child with `/Engine/BasicShapes/Plane` (100 cm, XY, faces +Z; check its UV orientation and rotate/flip so the sprite is upright and not mirrored when seen by the camera), no collision, no shadow casting, movable, and a `UMaterialInstanceDynamic` of `/Game/Glyphbound/Materials/M_PixelVfxSprite` (params below). If the material is missing: one warning and fall back to `/Engine/BasicShapes/BasicShapeMaterial` so something still shows.
  - Each tick: frame from time (stepped, never interpolated), set `FrameRect`; orient the quad by facing mode using the first local player's camera (`UGameplayStatics::GetPlayerCameraManager`); when there is no player camera keep the last rotation. Quad size = clip world size * `SizeScale`; position so the pivot rule above holds.
  - Light: a `UPointLightComponent` child (movable, `IntensityUnits = ELightUnits::Candelas`, `SetAttenuationRadius(RadiusCm * SizeScale)`, color from the clip, `SetCastShadows(clip/flag)`), placed at the anchor point, `SetIntensity(Clip.Light.Intensity * Track[frame] * LightIntensityScale)` each time the frame changes. No light component at all when the clip has `Light.Enabled` false or `bEnableLight` is false.
  - Ray tracing / Lumen flags on the quad: `SetVisibleInRayTracing(true)`, `bAffectDynamicIndirectLighting = true`, `bVisibleInReflectionCaptures = true`, `CastShadow = false` (the quad must not shadow its own light).
- `AGBPixelVfxActor`: an actor whose root is the component; `static AGBPixelVfxActor* SpawnPixelVfx(UObject* WorldContext, FName ClipId, const FTransform& Transform, float SizeScale = 1.f)` (one-shot clips destroy the actor when finished; loops live until destroyed).
- Console command `pvfx.Spawn <ClipId> [SizeScale]` (non-shipping, registered by the runtime module): spawns the clip 300 cm in front of the first player's view, on the floor under that point if a line trace finds one. This is how a person looks at a clip in the real game.

## Material script `Tools/make_pixelvfx_material.py`
Same pattern and helpers as `Tools/Jam/make_materials.py` (copy what you need into your file; do not import or edit theirs). Log prefix `make_pixelvfx_material:`; the last line on success is `make_pixelvfx_material: OK`. It builds, in `/Game/Glyphbound/Materials` (already in `DirectoriesToAlwaysCook`):
- `M_PixelVfxSprite`: Masked, Unlit, two-sided. Params: `Atlas` (texture object, sampled with the texture's own sampler "from texture asset" so nearest filtering survives), `FrameRect` (vector: UV = TexCoord0 * FrameRect.zw + FrameRect.xy), `Tint` (vector, default white), `Glow` (scalar, default 0). Emissive = sample.rgb * Tint.rgb * (1 + Glow). Opacity mask = sample.a (clip value 0.5).
- `M_PixelVfxStage`: Opaque, Default Lit. Params `Color` (vector), `Roughness` (scalar, default 0.6), `Metallic` (scalar, default 0). Used only by the showcase stage (a glossy floor to see reflections).

## Baker module `GBPixelVfxBaker`

### Entry point
`PixelVfx-Jam.ps1` starts `UnrealEditor.exe <project> /Engine/Maps/Entry?game=/Script/GBPixelVfxBaker.GBPixelVfxBakeGameMode -game ... -EnablePlugins=GBPixelVfx,NiagaraFluids` plus ONE of `-PvfxBake=<Id|All>`, `-PvfxCompare=<Id>`, `-PvfxShowcase`.
`AGBPixelVfxBakeGameMode` (DefaultPawnClass = nullptr, HUDClass = nullptr, spectator off): reads the mode with `FParse::Value(FCommandLine::Get(), ...)`, runs it as a small state machine in `Tick`, and at the end logs **`PVFX DONE`** and quits (`FPlatformMisc::RequestExit(false)` or the console `quit`). Every progress line is `UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX ..."))`; every problem is a `LogGBPixelVfx` Warning or Error (the script counts Errors as failure). A job that fails logs an Error and the run goes on to the next job. Guard against hangs: every wait has a frame budget, after which the job fails with an Error.

### Job loader (`FGBPixelVfxBakeJob`)
Reads `FPaths::ProjectPluginsDir() / "GBPixelVfx/Bake/Jobs/<Id>.json"` (All = every file there). Keys = exactly those in the job files (`Id`, `System`, `UserFloats`, `UserVectors`, `UserColors`, `Capture.*`, `Pixel.*`, `Light.*`, `World.*`); every key optional with the default that the FluidFire job shows; unknown keys ignored; `Comment` ignored. Colors are `#RRGGBB` sRGB strings. `Pixel.Ramps` is an array of ramps, each an array of colors from dark to light.

### Capture (per job)
1. Fixed time step for the whole process: `FApp::SetUseFixedTimeStep(true)`, `FApp::SetFixedDeltaTime(1.0 / SimFps)` (see the Astra notes for anything else).
2. Spawn an actor at the world origin with a `UNiagaraComponent`; `LoadObject<UNiagaraSystem>` the job's `System` (fail the job with an Error naming the path if it does not load: NiagaraFluids must be on the command line); apply user parameters (`SetVariableFloat`, `SetVariableVec3`, `SetVariableLinearColor`: check the 5.9 names); activate. Wait until the system has compiled and is ready (`UNiagaraSystem::IsReadyToRun` / `WaitForCompilationComplete`, check the names) before counting warm-up time.
3. Lighting for the capture: when `Capture.KeyLight` is true spawn a movable directional light coming from the **top left as seen by the capture camera** (style guide: light from the top left), intensity `KeyLightIntensity` (lux). No sky, no fog: the background must stay black.
4. A `USceneCaptureComponent2D`: **long-lens perspective** (`Capture.FovDeg`, default 10; see the notes, item 1; "Ortho" stays as an option with `OrthoWidth = WidthCm`), placed at `(WidthCm / 2) / tan(FovDeg / 2)` from `CenterCm` along the view direction given by `ViewYawDeg` / `ViewPitchDeg` (yaw 0 = the camera looks along +X, i.e. it sits at -X), `PrimitiveRenderMode = PRM_UseShowOnlyList` with the Niagara component in `ShowOnlyComponents`, `CaptureSource = SCS_SceneColorHDR` (RGB = scene color, A = inverse opacity), `bCaptureEveryFrame = false`, `bCaptureOnMovement = false`, `bAlwaysPersistRenderingState = true`, auto exposure / bloom / motion blur / tone mapper effects off in its show flags and post-process settings so frames are comparable. Render target `RTF_RGBA16f`, `Resolution` px wide, height = Resolution * FrameHeight / FrameWidth.
5. **Auto-frame pre-pass** (when `Capture.AutoFrame`): run the system once (warm-up + duration) capturing every 4th simulation frame at 256 px with the job's `WidthCm` / `CenterCm` as a generous first guess; compute the union bounding box (in world cm on the capture plane) of pixels whose alpha (rule below) is above `AlphaThreshold / 2`; expand by `AutoFramePadding` (fraction of the box size per side); keep the frame aspect (grow the short side); if the box touches the capture edge, log a Warning ("first guess too tight") and double `WidthCm` once and retry. Then destroy and re-spawn the system and do the real pass with that framing. If nothing is visible at all: Error "nothing visible", fail the job.
6. Real pass: wait `WarmupSeconds` of simulated time, then capture `FrameCount = round(DurationSeconds * Pixel.Fps)` frames, one every `SimFps / Fps` simulation frames: `CaptureScene()`, then read back with `FTextureRenderTargetResource::ReadLinearColorPixels` (or `ReadFloat16Pixels`; blocking is fine). Keep the raw `FLinearColor` frames in memory.
7. Per pixel: `Opacity = saturate(1 - A)`. Keep RGB as captured (linear HDR, already premultiplied by coverage against black).
8. Destroy the job's actors before the next job.

### Conversion (`GBPixelVfxConvert`, pure functions over `TArray<FLinearColor>` frames: unit-testable, no engine objects)
In this order:
1. **Exposure:** RGB *= `Capture.Exposure`.
2. **Loop blend** (only when `Loop` and `LoopBlendFrames` > 0, on the full-resolution linear frames): capture `LoopBlendFrames` EXTRA frames after the last one (so the baker really captures FrameCount + LoopBlendFrames); for i in [0, B): frame[i] = lerp(extra[i], frame[i], (i + 0.5) / B) on RGB and opacity. The extra frames are then dropped: the clip's last frame flows into the first.
3. **Downsample** each frame to `FrameWidth x FrameHeight` by box average (RGB and opacity).
4. **Alpha:** per pixel `Lum = dot(RGB, (0.2126, 0.7152, 0.0722))`; `AlphaMode` "Opacity": a = Opacity; "Luminance": a = saturate(Lum); "Max": a = max(Opacity, saturate(Lum)). Un-premultiply the color for the color step: `C = RGB / max(a, 1e-4)`.
5. **Levels:** `Peak` = the `LevelsPercentile` percentile of `Lum(C)` over all visible pixels (a >= AlphaThreshold) of ALL frames (one value per clip, so the clip does not pump); value `v = saturate(pow(Lum(C) / Peak, Gamma))`.
6. **Hard alpha:** pixel is solid when a >= `AlphaThreshold`, else fully transparent. No partial alpha, no dithering.
7. **Color mapping** to the palette:
   - "Ramp": index = floor(v * N) clamped, in ramp 0.
   - "Split": a pixel is HOT when its un-premultiplied HDR luminance (before levels) >= `HotLuminance` OR its saturation ((max-min)/max of C) >= `HotSaturation`; hot pixels use ramp 0 with v re-normalized among hot pixels (own percentile peak), the others use ramp 1 with v re-normalized among cold pixels. If the job has only one ramp, behave as "Ramp".
   - "Nearest": nearest color among all colors of all ramps, distance in OKLab, after tone-mapping C with `C / (1 + C)` scaled by 1/Peak.
8. **Temporal hysteresis:** when mapping frame f > 0, if the pixel was solid in frame f-1 with ramp index k (same ramp), and the new v lies within `Hysteresis` of the boundary between k and the new index, keep k. (Implement it as: the band of the previous index is widened by `Hysteresis` on both sides.) For a looping clip also run frame 0 against the last frame in a second pass.
9. **Cleanup:** per frame, remove 4-connected solid islands smaller than `MinIslandPixels`; when `FillHoles`, a transparent pixel whose 4 neighbors are all solid becomes solid with the most common neighbor color.
10. **Outline** `"None" | "Black" | "Darkest"`: transparent pixels 4-adjacent to a solid pixel become `#000000` or the darkest color of ramp 0. Never draw outside the frame.
11. **Light track:** per frame, `E = sum over solid HOT pixels (all solid pixels when the mode is not Split) of v`; track = E / max(E over frames); anchor = average over frames of the v-weighted centroid of those pixels, as U,V in 0..1. If the job's `Light.Enabled` is false, no track.
12. **Pivot:** project the system origin (world 0,0,0 of the job actor) on the capture plane -> `PivotU/V`.
Output colors are the palette colors exactly (sRGB bytes from the hex strings), alpha 0 or 255.

### Outputs of a bake
- `Content/Data/Vfx/<Id>.png` and `<Id>.json` (the clip; `Columns` = ceil(sqrt(FrameCount)) capped so the atlas stays <= 1024 px wide).
- Review files in `Glyphbound/Saved/PixelVfx/<Id>/`: `sheet.png` = every baked frame, upscaled x6 with nearest, on a mid-grey `#3A3A3A` background, 8 per row with 6 px gutters; `source.png` = the same frames before conversion (after exposure, tone-mapped with x/(1+x), composited on black, each scaled to the same cell size, smooth) so a person can compare source and result side by side; `frame_big.png` = the brightest frame upscaled x12.
- Log per job: `PVFX BAKE <Id>: frames=<n> frame=<w>x<h> world=<w>x<h> cm peak=<Peak> solid=<average % of solid pixels> colors=<distinct colors used> -> <png path>`.
- A job whose average solid coverage is below 1% or above 95% logs a Warning (framing or threshold is wrong).

### Showcase mode (`-PvfxShowcase`)
Builds a small stage from engine basic shapes: a floor 3000 x 2000 cm (dynamic instance of `M_PixelVfxStage`, dark, roughness 0.15 so reflections show), a back wall (roughness 0.7, grey), three boxes / cylinders of different heights standing between the clips and the wall so the clip lights throw visible shadows, one very dim directional light (so the stage is readable but the clip lights dominate), no sky. Spawns every clip found by `UGBPixelVfxSubsystem::ListClipIds()` in a row 600 cm apart, on the floor (`AGBPixelVfxActor`; one-shots are re-spawned when they finish). Spawns a camera (a `ACameraActor` set as the player controller's view target; if the game mode has no player controller, spawn one) that sees the whole row. Waits 2 s (real frames, not fixed step: restore `FApp::SetUseFixedTimeStep(false)` in this mode), takes `Saved/PixelVfx/Showcase/showcase_1.png`, then 0.7 s later `showcase_2.png`, then a close shot of each clip `showcase_<Id>.png`, using `FScreenshotRequest::RequestScreenshot(Path, false, false)` and waiting a few frames after each request (see how `gb.Smoke` does it). Logs `PVFX SHOWCASE: <n> clips, screenshots in <dir>`, then `PVFX DONE`.

### Compare mode (`-PvfxCompare=<Id>`): the "it is lighter" number
Same stage. Phase 0: one source Niagara system (from the job) on the left and the baked clip on the right, 500 cm apart; after the job's warm-up + 1 s take `Saved/PixelVfx/<Id>/compare.png`. Phase A: destroy both, spawn 8 instances of the SOURCE system in a 4 x 2 grid in view; let them run 3 s; then for 180 frames measure the real frame time (`FPlatformTime::Seconds()` between ticks; make sure nothing caps the frame rate: `t.MaxFPS 0`, `r.VSync 0`, fixed time step off) and the GPU frame time (`RHIGetGPUFrameCycles` -> ms; check the 5.9 name). Phase B: destroy them, spawn 8 baked clips at the same places, 1 s, measure 180 frames. Phase C: empty stage, 1 s, measure 180 frames (the baseline). Log one line:
`PVFX COMPARE <Id>: x8 source frame=<ms> gpu=<ms> | x8 baked frame=<ms> gpu=<ms> | empty frame=<ms> gpu=<ms>` and write the same numbers to `Saved/PixelVfx/<Id>/compare.json`. Then `PVFX DONE`.

## Tests (`Private/Tests/GBPixelVfxConvertTests.cpp`, names `GBPixelVfx.<Area>.<Name>`, no name a prefix of another)
Pure logic only, each must be able to fail (ask: which wrong implementation would still pass?). Six to eight tests, for example:
- hard alpha: an input with alpha just below / above the threshold gives exactly 0 / 255, and no output pixel ever has another alpha;
- ramp mapping: a horizontal luminance gradient gives non-decreasing ramp indices that use the first and the last color; every output color is one of the ramp's colors;
- split mode: a bright saturated pixel lands in ramp 0 and a grey dim one in ramp 1;
- hysteresis: a pixel whose value oscillates across a band edge by less than the margin keeps one color over 6 frames, and by clearly more than the margin changes;
- islands and holes: a 2-pixel island is removed when MinIslandPixels is 3 and kept when it is 2; a one-pixel hole is filled;
- outline: a solid 2x2 block in a 6x6 frame gets exactly its 8 4-adjacent pixels outlined, none outside the frame for a block at the corner;
- loop blend: with B blend frames, frame 0 of the result equals (mostly) the extra frame, and the last frame is unchanged;
- clip helpers: `FrameRect` for frame 9 of an 8-column atlas, `FrameAtTime` wrap for loops and finish for one-shots; JSON round trip of a clip keeps the light track.

## Order of work (stop after each step if it fails; fix before going on)
1. `.uplugin`, both `Build.cs`, module files, types, convert functions, tests -> `-Action Build`, `-Action Test` green.
2. Runtime subsystem, component, actor, material script -> `-Action Build`, `-Action Materials`.
3. Job loader, bake game mode (bake mode) -> `-Action Bake -Id FluidFire`. Open `Saved/PixelVfx/FluidFire/sheet.png` and `source.png` yourself (you can read images): if the source is black or empty, the capture is wrong (show-only list, alpha, exposure, warm-up, framing): fix it before anything else. Then `FluidSmoke`, `FluidExplosion`, `FluidArcaneFire`.
4. Showcase mode -> `-Action Showcase`; look at the screenshots.
5. Compare mode -> `-Action Compare -Id FluidFire`.
If a step fights you for more than about 20 minutes (for example the fluid does not render in a scene capture), write down exactly what you tried and what the log says, pick the fallback the notes give (perspective camera, no show-only list with the stage far from everything, luminance alpha), and go on: a working bake of ANY Niagara Fluids template beats a perfect plan.

## Final report (your last message, plain text)
1. Files written. 2. The verbatim result lines of the last Build, Test, Materials, each Bake, Showcase and Compare. 3. What differs from this brief and why. 4. Job values you would change (you may not edit the job files). 5. What you could not verify, and anything that looked wrong in the review images.
