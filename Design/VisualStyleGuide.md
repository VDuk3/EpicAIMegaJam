# Pixel-art visual style guide

> **Source.** The 9 sprites this guide is built from (`valki`, `axe`, `spada`, `sscudo`, `sscudo adnto`, `baston1`, `baston1.2`, `baston2`, `baston3`) were drawn **by hand in Pixelorama on the LAPTOP on 2026-11-12**, during the jam (file times 12:45 to 21:32 local, UTC-4). The guide was drafted the same evening by Cursor on the LAPTOP, which read the `.pxo` files, measured them (canvas, colors, alpha) and wrote it in Spanish at the artist's request. This is the English translation, made by the architect on CENTRAL; the Spanish original and the `.pxo` sources are in `Glyphbound/SourceArt/PixelArt/` (see its README). Log: `Devlog/raw/LAPTOP/2026-11-13/transcripts/cursor-2e46aba0-*.jsonl`.
>
> It complements GDD §3 "Art rule": that section says **what** is pixel art; this guide says **how** it is drawn. Where they disagree, the GDD wins.

---

## 1. The style in one line

> **Dark fantasy hi-bit pixel art, 16-bit inspired.**

- **Hi-bit:** modern pixel art that keeps the retro language (hard pixels, outline, a small palette per sprite) without a console's limits: large sprites, more colors, fine detail.
- **16-bit inspired:** the SNES / Mega Drive tradition: ramp shading, a dark outline, pinpoint highlights.
- **Dark fantasy:** dark base materials (iron, obsidian) with saturated accents (blood, fire, arcane, ice).

Tone references (reference only, nothing copied): *Castlevania: Symphony of the Night*, *Blasphemous*, *Eitr*, *Children of Morta*.

---

## 2. Visual pillars

1. **Silhouette first.** Every object reads from its black shape alone: crescent axe, straight sword, round shield, staff with an ornate head.
2. **Dark material + bright accent.** Most of a sprite is muted. Saturated color is kept for what is magical or important: gems, orbs, blades, hair.
3. **Clean pixels.** No semi-transparency and no automatic anti-aliasing. Every current sprite already follows this (alpha is 0 or 255).
4. **Few ramps, well chosen.** Each material has a fixed ramp; no new tones invented sprite by sprite.

---

## 3. Format and scale

| Rule | Value |
|---|---|
| Asset canvas (weapons, shields, characters) | 100×100 px |
| Longest side of the sprite | 88–96 px |
| Minimum margin to the canvas edge | 2 px |
| Resolution | 1:1, no scaling inside the canvas |
| Transparency | 0% or 100% only |
| Animation frame rate | 6 fps as a base (what the files already use) |

**To decide:** the character (`valki`, 93 px) and the weapons (89–99 px) are almost the same height today. That works for inventory icons or equipment cards, not for weapons held in the game. Define two scales:

- **Icon scale:** the current one, the object filling the canvas.
- **World scale:** the weapon in proportion to the character's hand. For example, a sword of about 40–50 px for a 93 px character.

`axe` is 99 px tall and touches the canvas edge; shrink or re-center it to keep the margin.

---

## 4. Outline

- **Outer outline:** `#000000`, 1 px, all around. It is already the outline color of `valki`, `axe`, `spada` and the shields.
- **Inner outline** (between parts, such as blade and guard or strap and metal): the darkest tone of the material's ramp, not black.
- **Staffs:** today their darkest tone is `#1D1D25` and they have no black outline. Pick one option and apply it to all four:
  - Add a `#000000` outline outside, to match the rest (recommended).
  - Keep them without black and record it as the exception of the "obsidian" family.
- **Thickness:** always 1 px. Avoid doubled corners (a 2 px outline on diagonals) unless weight is wanted on purpose.

---

## 5. Light and shading

- **Light source:** top left. The highlights on the shield and the sword blade already follow it; apply it to everything.
- **Ramp per material:** 3 to 5 tones (deep shadow, shadow, base, light, specular).
- **Specular highlight:** 1 to 3 near-white pixels, only on polished metal, gems and orbs. Never on wood, cloth or stone.
- **Hue-shifted ramps:** shadows lean cold (blue or violet), lights lean warm (yellow). It brings life to dark fantasy and avoids "dirty" greys.
- **Texture:** the loose-pixel noise on the shield works as wear. Use it only on rough materials (old iron, stone, wood), never on polished metal or skin.
- **Dithering:** optional, and only on large flat surfaces. Avoid it on small or animated sprites.

---

## 6. Master palette

Today the 9 sprites use **81 unique colors**, and the loaded palette (*DawnBringer 32 copy*) is barely used. There are also duplicates the eye cannot tell apart (`#000000`/`#010101`, `#434848`/`#434846`, and six different whites). The proposal consolidates them into fixed ramps, starting from the colors already in use.

### Neutrals and outline
| Use | Colors |
|---|---|
| Outline | `#000000` |
| Specular white (the only one) | `#F0F0F0` |

### Iron / steel (shield, axe, sword)
`#262827` → `#373837` → `#434848` → `#555957` → `#758685` → `#9DAAA9` → `#B8C3BD` → `#E5DCDC`

- Old dark iron (shield, axe): the first 5 tones, with `#B8C3BD` as the highlight.
- Polished steel (sword): from `#758685` to `#E5DCDC`.
- The axe blade uses warm greys (`#2F2A2A`, `#544F49`, `#B7B3AF`). Move it to this ramp, or create a "warm iron" ramp and document it.

### Obsidian (shaft of the magic staffs)
`#1D1D25` → `#252734` → `#32343C` → `#655478` (light)

### Wood
`#41290D` → `#57220D` → `#6C4B24` → `#8D2B11` → `#926806`

The axe's wood (neutral brown) and `baston1`'s (reddish brown) differ today. Pick one direction and use it on both.

### Bone / ivory (sword guard, wings)
`#85836A` → `#A19D4F` → `#E2DE9C` → `#E6E4B3` → `#F5F3CF`

### Skin
`#F9C990` → `#FBD6AA` → `#EFD9BF`

A skin shadow tone is missing. Add a warm, slightly pink one, around `#C88A6A`.

### Elemental accents (the only saturated colors)
| Element | Ramp | Where |
|---|---|---|
| Blood / red | `#4B0A0A` → `#810505` → `#B70000` → `#FF0000` | sword fuller, axe gem, Valki's hair |
| Fire | `#4B0F0F` → `#BC2A12` → `#F03416` → `#F3610B` | `baston3` |
| Arcane / void | `#441974` → `#650075` → `#8548C0` → `#B83ECC` → `#C0A2E1` | `baston1.2`, shield gems |
| Ice | `#1C8B9B` → `#14AFB4` → `#00F6FF` | `baston2` |
| Valki's armor | `#00168A` → `#384692` → `#717694` | Valki |

About **50 colors** in total, against 81 today, each with a clear role. Save them as the project's own palette in Pixelorama and stop using DawnBringer 32 as the reference palette, since it is not the one actually in use.

### Creature ramps (2026-11-15, from the castle enemies' pre-rendered sheets)
Added by the sprite baker (`Tools/Blender/sprite_bake.py`) for the castle's 8-direction enemies; everything else on those sheets comes from the ramps above.
- Orc skin green: `#1B2E2A #2B4A30 #3F6B35 #5E8F3E #8DB552`
- Brute, darker orc green: `#16241F #22392A #31502F #476B36 #628A40`
- Leather (orc vest and boots, centaur gear): `#2A1A1C #4A2F22 #6E4A2C #946A3A`
- Shaman robe purple: `#1E1428 #2E1E3C #45305A #5E4677`
- Barghest fur black: `#0E0C12 #1C1720 #2C2430 #40343F #5A4A55`; its ember-brown hide: `#2A1212 #4B1A16 #73291C #9C3F22`
- Horse chestnut (the centaur): `#2A1410 #4A2416 #6E3A1E #94542A #B8743A`
- Skin shadows: `#C88A6A`, and `#8E5B48` (the centaur)
- The Valkyrie's armour gets a deeper shadow `#0A0B3D` before its ramp; her wings use three tones `#A19D4F #E2DE9C #F5F3CF`

### Color budget per sprite
- Weapons and items: up to 16 colors.
- Characters: up to 24 colors.
- At most 1 or 2 accent ramps per sprite.

---

## 7. Design language by family

### Staffs (variant system)
A very good system already exists; this formalizes it:

| Variant | Shaft | Head | Reads as |
|---|---|---|---|
| `baston1` | reddish wood | silver orb | basic / mundane |
| `baston1.2` | obsidian | arcane orb + floating particles | void / shadow |
| `baston2` | obsidian | ice orb | ice |
| `baston3` | obsidian + lava veins | fire orb | fire |

**Rule:** the base silhouette (22×90 px) stays; only the accent ramp changes. The loose particles of `baston1.2` and the veins of `baston3` are a good marker of "magic level". Use them consistently: the basic variant has no effects, the elemental ones have veins or particles.

### Metal weapons
Dark iron, black outline and a single red accent (gem or fuller). The sword and the axe already follow this.

### Shields
A round iron base with textured wear and a raised cross. The ornate version adds arcane gems and a wooden strap. Future variants change the ornaments, not the base.

### Characters
Valki sets the reference: armor in a solid saturated color, hair as the main accent, and secondary elements (wings) in muted ivory so they do not compete with the body.

---

## 8. What we never do

- Colors outside the master palette.
- Automatic anti-aliasing, semi-transparency, blur or smooth gradients.
- Pure white `#FFFFFF` or stray whites. Only `#F0F0F0` exists.
- Saturated accents on large surfaces (an accent is small and pinpoint).
- Black and non-black outlines mixed inside one family.
- Light from different directions on sprites shown together.
- Texture noise on polished metal or skin.

---

## 9. Current inconsistencies and actions

| Problem | Files | Action |
|---|---|---|
| 81 colors, most outside DB32 | all | Remap to the master palette |
| Six near-identical whites | `valki`, `spada`, `baston*` | Unify on `#F0F0F0` |
| `#010101` instead of `#000000` | `axe` | Replace with `#000000` |
| `#434846` duplicating `#434848` | shields | Replace with `#434848` |
| Staffs without a black outline | `baston*` | Decide the rule (section 4) |
| Axe touching the edge (99 px) | `axe` | Shrink or re-center with a 2 px margin |
| Two different wood tones | `axe`, `baston1` | Unify the ramp |
| Warm greys on the axe | `axe` | Move to the iron ramp or document them |
| No saved palette | `baston1.2`, `baston2`, `baston3` | Assign the project palette |

---

## 10. Checklist before a sprite is done

- [ ] 100×100 canvas, longest side 88–96 px, 2 px margin.
- [ ] Silhouette readable in pure black.
- [ ] 1 px outer outline following its family's rule.
- [ ] Light from the top left.
- [ ] Every color is in the master palette.
- [ ] Within the color budget (16 for items, 24 for characters).
- [ ] At most 1 or 2 saturated accents.
- [ ] No semi-transparency.
- [ ] Next to the other sprites of its family, it looks like the same game.
