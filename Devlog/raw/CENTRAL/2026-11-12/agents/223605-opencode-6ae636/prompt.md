# Task: move the last hardcoded gameplay numbers into the data files

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first. In this task you **do not build, do not run tests, do not open the Unreal Editor and do not commit**: the architect reviews your diff, asks a second reviewer, and then builds once. So write code that compiles: check every engine API you are not sure about in `C:\Program Files\Epic Games\UE_5.9\Engine\Source`.

Owner's request (translated): "Try to leave not a single hardcoded value, because we are going to test all these values and they must be easy to change." The testers change numbers in `Glyphbound/Content/Data/*.json` and run `gb.ReloadData` in the console; nothing may need a recompile.

An audit found the literals below. For each one: read it from the data with the helper that file already uses (`UGBDataSubsystem::GetGameValue`, `UGBStatsComponent::GetStat`, the player's `ReadStat`, the QTE component's `ReadStat`, `ReadItemStat`, `FGBSpellCast::Get` / `AGBSpellActor::ReadParam`), keep **today's value as the code default**, and **add the key with today's value to the JSON file**, so the game plays exactly the same after your change. Line numbers are from today's tree and may be off by a few lines.

**Rules for the JSON files:** the architect has just rebalanced them. **Only add new keys. Never change, reorder or reformat an existing value or line.** Edit them as text (keep tabs and the one-line objects as they are). New stat-name constants go next to the similar ones (`Core/GBStatNames.h` or the header that already holds that file's names).

Read the value where it is used (live), not once in a constructor or `BeginPlay`, unless reading it per frame would be wasteful; in that case re-read it on `OnDataReloaded`.

## 1. `Game.json` "Values"
| Key | Today | Where |
|---|---|---|
| `FrostSlowMax` | 0.95 | `Combat/GBElementalStatusComponent.cpp:16` (used near :314) |
| `KnockbackMinLift` | 0.35 | `Combat/GBProjectile.cpp:27` (used near :364) |
| `ProjectileMaxLifetime` | 10 | `Combat/GBProjectile.h:34` `MaxLifetime` default; also the `10` inside `max(10, Range/Speed + 1.0)` in `Spells/GBSpell_Projectile.cpp:10,:40` |
| `SpellProjectileLifetimeMargin` | 1.0 | the `+ 1.0` in that same expression |
| `EnemyMeleeAcceptanceFraction` | 0.5 | `AI/GBEnemyCharacter.cpp:442` (`AttackRange * 0.5f`) |
| `EnemyProjectileRangeMult` | 1.5 | `AI/GBEnemyCharacter.cpp:773` (`AggroRange * 1.5f`) |
| `EnemyAimHeightFraction` | 0.5 | `AI/GBEnemyCharacter.cpp:38` `ChestHeightFraction` |
| `EnemyStuckSpeedFraction` | 0.25 | `AI/GBEnemyCharacter.cpp:40` |
| `EnemySteerSweepRadiusScale` | 0.9 | `AI/GBEnemyCharacter.cpp:39` |
| `DuplicateGunMagazines` | 1 | `World/GBPickup.cpp:195` (`AddSpareMagazines(1)`) |
| `DropScatterRadius` | 90 | `World/GBPickup.cpp:25` |
| `TestSkillPointAmount` | 1 | `UI/GBHUD_Menus.cpp:829` (`AddPoints(1)`, the test button) |
| `RespawnHealthFraction`, `RespawnManaFraction`, `RespawnRefillAmmo` | 1, 1, 1 | checkpoint respawn, `Player/GBPlayerCharacter.cpp` near :576-580 and :840 |
| `EraTravelHealthFraction`, `EraTravelManaFraction`, `EraTravelRefillAmmo` | 1, 1, 1 | era travel, `World/GBEraSubsystem.cpp:196` |

For the six restore keys: on respawn the player is set to `fraction x max` (a respawn always starts from that value); on era travel health and mana are raised to **at least** `fraction x max` and never lowered. `...RefillAmmo` is 1 = refill every gun as today, 0 = leave the ammo as it is. If today's code is one shared "restore everything" function, give it parameters instead of copying it.

Also: `ElementalTickInterval` is read once in `BeginPlay` (`Combat/GBElementalStatusComponent.cpp:53`), so `gb.ReloadData` does not change it. Make it follow a reload.

## 2. `Player.json` "Stats"
| Key | Today | Where |
|---|---|---|
| `AccelerationPerSpeed` | 10 | `Player/GBPlayerCharacter.cpp:50` |
| `BrakingPerSpeed` | 8 | `:51` |
| `GroundFriction` | 8 | `:52` (`GroundFrictionValue`) |
| `InteractFacingMinDot` | 0.3 | `:48` |
| `LookSensitivity`, `StickYawRate`, `StickPitchRate`, `PitchLimit` | 2.5, 200, 140, 89 | `:40-44` (check the real names and values there and keep them) |
| `DashExitSpeedMult` | 1 | `:389`: speed kept after a dash = direction x walk speed x this |

## 3. Enemies: one new optional stat
`ProjectileGravity` (default 0) for enemy shots, read next to `ProjectileSpeed` / `ProjectileRadius` in `AI/GBEnemyCharacter.cpp:768-773` and passed to the projectile. Add `"ProjectileGravity": 0` to every enemy in `Enemies.json` whose `AttackMode` is `Ranged`.

## 4. Electric grenade (`Items.json`, `ElectricGrenade` "Stats"; code in `Items/GBInventoryComponent.cpp:260-280`, `UseThrowable`)
Today the grenade always uses linear blast falloff, never pushes, and bounces with the engine defaults. Read and add: `ExplosionFalloff` 1 (1 = linear falloff, 0 = full damage in the whole radius, same meaning as the spell param), `ExplosionKnockback` 0, `Bounciness` and `Friction` (today's values are the `UProjectileMovementComponent` defaults: look them up in the engine source and write those numbers). `Range` is already in the file.

## 5. Spells (`Spells.json` "Params", each `{ "Base": x, "Min": x, "Max": x, "ScaledBy": "None" }`)
- `TouchMargin` 15 on `FireWall` and `IceWall`: `Spells/GBSpell_Wall.cpp:18` `WallTouchMarginCm` (used near :141).
- `BlockArcDegrees` 180 on `IceShield`: `Spells/GBSpell_Shield.cpp:22` blocks when `Dot >= 0`, i.e. a 180 degree arc in front. Block when the angle between the shield's forward and the direction to the attacker is at most half this arc (compare the dot with `cos(arc/2)`; 180 must behave exactly as today, 360 blocks from every side).

## 6. QTE (`QTE.json` "Stats"; code in `Spells/GBSpellbookComponent.cpp` near :426-507)
Today a spell that fizzles (every QTE step missed) still costs its full mana and starts its full cooldown. Add:
- `QTE.FailManaRefund` 0: share of the mana cost given back when the spell fizzles (0 = today, 1 = full refund).
- `QTE.FailCooldownMult` 1: the cooldown of a fizzled spell is multiplied by this (1 = today, 0 = no cooldown).
Do not change what happens when the QTE itself refuses to start (that refund stays).

## Do NOT touch
- `Weapons/GBGun.*`, `Combat/GBStatusEffectComponent.*`, `Core/GBDevCommands.cpp` (another task is working there).
- `Tests/`, `World/GBTestArena.cpp`, `Glyphbound.Build.cs`, `Config/`.
- Guards and engineering limits (minimum tick interval, clamps of angles, array sizes, `NumSlots`), UI layout numbers, the visual constants of the explosion effect, the QTE gesture maths.
- No new tests: these are plumbing changes with unchanged behavior.

## Final report (plain text, short)
1. Every key you added: name, file, default, the code line that reads it.
2. Any literal from the list you did NOT move, and why.
3. Anything you expect may not compile (API you could not verify).
