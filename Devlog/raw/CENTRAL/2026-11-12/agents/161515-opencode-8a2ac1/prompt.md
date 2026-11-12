# Task: overlapping ground spells - the last one placed is on top, no flicker

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

The owner played (translated): "When I place the lightning rune first and then the ice floor, the ice floor must completely cover the part that overlaps the rune. If it is the other way round, the rune must be above the ice floor, but without the flickering we see in the game." GDD v0.19, section 18 "Overlapping ground spells".

Cause: both figures (`Figure` in `Spells/GBSpell_Trap.cpp` and `Spells/GBSpell_Zone.cpp`, set up by `GBPlaceholder::SetupFlatSprite`) sit at the same height (`TrapFigureLiftCm` / `ZoneFigureLiftCm` = 3 cm) above their ground point, so overlapping quads z-fight.

## The rule (implement exactly this)
Each new ground figure is placed **above the highest live ground figure it overlaps**:
- `figureZ = max(groundZ + GroundFigureLift, highestOverlappingFigureZ + GroundFigureLayerStep)`.
- "Overlaps" = another live ground figure whose horizontal (XY) distance between centers is less than the sum of the two radii, and whose figure world Z is within `GroundFigureLayerMaxRise` above or below this one's ground point (so figures on a different floor are ignored).
- Figures placed later always end on top; a figure never moves after it is placed. When figures expire, the others stay where they are.
- New data keys in `Glyphbound/Content/Data/Game.json` `Values`: `GroundFigureLift` 3, `GroundFigureLayerStep` 1.5, `GroundFigureLayerMaxRise` 60 (cm). Read them like the other `Game.json` values; remove the two hardcoded lift constants.

## Where it goes
- Pure function (testable), in `Spells/GBSpellActor.h/.cpp` as a static of `AGBSpellActor`:
  `static float ComputeGroundFigureZ(const FVector& GroundPoint, float Radius, const TArray<FVector4>& Existing, float Lift, float LayerStep, float MaxRise);`
  where each `FVector4` of `Existing` is (figure center X, Y, figure world Z, radius). Returns the world Z for the new figure by the rule above.
- In `AGBSpellActor`: a protected helper `void PlaceGroundFigure(UStaticMeshComponent* Figure, float Radius);` that collects the live ground figures of the world, calls the pure function and sets the figure's world Z (keep X/Y at the actor). A spell actor counts as a live ground figure when it was placed with this helper and has not ended (keep a flag + the radius on the actor; iterate `TActorIterator<AGBSpellActor>`; skip itself and actors that are ending or pending kill).
- `GBSpell_Trap` and `GBSpell_Zone` call `PlaceGroundFigure` in `StartSpell` instead of setting the relative location by hand, for both the sprite figure and the no-sprite disc fallback (for the disc keep its half thickness on top, as today).
- Nothing changes in damage, trigger areas, durations or arming.

## Test (pure logic, `Source/Glyphbound/Tests/`, an existing spell test file if one fits)
`Glyphbound.Spells.GroundFigureLayering`: no existing figures -> ground + lift; one overlapping figure at ground + lift -> one step above it; two overlapping figures stacked -> above the higher one; a figure that does not overlap (too far) -> ignored; a figure on a floor far above (more than MaxRise) -> ignored; the newest is always strictly above everything it overlaps. Each case must be able to fail. Check the name rule (no test name is a prefix of another).

## Smoke script (`Core/GBDevCommands.cpp`)
The ground spell steps at the end cast the rune close and the ice floor far, so they do not overlap. Change them so the ice floor is cast **overlapping about half of the rune** (rune first, then the ice floor), and log both figure world Z values: `SMOKE ground layers runeZ=%.1f floorZ=%.1f`. Log a `UE_LOG(LogGB, Error, ...)` if the floor's figure is not above the rune's. Keep the screenshot `10_ground_spells` (one second after the cast), framed so the overlap is visible.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 27` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the `SMOKE ground layers` line, and no `LogGB` warning. Open `Glyphbound/Saved/Screenshots/Smoke/10_ground_spells.png` and describe the overlap in the report (the ice must hide the rune where they overlap, with no dotted / mixed pixels).
Never make an error disappear by deleting behavior. All JSON files stay valid. No gameplay numbers in code.

## Final report (plain text)
1. The four result lines verbatim, plus the `SMOKE ground layers` line.
2. What you changed, with file names.
3. Your description of the screenshot.
4. Anything you could not do as described, and why.
5. What a person should check by playing.
