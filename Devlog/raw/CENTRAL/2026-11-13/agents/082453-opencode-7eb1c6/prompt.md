# Task: GBPixelVfx, second batch (fixes after the architect's look at the results and the code review)

You are the implementer. Same rules as `Prompts/pixelvfx_implement.md` (read its sections "File ownership", "Iteration rule" and "Final report" again: they apply unchanged). You own only `Glyphbound/Plugins/GBPixelVfx/**` except `Bake/Jobs/*.json`. Build, test and run only through `Tools/Jam/PixelVfx-Jam.ps1` (no `-Project` argument needed any more). Do not commit.

State: the first batch works end to end for bakes (FluidFire, FluidSmoke, FluidExplosion, FluidArcaneFire bake cleanly; the architect tuned the job files since, so the clips on disk are newer than the first batch's). The showcase did not show the clips. This batch fixes the runtime, improves the conversion and finishes the showcase and compare modes.

Work through the items IN ORDER. After each group: `-Action Build`, then the check named in the group. Budget: if one item fights you for more than 15 minutes, write down what you tried, leave it, and go on to the next one.

## A. Runtime component (`Source/GBPixelVfx`)
A1. **Pivot and light anchor are applied in the wrong space** (`UGBPixelVfxComponent::UpdateQuadTransform`). `PivotWorldOffset()` is in the quad's own XY (X = U to the right, Y = V up), but it is passed to `SetRelativeLocation`, which is in the parent's space and is not rotated with the quad. For `Upright` and `Billboard` the sprite ends up centered on the component (half under the floor) and shifted sideways. Correct rule: compute the quad's world rotation `R` first, using the COMPONENT's world location (not the quad's) for the direction to the camera; then quad world location = `ComponentLocation - R.RotateVector(PivotLocal)` with `PivotLocal = ((PivotU - 0.5) * W, (0.5 - PivotV) * H, 0)`; light world location = `ComponentLocation + R.RotateVector(((AnchorU - PivotU) * W, (PivotV - AnchorV) * H, 0))` plus 10 cm toward the camera (along R's Z axis) so the light is not inside the quad plane. Set world location and rotation of the quad and the light directly (`SetWorldLocationAndRotation`). `Ground` and `Fixed` use the component's own rotation as `R` (Ground: the quad lies flat in the component's XY plane, facing +Z).
A2. Put that math in a pure static function in `GBPixelVfxTypes.h` (inputs: clip, size scale, component location, rotation R; outputs: quad location, light location) and add two tests: for an upright quad (R such that local Y = world up) with `PivotV = 1` the quad center is exactly `H / 2` above the component location and not displaced horizontally; the light for `AnchorV = 0.5, PivotV = 1` is `H / 2` above the component.
A3. Verify the sprite is neither mirrored nor upside down. Do it for real, not by reasoning about the plane's UVs: add a temporary test clip (an 8x8 atlas with a known asymmetric mark, e.g. a red pixel block in the top-left corner) or use the showcase close shot of FluidSmoke (the fire is at the BOTTOM of that clip) and look at the screenshot.
A4. Any finding listed under "Review findings" below that touches the runtime module.

## B. Conversion (`GBPixelVfxConvert`), each with a test that can fail
B1. **Color clusters** (new setting `MinColorClusterPixels`, job key `Pixel.MinColorClusterPixels`, default 0 = off): after islands / holes and before the outline, every 4-connected region of solid pixels with the same ramp and index that is smaller than N pixels is recolored with the most common color among the solid pixels 4-adjacent to the region (ties: the darker one). One pass, regions found on the state before the pass. This removes single-pixel color specks inside the flames: a pixel artist paints clusters, a filter leaves salt and pepper.
B2. **Fade-out for one-shot clips** (new settings `FadeOutFrames`, default 0, and `FadeOutMode` "Dissolve" | "Erode", job keys under `Pixel`): only when `Loop` is false. Over the last `FadeOutFrames` frames, progress `t` goes from 1/(N+1) to N/(N+1). "Dissolve": a solid pixel is removed when `Bayer4x4(x, y) < t` (the classic 4x4 ordered matrix normalized to 0..1; the pattern is fixed in frame space so removed pixels never come back). "Erode": remove every solid pixel within `ceil(t * MaxDepth)` 4-connected steps of a transparent pixel or the frame edge, where `MaxDepth` is the largest such distance in that frame. Applied after color clusters, before the outline. The light track is computed after the fade (a faded frame emits less).
B3. Review images: the cells of `sheet.png` and `frame_big.png` must be opaque with a dark background `#16161C` (today the cells are transparent, so viewers show them white and fire is hard to judge); gutters stay `#3A3A3A`.
B4. Any finding listed under "Review findings" below that touches the conversion or its tests.
Check: `-Action Test` green, then `-Action Bake -Id All` and look at the four `sheet.png`.

## C. Showcase and compare (`GBPixelVfxBakeGameMode_Stage.cpp`)
The first batch's showcase screenshots were black above the floor: no clip visible, no light visible, and the last run died with `DXGI_ERROR_DEVICE_HUNG` inside `DistanceFieldShadows` of `Entry.DirectionalLight_0` (log `Glyphbound/Saved/Logs/pixelvfx_showcase_20261113-081556.log`). The diagnostic line said `cam=V(0)`: check first whether the camera really is where you think (a camera at the world origin looks along the floor from inside the row of clips, which is exactly what the screenshot shows).
C1. Remove every temporary probe (probe meshes, atlas dumps, `Tools/probe_material.py`, `Tools/dump_material.py`, any `read_show.ps1` you left anywhere, extra diag lines). Keep one useful diag line per mode.
C2. Stage: the fill directional light must not use distance-field shadows (`bUseRayTracedDistanceFieldShadows = false`, and `SetCastShadows(false)` is fine for the fill: the clip lights are the ones that must cast shadows). Give the stage a fixed manual exposure so dark scenes are not auto-exposed: a post-process on the camera (`AutoExposureMethod = AEM_Manual`, `AutoExposureBias` chosen so the lit floor is readable and the sprites are not blown out; override flags set) and bloom at default.
C3. Camera: spawn the `ACameraActor` with its final transform in the spawn call, set it as view target with `SetViewTargetWithBlend(Camera, 0)`, and log its real location and the view target's name. Lay the stage out along world Y: clips at X = 0, Y = -900, -300, 300, 900 (more clips: keep 600 cm spacing, center the row), back wall at X = +500, obstacles (boxes / cylinders 80 to 250 cm tall) at X = +250 between the clips and the wall and a few at X = -200 beside the clips, camera at X = -1400, Z = 350, looking at (0, 0, 180). Close shots: camera 600 cm in front of each clip at Z = 200.
C4. After A1 the clips stand on the floor at their pivot. Check in the screenshots: each sprite upright, base on the floor, fire-colored light on the floor and the wall, shadows of the obstacles cast away from the fire clips.
C5. Compare mode as specified in the first brief, with the notes' rules (wall clock, caps off, warm-up, baked population includes its lights). If the source Niagara system does not show in the main view, say so in the report with the log lines; the timing numbers are still valid only if the simulation runs, so also log the source component's `IsActive()` and its system instance tick count or age.
Check: `-Action Showcase` (look at every screenshot yourself), `-Action Compare -Id FluidFire`.

## Review findings (Sol 6.1) and the architect's decision on each
All 18 findings are ACCEPTED: fix every one, in this batch (full text below, verbatim from the reviewer). Architect's notes on some of them:
- 1: this is item A1 above; use the reviewer's formulation (rotation first, then `Quad = Component - L*p`, `Light = Quad + L*a`), keep the 10 cm push of the light toward the camera.
- 3: implement `OnComponentDestroyed` (or `DestroyComponent` override, whichever 5.9 offers) that calls `Stop()`; also `EndPlay`.
- 4: lit variant emissive = color * Glow (unlit stays color * (1 + Glow)). Re-run `-Action Materials` after changing the script.
- 8: include the projected source origin in the auto-frame box; never clamp the pivot.
- 12: code defaults = the notes (MinIslandPixels 0, FillHoles false, MinColorClusterPixels 0, FadeOutFrames 0); a job without `Ramps` uses one default ramp (the fire ramp of FluidFire) and logs a Warning.
- 13 to 18 (tests): strengthen the named tests as described. For 18 add at most three small tests (percentile peak, light-track energy and anchor, un-premultiply with fractional opacity). Keep the whole plugin suite under 3 seconds.

1. **MAJOR — GBPixelVfxComponent.cpp:252,317:** Confirmed: pivot and light offsets are expressed in quad XY but applied in the parent’s space. An Upright fire with PivotV=0.94 shifts sideways and puts its light beside the flame. Choose rotation first; with the quad’s final world linear transform `L` and mesh-local pivot/anchor points `p/a`, use `QuadWorldLocation = ComponentWorldLocation - L*p` and `LightWorldLocation = QuadWorldLocation + L*a`.

2. **MAJOR — GBPixelVfxComponent.cpp:95:** Atlas, material variant, and Glow are assigned only when creating the quad. Calling `Play(FluidArcaneFire)` after `Play(FluidFire)` without `Stop()` keeps the fire atlas, while switching to a lit clip keeps the unlit material. Refresh clip-dependent material state on every successful Play.

3. **MAJOR — GBPixelVfxComponent.cpp:105; GBPixelVfxComponent.h:64:** The spawned mesh/light belong to the owner actor, but destroying just the VFX component never calls Stop. Removing a looping VFX component from a surviving actor leaves its detached sprite and light registered; UE’s scene-component destruction reparents/detaches children rather than destroying them. Override destruction to explicitly destroy both spawned components.

4. **MAJOR — Tools/make_pixelvfx_material.py:193,201:** Both material variants use emissive `color*(1+Glow)`. A lit smoke clip with Glow=0 therefore still emits its full color in complete darkness. Connect `color*Glow` to emissive for the lit variant.

5. **MAJOR — GBPixelVfxConvert.cpp:64:** Nearest matching tone-maps palette candidates as well as the captured color, changing the target palette’s positions in OKLab. With Peak=1, white input, and palette black/#BCBCBC/white, this selects white instead of the grey nearest the tone-mapped input. Convert palette colors directly from sRGB to linear and then OKLab; tone-map only the input.

6. **MAJOR — GBPixelVfxBakeGameMode_Bake.cpp:273,467:** The “full scene” fallback retains `PRM_UseShowOnlyList` while omitting its sole component. UE’s `GetShowOnlyAndHiddenComponents` consequently creates an empty visibility set, so an empty first capture guarantees another empty capture. Set `PRM_LegacySceneCapture` when `bLegacyCapture` is true.

7. **MAJOR — GBPixelVfxBakeGameMode_Bake.cpp:853,887,901:** These failure branches increment BakeJobIndex after BakeLogAndCleanup already increments it. During an All bake, a system-load, readiness, or readback failure silently skips the following job. Advance the index in exactly one place.

8. **MAJOR — GBPixelVfxBakeGameMode_Bake.cpp:649:** Clamping the projected pivot silently breaks alignment when auto-framing excludes the source origin. If the visible plume starts 100 cm above its emitter, clamping its below-frame pivot makes runtime playback start at the emitter instead. Include the origin in the framing bounds before fitting the aspect ratio, so the stored pivot remains valid without clamping.

9. **MINOR — GBPixelVfxComponent.cpp:270:** Ground uses zero relative rotation, so it inherits the component’s rotation. A Ground clip spawned on an actor pitched 90 degrees becomes vertical. Set its world rotation so its normal remains world +Z.

10. **MINOR — GBPixelVfxBakeGameMode_Bake.cpp:439:** Auto-framing evaluates luminance before applying Capture.Exposure, whereas conversion evaluates it afterward. Raising exposure can make outer pixels visible in the final pass after the prepass has cropped them away. Apply the same exposure when calculating the prepass alpha rule.

11. **MINOR — GBPixelVfxBakeGameMode_Bake.cpp:374:** Rounding one capture stride changes animation speed for non-divisible rates. SimFps=60/Fps=24 captures every three simulation frames, recording 20 fps while declaring 24 fps. Schedule each sample independently with `WarmupFrames + Round(i*SimFps/Fps)`.

12. **MINOR — GBPixelVfxBakeJob.cpp:229; GBPixelVfxBakeJob.h:38,43,46:** Optional defaults diverge from the job contract: omitted Ramps produces a failed bake, omitted levels/alpha values differ from FluidFire, and MinIslandPixels defaults to 2 despite the notes requiring cleanup off. Initialize the required defaults and preserve default ramps when the array is absent.

13. **MINOR — GBPixelVfxConvertTests.cpp:121:** RampGradient passes `floor(v*(N-1))`, despite that implementation placing band boundaries incorrectly. Assert exact indices immediately below, at, and above several `k/N` boundaries.

14. **MINOR — GBPixelVfxConvertTests.cpp:159:** SplitRouting’s hot pixel exceeds both hot thresholds, so replacing the required OR with AND still passes. Add separate luminance-only and saturation-only hot pixels, and assert independently calculated hot/cold levels.

15. **MINOR — GBPixelVfxConvertTests.cpp:369:** LoopBlend passes an implementation that blends only frame 0 or leaves opacity untouched: frame 1 is unexamined and every opacity equals 1. Use distinct extra frames and opacities, then assert both blended frames and the non-loop case.

16. **MINOR — GBPixelVfxConvertTests.cpp:259:** IslandsAndHoles passes a hole filler that fills every transparent pixel and always copies its first neighbor. The fixture has just one transparent pixel and identical neighbor colors. Add an unfillable gap and a hole whose majority color differs from its first neighbor.

17. **MINOR — GBPixelVfxConvertTests.cpp:398,417:** PlaybackHelpers passes a one-shot implementation that finishes when the last frame begins, and serialization that replaces every track value with 1. Assert the last frame remains unfinished until `FrameCount/Fps`, and compare every round-tripped track entry.

18. **MINOR — GBPixelVfxConvertTests.cpp:61:** No tests exercise percentile selection, fractional-opacity un-premultiplication, light-track energy/centroids, or hysteresis across disappearance and rapid coverage changes. Implementations using maximum brightness, multiplying RGB by alpha again, returning constant lighting, or retaining disappeared pixels can escape the suite. Add small fixtures with hand-calculated peaks, colors, normalized energies, anchors, and solid masks.

Fix first tonight: **1 (pivot/light transforms), 2 (clip switching), 3 (component destruction cleanup).**

## Final report
As in the first brief: files changed, the verbatim result lines of the last Build, Test, Bake All, Showcase and Compare, what differs from this brief and why, what you could not verify, and what still looks wrong in the screenshots.
