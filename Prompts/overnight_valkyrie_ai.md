# Overnight block D: the Valkyrie's fight, near and far, with a spell (owner's point 1, AI part)

Read first: `Prompts/implementer_rules.md` (hard rules apply except the build rule: see "Build"), `Design/ARCHITECTURE.md`,
`Design/GDD.md` §10 ("The Valkyrie" bullet: sealed in her tube until the glass breaks, then she wakes and fights) and §11.

## The owner's words (Spanish, translated)
"The cyberpunk Valkyrie: AI that fights both up close and at range, with at least one spell. Animation or VFX
placeholders are allowed if they work." (Her wings and her sword's grip are being fixed by the architect: the wings
now ride her chest bone rigidly; the sword is on `hand_r` at the `WeaponOffset` of her data.)

## What exists
- `Valkyrie_Future` in `Content/Data/Enemies.json`: `ActorClass GBSkeletalEnemy`, ranged (bursts of light bolts from
  her sword: hitscan with `Fx.Tracer`), `MeleeRange` / `MeleeDamageMult` used by `AGBSkeletalEnemy::TickMeleeSwing`,
  the dormant/sealed logic and the wake leap in the base classes (`AI/GBEnemyCharacter.*`, `AI/GBSkeletalEnemy.*`).
- Her clips in `/Game/Glyphbound/Characters/Valkyrie/Anims/`: `A_Valkyrie_Idle`, `_Unarmed_Walk_Fwd/Bwd/Left/Right`,
  `_Unarmed_Jog_Fwd`, `_Attack_01/02/03`, `_ChargedAttack`, `_HitReact_Front_Lgt_01`, deaths, and new tonight (from the
  Combat Magic Animations pack): `_CastSpell` (4.8 s), `_LevitatingMagicStrike` (2.37 s), `_Levitating` (5.6 s),
  `_GroundManaShot` (5.2 s), `_MagicStrike` (1.6 s), `_MagicStrike2` (1.7 s), `_Ascension` (3.1 s), `_SpellAttack` (2.37 s).
  Clips are played through `UGBEnemyAnimInstance` (`AI/GBEnemyAnimInstance.*`): named slots from `Anim.<Name>` strings.
  You may add a generic "play this named one-shot clip as a full-body action" if it does not exist.
- Effects available: `/Game/SciFi_Impacts/FX/*` (energy impacts, tracers), `/Game/GunFX`, `/Game/MuzzleFlash3D`, and
  the baked pixel clips of `Plugins/GBPixelVfx` (pixel art, for magic; the art rule allows pixel magic in the future).

## What to build
A new class **`AGBValkyrieEnemy`** (`AI/GBValkyrieEnemy.h/.cpp`) derived from `AGBSkeletalEnemy`, used by
`Valkyrie_Future` (and later by the castle's pixel Valkyrie `Valkyrie_Past`, which another agent gives a sprite look
inside `AGBSkeletalEnemy`: so talk to the look only through what `AGBSkeletalEnemy` offers, never to the mesh directly
where a sprite-look enemy would break). A boss fight with readable phases, all numbers in data:
1. **Close (melee):** when the player is within `MeleeRange`, a sword combo of two or three swings (Attack_01/02/03),
   each with a short windup the player can read, damage on a cone in front at the swing's hit time, and a step forward
   with each swing. After a combo she backs off or dashes away (a short wing-assisted hop backwards).
2. **Far (ranged):** her bursts of light bolts (what exists), from her sword, keeping her preferred band, strafing.
3. **Dash in:** at middle range, sometimes a fast dash (wings) to close in and open with a swing.
4. **Spell, at least one (a boss needs one that reads):** "Judgement": she rises a little off the floor
   (`Levitating`/`CastSpell`/`Ascension`), marks 3 to 5 circles on the floor around and at the player (a glowing ring
   that grows over 1.2 s: telegraph), then a pillar of light strikes each (damage in the circle, a flash, a point
   light, camera-visible VFX). Second spell if time allows: a fan of 5 to 7 light lances that fly straight (projectiles,
   dodgeable) from `MagicStrike`. Cooldowns and weights in data.
5. **Phase 2 under 50% health:** shorter cooldowns, Judgement with more circles, combos of three.
- **Her hand on the sword:** the clips are unarmed, the fingers open around the grip. Close the right hand's fingers
  around it procedurally (a fixed curl on the finger bones of `hand_r` in the anim instance, data `GripCurlDeg`), only
  for enemies that hold a weapon in that hand and have the data key.
- Every timing, damage, radius, cooldown and weight in `Enemies.json` `Valkyrie_Future` Stats (named keys), defaults
  in code. Hits go through `UGBCombatLibrary::DealDamage`.
- Smoke: add a step to the `Enemies3D` block in `Core/GBDevCommands.cpp` (or a small block `gb.Smoke Valkyrie` if
  cleaner) that wakes her in the arena, puts the player near (god mode), checks she swings and hits, then far, checks
  she fires, and forces a Judgement cast and checks its circles strike. Log lines with counts.

## Files you own
New: `AI/GBValkyrieEnemy.*`. Shared, **Edit tool only (never rewrite a whole file)**, minimal hunks, because another
agent edits the same files right now: `AI/GBSkeletalEnemy.h` (you may add `virtual` to an existing method or a
protected accessor; nothing else), `AI/GBEnemyAnimInstance.*` (the finger curl and a named one-shot clip, if missing),
`Core/GBDevCommands.cpp` (your step only), `Content/Data/Enemies.json` (`Valkyrie_Future` only).

## Build
You may build: take the machine lock (`Glyphbound/Saved/machine.lock`: one line, who, what, UTC time; if another
agent holds it and it is younger than 45 min, wait and do other work; delete your line when done; an open
`UnrealEditor.exe` also blocks the build: never close it yourself, report and wait). Iterate cheaply (owner's rule):
Editor target only (`powershell -File Tools/Jam/Build-Jam.ps1`), your smoke step with `Smoke-Jam.ps1 -Only <Step> -Offscreen`.
No full smoke, no game build, no package, no commit. Look at the screenshots yourself.

## Final report
As in `implementer_rules.md`, plus the fight's loop in five lines, the data keys you added with their values, the
smoke output lines, and what needs a human to judge by playing.
