# 2026-11-15 - Elemental Sword: animated sprite per element

Owner's request: "a sprite for each element, a base of a related color and a good sprite animation for the blade's
gradient, as if the edge were ethereal; solid hilt and blade base, the rest a pixel-art gradient." Left to the
architect's taste; everything else fixed by the architect.

- **Art, by script.** `Tools/Jam/gen_sword_sprites.js` writes 24 entries to `Sprites.json`:
  `sword_blade_<fire|lightning|ice>_<0..7>`, 16 x 88 px. Solid hilt (wrapped grip, guard with the element's gem,
  pommel) and a solid blade base with ridge lines; above it the blade is made of four color bands at four opacity
  levels that dissolve toward the tip, with a bright core line and wisps that scroll up the blade (a loop of 8 frames).
  Fire: wider licking flames and embers. Lightning: the core zigzags every frame. Ice: narrower, cleaner, with twinkles.
  Reviewed from a 4x contact sheet of all 24 frames before use.
- **HUD** (`GBHUD.cpp`, sword branch of DrawHands): picks `<Sprite>_<element>_<frame>` (10 fps) by the weapon's
  element, which the weapon enchant overrides while it lasts; falls back to the old tinted blade if the frames are
  missing. The sword now rises from the bottom of the view and leans a little left; the swing sweeps it across,
  pivoting on the pommel (before: it floated where the hand used to be and only slid sideways).
- GDD v0.73, and the stale lines saying the sword was "the only melee weapon" and "not enchantable" are fixed.
- Verification: Live Coding compiled; not seen in PIE (a human has to).
- Implementer: the architect itself (small, art-by-script change; reason noted per the rules).
