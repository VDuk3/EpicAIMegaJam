# Task: weapon tree rework and three new weapon perks

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

Owner's request (translated): "Add these perks and fix the weapon branch. Precision Recovery heals a fixed 3% of max health, but its internal cooldown goes down with the ranks: 6 / 5 / 4 s. Deadeye gives +0.25x damage per rank." GDD v0.24, section 5 and section 18 "Weapon tree (owner, v0.24)". Every number below goes in data, never in code.

## 1. The new layout (`Content/Data/Skills.json`, tree `Weapon`)
Three columns (0 left, 1 middle, 2 right). Keep the existing node ids; change only `Column`, `Row`, `Requires` and what is listed here.

| Node | Id | Col | Row | Requires | Cost | Max rank | Modifiers per rank |
|---|---|---|---|---|---|---|---|
| Heavy Rounds | `Weapon_Damage` | 1 | 0 | - | 1 | 3 | unchanged |
| Quick Trigger | `Weapon_FireRate` | 0 | 1 | Weapon_Damage | 1 | 3 | unchanged |
| Rapid Reload (new) | `Weapon_ReloadSpeed` | 0 | 2 | Weapon_FireRate | 1 | 3 | `Weapon.All.ReloadTime` Multiply 0.9 (stacks to -30% at rank 3) |
| Steady Grip | `Weapon_Recoil` | 0 | 3 | Weapon_FireRate | 1 | 3 | `Weapon.All.RecoilPitch` Multiply 0.8 **and** `Weapon.All.RecoilYaw` Multiply 0.8 |
| Marksman | `Weapon_Headshot` | 2 | 1 | Weapon_Damage | 2 | 3 | unchanged |
| Precision Recovery (new) | `Weapon_PrecisionRecovery` | 2 | 2 | Weapon_Headshot | 1 | 3 | `PrecisionRecovery` Add 1 (a player stat = the rank, see 2) |
| Arcane Conduit (new) | `Weapon_EnchantBuildUp` | 2 | 3 | Weapon_Headshot | 1 | 3 | `Weapon.All.EnchantBuildUpMult` Multiply 1.1 (stacks to +30%) |
| Deadeye (new) | `Weapon_PistolHeadshot` | 0 | 4 | Weapon_Damage | 1 | 3 | `Weapon.Pistol.HeadshotMult` Add 0.25 |
| Point Blank | `Weapon_ShotgunClose` | 1 | 4 | Weapon_Damage | 1 | 3 | unchanged |
| Long Shot | `Weapon_RifleFar` | 2 | 4 | Weapon_Damage | 1 | 3 | unchanged |

Check how a `Multiply` op scales with the rank (the architecture says linear per rank: 0.9 at rank 3 = 0.7) and confirm it in your report.

Descriptions (English, player-facing, short, in the same style as the others):
- Rapid Reload: "10% faster reloads per rank."
- Steady Grip: "20% less recoil per rank." (unchanged text)
- Precision Recovery: "Defeating an enemy with a head or weak point hit restores 3% of your max health. Cooldown 6 / 5 / 4 s by rank." (Read the 3% and the cooldowns from the data when building the text only if the HUD already supports that; otherwise a fixed string is fine.)
- Arcane Conduit: "+10% elemental build-up from enchanted weapons per rank."
- Deadeye: "Pistol: +0.25x head / weak point damage per rank."
Make sure the skill-tree screen (`UI/GBHUD_Menus.cpp`) draws 5 rows and the connector lines for these `Requires` cleanly (the spell tree already uses row 4; check the weapon tree does too, in both UI styles). Take a look at the layout in the smoke screenshot of the skill screen and fix overlaps if there are any.

## 2. Precision Recovery (new code)
- Rule: when an enemy dies and its **killing blow** was a head / weak point hit (`FGBDamageInfo::bHeadshot` on the killing blow) whose `Instigator` is the player, and the player has `PrecisionRecovery` rank >= 1 and the internal cooldown is over, the player heals `PrecisionRecoveryHealFraction` x max health and the cooldown starts. Once per enemy death by construction (a shotgun's pellets kill once). Hitting a head without killing never heals.
- Find where enemies die (`AGBEnemyCharacter`, `OnDied`, the attribute component's `OnDeath` carries the killing blow) and pick the cleanest hook that gives you the killing blow and its instigator; tell me which in the report. Keep the logic in the player (or a small component on the player), not in the enemy.
- `Game.json` `Values` (new): `PrecisionRecoveryHealFraction` 0.03, `PrecisionRecoveryCooldownBase` 7, `PrecisionRecoveryCooldownPerRank` 1, `PrecisionRecoveryCooldownMin` 1. Cooldown = max(Min, Base - PerRank x rank) -> 6 / 5 / 4.
- Pure function, static and testable (place it next to similar combat helpers, e.g. `UGBCombatLibrary`): `static float ComputePrecisionRecoveryCooldown(int32 Rank, float Base, float PerRank, float MinCooldown);` (rank <= 0 returns 0 = perk off; callers check the rank first). And the decision `static bool ShouldPrecisionRecover(bool bKillingBlowWeakPoint, bool bInstigatorIsPlayer, int32 Rank, float TimeSinceLastHeal, float Cooldown);`.
- Test `Glyphbound.Combat.PrecisionRecovery`: cooldowns 6/5/4 for ranks 1/2/3, rank 0 never heals, a body-shot kill never heals, another instigator never heals, inside the cooldown no heal, exactly at / after the cooldown heals. Check the test name rule (no name may be a prefix of another, e.g. not `Combat.Precision` next to it).
- Show a short heal feedback the way other heals show (a green damage-number-style "+N" if the damage numbers support heals, or the existing HUD message), no new UI system.
- Use a world-time stamp for the cooldown (not a timer per kill). Reset is not needed on death or checkpoint.
- `PrecisionRecovery` is a player stat key (no `Spell.` / `Weapon.` prefix): check how other player stat modifiers from skills reach the player (`UGBStatsComponent`, e.g. how `MoveSpeed` or another non-prefixed key works) and do it the same way.

## 3. Arcane Conduit (enchant build-up)
- New weapon stat `EnchantBuildUpMult` (add to `Core/GBStatNames.h`, value 1 in the `Stats` of every gun in `Weapons.json` so the modifier key is valid for the load-time check; keep the stat list in `GBGun.h`'s comment current).
- In `AGBHeldItem::MakeDamageInfo` (`Weapons/GBHeldItem.cpp`, the enchant branch near `GetActiveEnchant`): when the hit's element comes **from the enchant**, multiply `Info.BuildUp` by `GetStat(EnchantBuildUpMult, 1)`. The elemental sword's own element (not an enchant) is not affected. Keep the weak-point build-up rule as it is.

## 4. Rapid Reload
`ReloadTime` already goes through `GetStat` with modifiers; verify that the reload progress bar on the HUD follows the shorter time.

## 5. Smoke script (`Core/GBDevCommands.cpp`)
- Give the player the skills through the skill tree component (the smoke run already adds points and unlocks nodes for other checks; follow that), then:
  - Precision Recovery rank 1: set the player's health to 50% of max, kill a fresh weak enemy (or the training dummy if it can die; otherwise spawn an orc the way the smoke run already does) with a `DealDamage` killing blow that has `bHeadshot = true` and the player as instigator; log `SMOKE precision recovery healed=%.1f` (health gained), which must equal 3% of max health (+-0.5), else `UE_LOG(LogGB, Error, ...)`. Then a second weak-point kill right away must heal 0 (cooldown): `SMOKE precision recovery cooldown healed=%.1f`, must be 0.
  - Rapid Reload rank 3: log `SMOKE rapid reload pistol=%.2f` with the pistol's `GetStat(ReloadTime)`, which must be 0.7 x its base (+-0.01).
  - Arcane Conduit rank 3 with an enchanted gun: log `SMOKE arcane conduit buildup=%.2f base=%.2f` from `MakeDamageInfo` before and after; must be 1.3x (+-0.01).
- Take a screenshot `12_weapon_tree` of the weapon tree screen with a few of the new nodes unlocked.
- Reset the skills afterwards so the later smoke steps start from the same state as before (the reset exists: `UGBSkillTreeComponent::ResetAll`).

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 36` passes (35 today + yours).
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the new SMOKE lines, and no `LogGB` warning (the load-time modifier key check must stay silent).
Never make an error or a failing test disappear by changing the expected numbers or deleting behavior: if a check fails, fix the code. All JSON files stay valid.

## Final report (plain text)
1. The four result lines verbatim, plus the new SMOKE lines.
2. What you changed, with file names; the death hook you used for Precision Recovery and why; how `Multiply` scales with rank.
3. Anything you could not do as described, and why.
4. What a person should check by playing.
