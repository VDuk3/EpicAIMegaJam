# Code review: HUD and menus rework (two skins), Glyphbound (UE 5.9, C++)

You are reviewing, read-only, a rework of the game's whole on-screen UI. Everything is drawn with the HUD canvas
(`AHUD::DrawHUD`, no UMG widgets). Two skins switch with the held item: a 1990s first-person RPG look (staff and
sword) and a modern cyberpunk look (gun).

## Files to review (all under `Glyphbound/Source/Glyphbound/`)
- `UI/GBHUD.h` (class layout, painting API), `UI/GBHUDShared.h` (palettes, helpers)
- `UI/GBHUD.cpp` (frame, hands, crosshair, prompt, toasts, QTE, flash, fade)
- `UI/GBHUD_Paint.cpp` (primitives, pixel font drawing, Slate-font text on the canvas, text roles)
- `UI/GBHUD_Rpg.cpp`, `UI/GBHUD_Cyber.cpp` (the two status HUDs)
- `UI/GBHUD_Menus.cpp` (menu input/state code at the top is older; the drawing part from "drawing: shared widgets" on is new)
- `UI/GBHUD_DamageNumbers.cpp` (only `DrawDamageNumbers` changed)
- `UI/GBPixelFont.h/.cpp` (new: the game's own 5x7 pixel font, baked into atlases)
- `UI/GBPixelSpriteLibrary.h/.cpp` (small change: public `CreatePixelTexture`, `HasSprite`)
- `Core/GBDevCommands.cpp`: only the block that starts at `// ---- UI block (gb.Smoke ui)` (smoke steps that take screenshots)

Use `git diff HEAD -- <file>` for changed files; `GBHUD_Paint.cpp`, `GBHUD_Rpg.cpp`, `GBHUD_Cyber.cpp`,
`GBPixelFont.*` are new (untracked): read them whole. Ignore other modified files in the tree (another person's
work: GBProjectile, GBPixelVfx, Spells.json, GDD.md).

## What to look for (ranked)
1. Crashes or undefined behaviour: null derefs (Canvas, PixelFont, RuntimeFont, Player components), out-of-range
   indexing, division by zero, dangling pointers (e.g. a pointer into a TArray kept across a modification:
   `const FGBActiveStatusEffect* Enchant` in `DrawCyberWeapon`), lambdas capturing by reference past their scope.
2. Behaviour regressions against the old HUD: every clickable must still be registered with `AddMenuHit` with the
   same kinds/indices as before (mouse support), menu keyboard navigation and confirm unchanged, the death screen
   still respawns, toasts still show.
3. Garbage collection: UObjects (textures, the pixel font, the runtime UFont) must be held by UPROPERTY members.
   `UGBPixelFont` keeps textures in a UPROPERTY TMap. Static caches of `FSlateFontInfo` in `GBHUD_Paint.cpp`.
4. Packaged build risks: the cyberpunk text loads TTF files from `Engine/Content/Slate/Fonts` by path and needs
   `/Engine/EngineFonts/Roboto` as a runtime UFont (a ConstructorHelpers reference in the HUD constructor). Is that
   enough for a packaged Win64 build? Anything that only works in the editor?
5. Per-frame cost: anything that scales badly (allocations per glyph, FText::FromString per draw, measuring text
   many times, scanline loops) — only flag what would matter at 60 fps on a 1440p screen.
6. Layout logic errors that a person would see: text that can overflow its box at 1280x720 or 2560x1440, values
   computed with the wrong scale (`UIScale()` for the cyberpunk layout in 1080p units, `ArtPixel()` whole art pixels
   for the RPG layout).

Do not suggest style rewrites, renames or new features. For each finding give: file:line, what goes wrong, a
concrete scenario, and the smallest fix. Say "no findings" for a category if there are none. Be concise.
