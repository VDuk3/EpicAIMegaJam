# Glyphbound: code architecture

> Written by the architect (Claude Code) on 2026-11-12. Source of truth for how the C++ is organized.
> Design lives in `Design/GDD.md`; this file says how the code implements it. Keep it short and current.

## Principles
1. **Data, not code.** Every gameplay number lives in `Glyphbound/Content/Data/*.json` and is read through `UGBDataSubsystem`. Code never hardcodes damage, costs, speeds, durations, sizes or timings. The JSON ships as loose files, so numbers can be tuned in a packaged build with a text editor (`gb.ReloadData` reloads while playing).
2. **One stat system.** A number is a *named stat*: base value from data, then modifiers. `UGBStatsComponent` evaluates `(Base + adds) * multipliers` (or an override). Skill-tree nodes, armor, status effects, air passives and the weapon enchant are all just groups of modifiers with a source name. Nothing else changes numbers.
3. **One damage path.** Everything that hurts goes through `UGBCombatLibrary::DealDamage(Target, FGBDamageInfo)`. Shields, resistances, teams, on-hit effects and kill credit hook in there once.
4. **Spells are data + a behavior.** A spell is a `FGBSpellDefinition` (params with `Base/Min/Max/ScaledBy`) plus the class of an `AGBSpellActor` behavior. The QTE result (power, reach) scales the params tagged for it. A new spell that fits an existing behavior is a JSON entry, no code.
5. **Shared pieces.** One projectile (`AGBProjectile`) for staff bolts, spell projectiles, enemy shots and grenades. One status-effect component for slows, burns, potions and buffs. One interact interface for portals, chests, pickups and the seal.
6. **Text in git.** No binary assets are required to run: input actions, the test arena, placeholder shapes and placeholder pixel sprites are all built in C++ from text. `Content/` is ignored by git except `Content/Data/`.
7. **No GAS.** Unreal's Gameplay Ability System was considered and rejected for the jam: it needs a lot of boilerplate and asset setup, and the game's needs (costs, cooldowns, timed effects, stat upgrades) are covered by principles 2-4 in a few hundred lines we fully control.
8. **PIE is human-only.** Agents build, run the pure-logic tests and package; a person plays.

## Module layout (`Glyphbound/Source/Glyphbound`, one runtime module, prefix `GB`)
Includes are relative to the module root: `#include "Stats/GBStatsComponent.h"`.

| Folder | What | Key types |
|---|---|---|
| `Core/` | Shared enums, stat names, placeholders (plain shapes and pixel-textured structures), game mode, dev commands | `GBTypes.h`, `GBStatNames.h`, `GBPlaceholder.h` (`SetupTexturedShape`), `AGBGameMode`, `UGBGameInstance`, `GBDevCommands.cpp` (`gb.Smoke`) |
| `Data/` | Definitions and the JSON loader | `GBDataTypes.h`, `UGBDataSubsystem` |
| `Stats/` | Stats + modifiers, health/mana | `UGBStatsComponent`, `UGBAttributeComponent` |
| `Combat/` | Damage (one path, one global `OnDamageDealt` event), status effects, elemental build-up, projectile, explosion shockwave | `UGBCombatLibrary`, `UGBStatusEffectComponent`, `UGBElementalStatusComponent`, `FGBBuildUpMeter` (pure, tested), `AGBProjectile`, `AGBExplosionEffect` |
| `Player/` | Character, controller, input | `AGBPlayerCharacter`, `AGBPlayerController`, `UGBInputSet` |
| `Weapons/` | Held items and slots | `UGBEquipmentComponent`, `AGBHeldItem`, `AGBStaff`, `AGBGun`, `AGBSword` |
| `Spells/` | Spellbook, cast resolution, behaviors | `UGBSpellbookComponent`, `FGBSpellCast`, `AGBSpellActor`, `AGBSpell_*` |
| `QTE/` | Quick-time event | `FGBQTESession`, `FGBGestureRecognizer`, `UGBQTEComponent` |
| `World/` | Eras, progress, interactables, test arena | `UGBEraSubsystem`, `UGBEraComponent`, `UGBProgressSubsystem`, `IGBInteractable`, `AGBPortal`, `AGBChest`, `AGBPickup`, `AGBCheckpointVolume`, `AGBDoor` (a leaf that slides open while the player is near, `AGBBreakable` (breaks after N hits, cracks, progress flag), `AGBEnergyGate` (blocks until its flags exist, then fades) and `AGBKeySocket` (takes an item from the player, sets a flag), optional progress flag), `AGBTestArena` |
| `AI/` | Enemies (navmesh path, local steering fallback, stuck check) and encounters | `AGBEnemyCharacter`, `GBSteering` (pure decision, tested), `AGBEncounter` |
| `Items/` | Inventory, consumables, armor, recipes | `UGBInventoryComponent` |
| `Progression/` | Skill trees | `UGBSkillTreeComponent` |
| `UI/` | Canvas HUD (two skins), menus, pixel sprites | `AGBHUD`, `UGBPixelSpriteLibrary` |
| `Tests/` | Pure-logic automation tests | `Glyphbound.<Area>.<Name>` |

## Data files (`Glyphbound/Content/Data`)
`Player.json`, `QTE.json`, `Game.json`, `Spells.json`, `Weapons.json`, `StatusEffects.json`, `Items.json` (items + recipes), `Enemies.json`, `Encounters.json`, `Skills.json`, `Sprites.json`. Schemas are the structs in `Data/GBDataTypes.h` (field name = JSON key). Stat and parameter names are the constants in `Core/GBStatNames.h`.

Modifier keys: an actor stat (`MoveSpeed`), `Weapon.<Id>.<Stat>`, `Weapon.<Slot>.<Stat>` (`Weapon.Staff`, `Weapon.Gun`, `Weapon.Sword`), `Weapon.All.<Stat>`, `Spell.<Id>.<Param>`, `Spell.All.<Param>`, `QTE.TimeDilation`...

Standard status effect ids (defined in `StatusEffects.json`, referenced from other files): `Slow`, `Ignite`, `HealthPotion`, `ManaPotion`, `HealthInjection`, `Enchant_Fire`, `Enchant_Lightning`, `Enchant_Ice`. (`Burn`, `Shock`, `Chill` were removed in GDD v0.22.)

Modifier ops: `Add`, `Multiply`, `Override`, and `MultiplyMin` (only the lowest one on a stat counts: slows never stack, the strongest wins).

Elemental keys (GDD 6 "Elemental build-up"): per enemy in `Enemies.json` `Stats` (`<E>DamageTaken`, `<E>BuildUpTaken`, `<E>BuildUpThreshold`, `<E>BuildUpDecayDelay`, `<E>BuildUpDecayRate`, `<E>StatusDuration`, `<E>StatusImmunity`, `IgniteDamageMult`, `ShockDischargeDamage`, `FrostSlow`; E = Fire, Lightning, Ice) and `Strings` (`<E>StatusReaction`); missing keys fall back to `Game.json` `ElementalDefault.<Key>`. Sources: spell param `BuildUp`, weapon stats `FireBuildUp` / `LightningBuildUp` / `IceBuildUp` and `WeakPointBuildUpMult`, item stat `BuildUp`.

## How the main flows run
- **Cast:** input -> `UGBSpellbookComponent::TryCastSlot` (checks staff in hand, known, cooldown, mana; spends mana) -> `UGBQTEComponent::StartQTE` (slow time, sequence = power, gesture = reach; or instant when off) -> `GBSpell::ResolveCast` (data -> QTE scaling -> modifiers) -> spawn the behavior actor -> it deals damage through `UGBCombatLibrary`.
- **Shoot:** input -> `AGBGun` (fire interval, magazine, spread, recoil, pellets; every number via `GetStat`) -> hitscan -> headshot and distance falloff math in `UGBCombatLibrary` -> `DealDamage`.
- **Era travel:** `AGBPortal::Interact` -> `UGBEraSubsystem::RequestTravel` (fade out, swap era for `UGBEraComponent` actors and `_Past` / `_Future` sub-levels, teleport, restore health/mana/ammo, save checkpoint, fade in).
- **Progress:** encounters set flags in `UGBProgressSubsystem` when cleared; `UGBEraComponent` reads flags, which is how era-gated zones and past-to-future unlocks are built. Death restores the last checkpoint snapshot; flags are never rolled back (enemies do not respawn).
- **Elemental build-up:** a hit carries `FGBDamageInfo::BuildUp` (spell param, weapon stat by element, item stat) and a `BuildUpEventId` (one per shot, cast, blast or tick). `DealDamage` passes it to the target's `UGBElementalStatusComponent` (`ApplyBuildUp`; the ice floor, which deals no damage, calls it directly), which dedupes by event id, applies the enemy's resistance and fills one of three `FGBBuildUpMeter`s. A full bar starts the status: Ignite is the `Ignite` status effect (DOT through `DealDamage`); Shock deals its discharge; Frost slow is a `MultiplyMin` modifier; stun, interrupt and freeze are done by `AGBEnemyCharacter` from the component's `OnStatusStarted` event. The component only ticks (every `ElementalTickInterval`) while a bar is not empty. Looks bind to its events; `gb.ShowElemental 1` is a debug view and `gb.BuildUp` a test command.
- **Upgrades:** `UGBSkillTreeComponent::Unlock` adds the node's modifiers to the player's stats; weapons and spells pick them up on their next read.

## Conventions
- Game text in English. Code comments in English. ASCII only in source files.
- Native multicast delegates (C++ project; no Blueprint graphs needed).
- No custom collision channels: projectiles and queries use the default channels and filter by team in code.
- Pointers to definitions are not kept across frames (data can be reloaded): keep the id, look it up.
- Tests: only pure logic that is easy to break and hard to see by playing; each one must be able to fail; names `Glyphbound.<Area>.<Name>` where a name is never also a prefix of another.
- Build: `powershell -File Tools/Jam/Build-Jam.ps1` (Editor closed). One build at a time per machine.
- Art rule in code: characters, items and effects are camera-facing pixel sprites; **structures are 3D shapes with pixel tiles** (`GBPlaceholder::SetupTexturedShape`, tiles are sprites of `Sprites.json` named `tile_*`). The material behind it (`M_PixelSurface`) is the only binary asset and is generated by `Tools/Jam/Make-Materials.ps1`, never edited by hand.
- A structure that looks different in each era has ONE hidden collision body present in both eras and two non-colliding looks (PastOnly / FutureOnly), so footing and navigation never change on era travel.
- UI that reacts to combat listens to `UGBCombatLibrary::OnDamageDealt()`; nothing in combat code knows about the HUD.
