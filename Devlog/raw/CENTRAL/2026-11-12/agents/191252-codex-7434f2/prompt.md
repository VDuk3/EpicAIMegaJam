# Code review: Glyphbound, elemental build-up (read-only)

You are the second-opinion reviewer for **Glyphbound**, a first-person RPG shooter in Unreal Engine 5.9.0 (C++), made for a 4-day game jam. Do not modify any file. Do not build or run anything.

## Context
- Read `Design/ARCHITECTURE.md` first (short). The design is `Design/GDD.md` section 6 "Elemental build-up" (v0.22) and the v0.22 lines of section 18 "Numbers". The owner's original spec (Spanish) is `Prompts/feedback8_elemental_spec_owner.md`; the architect's design brief given to the implementer is `Prompts/feedback8_elemental_buildup.md`.
- The change under review is commit `99ee8f8` (`git show 99ee8f8 --stat`, then read the files). One AI implementer wrote it; the architect added one fix in `Combat/GBProjectile.cpp`. It builds, 34 tests pass, and a scripted smoke run passes in the Editor and in the packaged exe, so a missing include is NOT what we want from you.
- Code: `Glyphbound/Source/Glyphbound/`. Data: `Glyphbound/Content/Data/*.json`.
- Engine source for checking APIs: `C:\Program Files\Epic Games\UE_5.9\Engine\Source`.

## What changed (review these areas)
1. **Meters** (`Combat/GBBuildUpMeter.*`): three phases (building, active, immune), decay after a delay, frame-rate independence, the event log that makes one event count once (biggest amount wins).
2. **Component** (`Combat/GBElementalStatusComponent.*`): config read from enemy stats with `Game.json` `ElementalDefault.*` fallbacks, reactions from enemy strings, tick only while busy (`SetComponentTickEnabled`, `TickInterval`), Ignite through `UGBStatusEffectComponent`, Shock discharge through `DealDamage` (re-entrancy: it damages its own owner from inside a damage call chain), Frost slow as a `MultiplyMin` modifier, cleanup on death / EndPlay / data reload.
3. **Damage path** (`Combat/GBCombatLibrary.*`, `Stats/GBAttributeComponent.cpp`, `Core/GBTypes.h`, `Core/GBStatNames.h`): `ApplyBuildUp` after a damaging hit, `IsWeakPointHit` split from `ApplyWeakPointBonus`, per-element damage taken, event ids.
4. **Sources** (`Spells/GBSpellTypes.cpp`, `Spells/GBSpell_Zone.cpp`, `Weapons/GBHeldItem.cpp`, `Items/GBInventoryComponent.cpp`, `Combat/GBProjectile.cpp`): one event id per shot / cast / blast / tick; shotgun pellets, piercing bolts, the fireball explosion, the rune blast (`Spells/GBSpell_Trap.cpp`), the flamethrower ticks (`Spells/GBSpell_Beam.cpp`), the ice floor (no damage, so it sends build-up directly).
5. **Enemy reactions** (`AI/GBEnemyCharacter.*`): stun / freeze lock that pauses the state machine, interrupt that cancels a windup. Look for: an enemy stuck forever, an attack landing after it was cancelled (a melee hit or a ranged projectile scheduled by a timer), movement continuing while locked, locks surviving death or era travel.
6. **Stats** (`Stats/GBStatsComponent.*`): the new `MultiplyMin` op; the ice floor's `Slow` effect now uses it; Deep Freeze and the 80% cap must be unchanged.
7. **Removed per-hit effects** (`Burn`, the old `Shock` slow, `Chill`): anything in code or data that still refers to them, or code that silently depended on them.
8. **Debug view and commands** (`UI/GBHUD.cpp` `gb.ShowElemental`, `Core/GBDevCommands.cpp` `gb.BuildUp` and the smoke step).

## What to report (real defects only, most severe first)
- Runtime bugs a player would hit: wrong build-up amounts (double counting, never counting), statuses that never end or never start, immunity not respected, slows stacking, Ignite damage that differs from the data, crashes on death / destroy / data reload.
- Contract breaks with existing systems: the flamethrower (mana, hold, interrupt), the ice floor (slow, Deep Freeze, QTE), weapon damage and head bonus, encounter progress on death.
- Data and code mismatches: a key in a JSON file that the code spells differently or never reads, and the reverse.
- Things that work in the Editor but break in a packaged build.
- Per-frame costs that grow with the number of enemies (the game will have hordes).
- Gameplay numbers hardcoded where data should decide.

Do not report: style, naming, formatting, missing comments, micro-optimizations, multiplayer issues (single player), or the number of tests.

## Output format
A numbered list, most severe first, at most 15 items. For each:
- `file:line` (or lines),
- what is wrong, in one or two sentences,
- why it matters when playing (what the player would see),
- the smallest fix you would make.
Finish with one short paragraph: the three things you would fix first, and anything you checked and found solid.
