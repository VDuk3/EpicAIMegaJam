# Task: ground spells draw their pixel-art figure over the whole area

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor yourself (the material script below runs it headless; that is allowed). Do not commit.

The owner played and said (translated): "Lightning Rune and Ice Floor: the figure in the center must cover the whole area, not just that small part." GDD v0.17: changelog, section 3 "Ground spells are drawn on the ground", section 18 "Ground spell figures". The architect decided the design below; implement exactly this.

Today `Spells/GBSpell_Trap.cpp` (lightning rune) and `Spells/GBSpell_Zone.cpp` (ice floor) draw a translucent tinted cylinder (`Disc`) the size of the area, plus a small camera-facing billboard (`Sprite`) in the middle. After this task each one draws ONE flat quad lying on the ground, textured with the spell's sprite, as wide as the area (diameter = 2 x radius). No disc, no billboard.

## 1. A second generated material: `M_PixelDecal`
`Tools/Jam/make_materials.py` builds `M_PixelSurface` (opaque, world-space tiles). Add a function `build_pixel_decal()` called from `main()` that builds `/Game/Glyphbound/Materials/M_PixelDecal`:
- Blend mode **Masked**, opacity mask = texture alpha (clip value 0.5 is fine), default lit like `M_PixelSurface`, **two-sided**, no shadow needed.
- Same parameter names as `M_PixelSurface` where they mean the same: `Texture` (texture object parameter, sampled with the **texture's own sampler**, never a shared or world-group sampler: the reason is in the comment at the top of the script; pixels must stay sharp), `Tint` (vector, multiplies base color), `Glow` (scalar, emissive = color x Glow, like the surface material).
- **UVs from the mesh** (TexCoord 0), not world position: the whole texture is stretched once over the quad.
- Reuse the script's helpers (`Graph`, `get_or_create_material`, the checks at the end of `build_pixel_surface` for unconnected / bad nodes) instead of duplicating them.
- `Tools/Jam/Make-Materials.ps1` checks that `M_PixelSurface.uasset` exists afterwards; make it check `M_PixelDecal.uasset` too.
- Run it: `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Make-Materials.ps1` (needs the Editor target built and the Editor closed). Both `.uasset` files must exist afterwards. `Content/` is git-ignored except `Content/Data`: that is expected, the material is generated, never committed.

## 2. `Core/GBPlaceholder.h/.cpp`: one helper for flat sprites
`GLYPHBOUND_API UMaterialInstanceDynamic* SetupFlatSprite(UStaticMeshComponent* Component, FName SpriteId, float SizeCm, const FLinearColor& Tint = FLinearColor::White, float Glow = 0.f);`
- Mesh = the engine plane (`EGBShape::Plane`, 100 cm, lies in XY facing +Z), scaled to `SizeCm` x `SizeCm`, no collision, no shadow.
- Texture = `UGBPixelSpriteLibrary::GetSprite(SpriteId)` (the clamped one, not the tile texture).
- Material = `M_PixelDecal` loaded the same way as `M_PixelSurface` (cached, quiet load). If it is missing: log ONE warning saying to run `Tools/Jam/Make-Materials.ps1`, and fall back to `SetupShape(Plane, Tint, glow on)` so the game still shows the area.
- Second helper to change it later: `GLYPHBOUND_API void SetFlatSpriteLook(UStaticMeshComponent* Component, const FLinearColor& Tint, float Glow);` (sets the parameters on the dynamic material; on the fallback it sets the shape color).
- Header comment: this is how ground spells are drawn (GDD 3); the sprite is stretched once over the quad, not tiled.
- If the sprite's texture is not square, stretch it anyway (the sprites for ground spells are drawn square, see item 4).

## 3. The two spells
In both `Spells/GBSpell_Trap.*` and `Spells/GBSpell_Zone.*`:
- Replace `Disc` + `Sprite` with one `UStaticMeshComponent* Figure` (no collision, no overlap events, no shadow), set up in `StartSpell` with `SetupFlatSprite(Figure, GetSpriteId(), 2 * radius, ...)`, placed a few cm above the actor's ground point (keep the existing lift constant idea: about 3 cm, so it does not fight with the floor). The actor already takes the caster's yaw; the figure keeps it (it never turns to the camera).
- Tint: the spell's `Color` string if set, otherwise white (the sprite's own colors are the look). Keep `GetSpellColor()` available for anything else that uses it; do not change its meaning.
- Glow (new optional numeric param `FigureGlow`, default 1.0; document it in each header's param list). Trap only: while not armed the figure shows at `FigureGlow * TrapUnarmedBrightness` with a dimmer tint (`Tint * TrapUnarmedBrightness`, alpha 1); `Arm()` switches to full tint and glow via `SetFlatSpriteLook`.
- `SpriteSize` param: no longer used by these two spells (the size is the area). Remove it from their header param lists and from their entries in `Glyphbound/Content/Data/Spells.json` if present there. Do not touch other spells.
- If a spell has no sprite id, fall back to the old look: a flat tinted disc (`SetupShape` cylinder as today) so the area is still visible. Keep that small.
- Damage, radius, arming, durations: unchanged. This task changes only the look.

## 4. Sprites (`Glyphbound/Content/Data/Sprites.json`)
- `ice_floor` today is 32x12, drawn as a disc seen from the side. Laid flat it would be squashed. Redraw it **top-down, 32x32**: a round sheet of ice that fills the square (circle touching the edges), same palette keys as today (`o` outline, `c` ice, `w` highlight, `0` cracks; check the palette of the entry), with a few cracks and highlights spread over the whole circle, transparent (`.`) outside the circle. Keep rows the same length.
- `rune_lightning` (16x16, ring + bolt) is already round: redraw it at **32x32** so it reads well at 3 m: the yellow ring at the edge, a second thinner inner ring, and a bigger lightning bolt in the center, same palette keys as today. Keep rows the same length.
- Unreal map keys ignore case: no two palette keys may differ only by case (that is why palettes use digits).

## 5. Smoke script (`Core/GBDevCommands.cpp`)
Add steps (after the existing ones, same spacing style, at least one second after each screenshot request) that put the player in the past with the staff, looking down at the floor about 4-5 m ahead, cast the lightning rune and the ice floor next to each other without the QTE (use the same way the script already casts with the QTE off, or spawn through the spellbook; teach the spellbook the two spells if the player does not know them yet, like the script already does for others if it does), then take a screenshot `10_ground_spells`. If either spell actor did not spawn, log an `UE_LOG(LogGB, Error, ...)`. Restore whatever state later steps need (or put these steps at the very end).

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Make-Materials.ps1` succeeds, and both `Glyphbound/Content/Glyphbound/Materials/M_PixelSurface.uasset` and `M_PixelDecal.uasset` exist.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 26` passes (the sprite parsing test reads `Sprites.json`: your new rows must be valid).
5. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, no `LogGB` warning (in particular not the "material is missing" one), and the screenshot `Glyphbound/Saved/Screenshots/Smoke/10_ground_spells.png`. **Open that screenshot and describe it** in the report: both figures must lie flat on the floor, fill their circles, be sharp pixel art, and show the floor through their transparent pixels.
Never make an error disappear by deleting behavior. All JSON files stay valid. No gameplay numbers in code (visual-only constants like the lift are fine, named, in an anonymous namespace).

## Final report (plain text)
1. The result lines of the five checks, copied verbatim.
2. What you changed, with file names.
3. Your description of `10_ground_spells.png`.
4. Anything you could not do as described, and why.
5. What a person should check by playing.
