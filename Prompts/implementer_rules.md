# Rules for every implementer on Glyphbound (read first)

You implement one part of **Glyphbound**, a first-person RPG shooter for the Epic AI MegaJam 2026, in **Unreal Engine 5.9.0**, C++. The architect (another agent) wrote the public headers; you write the implementation. Several implementers work **at the same time** on different folders, so the rules about file ownership matter.

## Read before writing
1. `C:\GameJam\Design\ARCHITECTURE.md` (the whole file, it is short).
2. `C:\GameJam\Design\GDD.md`: the sections named in your task.
3. In `C:\GameJam\Glyphbound\Source\Glyphbound\`: `Core/GBTypes.h`, `Core/GBStatNames.h`, `Core/GBPlaceholder.h`, `Data/GBDataTypes.h`, `Data/GBDataSubsystem.h`, `Stats/GBStatsComponent.h`, `Stats/GBAttributeComponent.h`, `Combat/GBCombatLibrary.h`, plus every header your task names. The header comments are the specification: implement what they say.

## File ownership
- You own ONLY the files listed in your task. Create the `.cpp` for each header you own, and new files inside your folders if you need them.
- **Never edit a file you do not own.** If you need something from another module that its header does not offer, code against what exists, and write the request in your final report.
- In headers you own you MAY add includes, private/protected members, helper methods and extra public methods. You MUST NOT remove, rename or change the signature of anything already public there: other implementers are calling it right now.
- Do not edit `Glyphbound.Build.cs`, `.uproject`, or `Config/`. Module dependencies available: Core, CoreUObject, Engine, InputCore, EnhancedInput, Json, JsonUtilities, AIModule, NavigationSystem, Slate, SlateCore, UMG. If you need another one, say so in your report.

## Hard rules
- **Do not build, do not run the Editor, do not run tests, do not commit, do not launch other agents.** The architect integrates and builds after everyone finishes. Write code that compiles: check every engine API you are not sure about in the engine source at `C:\Program Files\Epic Games\UE_5.9\Engine\Source` (grep the header). UE 5.9 is newer than your training data; when in doubt, look.
- **No gameplay numbers in code.** Damage, costs, cooldowns, speeds, sizes, durations, timings and counts come from the JSON data through `UGBDataSubsystem` and `UGBStatsComponent` (stat names in `Core/GBStatNames.h`). If you need a number that has no stat yet, add a named constant `inline const FName` in a header YOU own (same style as `GBStatNames.h`), read it from the data with a sensible default, put its value in your JSON file, and list it in your report. Layout pixels and pure visual offsets may be code constants.
- Everything must work when a JSON entry is missing: use the default argument, log a warning with `UE_LOG(LogGB, Warning, ...)`, never crash, never divide by zero.
- No binary assets. 3D placeholders: `GBPlaceholder::SetupShape` (engine basic shapes). Pixel-art placeholders: `UGBPixelSpriteLibrary` (sprites described as text in `Sprites.json`). Art rule (GDD 3): medieval and magic things are pixel-art sprites; cyberpunk things are 3D.
- No custom collision channels or profiles that need config: use the engine's default channels and filter by team in code (`UGBCombatLibrary::AreHostile`).
- Do not keep pointers to definitions (`FGBSpellDefinition*`...) across frames; keep the id.
- A class member must never be named `Cast` (it hides the engine's `Cast<T>()`).
- All on-screen text in English. Comments in English, and only where they explain a decision or a non-obvious rule. ASCII only.
- Includes are relative to the module root: `#include "Stats/GBStatsComponent.h"`. Every `.cpp` includes its own header first. Every header with reflected types includes its `.generated.h` last.
- Keep it simple. This is a 4-day jam: clear, short code that works beats clever code. No speculative abstractions beyond what the headers define.

## Tests (only if your task asks for them)
- Pure logic only, no world, no assets: `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FName, "Glyphbound.<Area>.<Name>", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)` inside `#if WITH_DEV_AUTOMATION_TESTS`, in `Tests/GB<Area>Tests.cpp`.
- Each test must be able to fail: before writing it, ask which wrong implementation would still pass it. No tests that copy a constant, check that a field exists, or read source text.
- A test name must never be a prefix of another test name (`A.B` next to `A.B.C` silently stops `A.B`).
- Keep them few and fast (the whole suite must stay well under 30 s).

## Final report (your last message, plain text, short)
1. Files you wrote or changed.
2. Anything in your headers that you added to the public API.
3. Requests for other modules (what you needed and did not find).
4. Design values you had to invent (they will be marked provisional in the GDD).
5. Risks: API calls you could not verify, things you expect may not compile, behavior you are unsure about.
