# Code review 2: the GBPixelVfx plugin after the fix batch

You are the reviewer. Read-only: do not edit, build or run anything (you may run `git diff` and `git show`). Project `C:\GameJam`, Unreal Engine 5.9.0 at `C:\Program Files\Epic Games\UE_5.9` (check engine APIs in its source when a finding depends on them).

## Context
`Glyphbound/Plugins/GBPixelVfx/` bakes a Niagara / Niagara Fluids effect into a pixel-art flipbook in the game's palette (`Glyphbound/Content/Data/Vfx/<Id>.png` + `.json`) and plays it at runtime on a camera-facing quad with a point light driven by a baked light track. Your first review is in `Prompts/pixelvfx_fixes.md` (section with the 18 findings, all accepted). The implementer (Muse Spark) then worked on that brief, and the architect finished the showcase stage by hand. The changes are the working tree against commit `566ad63`:

`git diff 566ad63 -- Glyphbound/Plugins/GBPixelVfx/Source Glyphbound/Plugins/GBPixelVfx/Tools`

## What to review
1. For each of your 18 findings: is it really fixed in the working tree? Answer only for the ones that are NOT fixed, fixed wrongly, or fixed with a new bug. Do not list the ones that are fine.
2. The new code of the batch:
   - `FGBPixelVfxClip::ComputeQuadAndLightPlacement` and `UGBPixelVfxComponent::UpdateQuadTransform` (pivot rule: the point `PivotU/V` of the frame lands on the component's location for every facing mode; light at `AnchorU/V`, 10 cm toward the camera).
   - `RefreshClipMaterial` / `RebuildLight` / `Stop` / `OnComponentDestroyed` (GC safety, re-Play with another clip, no leaked components; a new dynamic material instance per Play: is that a leak or churn problem for a looping spell effect?).
   - Color cluster clean-up (`MinColorClusterPixels`), the one-shot fade-out (`FadeOutFrames`, `FadeOutMode` Dissolve / Erode) and the opaque review-sheet cells in `GBPixelVfxConvert.cpp` and the bake game mode: correctness, out-of-range indices, frames left with stray pixels, a fade that never reaches an empty last frame.
   - The bake game mode changes (origin included in the auto-frame, `PRM_LegacySceneCapture` fallback, job index on failure, exposure in the prepass, capture stride).
   - The new and rewritten tests in `Private/Tests/GBPixelVfxConvertTests.cpp`: for each one, name a wrong implementation that would still pass it; report only the tests that are really too weak.
3. Anything in the Runtime module (`Source/GBPixelVfx/**`) that would break or misbehave in the packaged (cooked, Shipping or Development) game: Editor-only API, paths, the materials `/Game/Glyphbound/Materials/M_PixelVfxSprite*` not being cooked when nothing references them by hard reference (they are loaded with `StaticLoadObject` by path; `DefaultGame.ini` has `DirectoriesToAlwaysCook` for `/Game/Glyphbound/Materials`: confirm that is enough), the loose `Content/Data/Vfx` files (`DirectoriesToAlwaysStageAsNonUFS=(Path="Data")`), the `pvfx.Spawn` console command in Shipping.
Do NOT review `GBPixelVfxBakeGameMode_Stage.cpp` (throwaway showcase / compare stage) unless something there can crash the bake path or leak into the packaged game.

## Output (English, at most 70 lines)
A numbered list of findings, most severe first. For each: severity (BLOCKER / MAJOR / MINOR), file:line, what is wrong, a concrete failing scenario, and the fix in one or two sentences. No praise, no style nits. If everything in an area is fine, say so in one line. End with one line: which findings must be fixed before the plugin is enabled in the game's `.uproject`.
