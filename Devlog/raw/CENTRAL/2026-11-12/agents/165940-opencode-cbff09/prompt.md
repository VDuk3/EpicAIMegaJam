# Task: six new spell perks, a param cap, and a hold-to-sustain flamethrower

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

Owner's requests, GDD v0.20 (changelog; section 5 "New perks"; section 6 fire row; section 18 "Sustained spells" and "New spell perks"). The architect decided the design below; implement exactly this.

## 1. Six new nodes in `Glyphbound/Content/Data/Skills.json` (data only)
All `"Tree": "Spell"`, `"Cost": 1`, `"MaxRank": 3`, no `Requires`. Columns follow the existing spell tree (column 0 fire, 1 lightning, 2 ice). Modifier ops behave like the existing nodes (`Multiply` stacks linearly per rank: 1.15 at rank 1, 1.30 at rank 2, 1.45 at rank 3).

| Id | DisplayName | Description | Modifier per rank | Column | Row |
|---|---|---|---|---|---|
| `Spell_FireWallDuration` | Lingering Flames | +15% fire wall duration per rank. | `Spell.FireWall.Duration` Multiply 1.15 | 0 | 2 |
| `Spell_FlamethrowerDamage` | Intense Heat | +12% flamethrower damage per rank. | `Spell.Flamethrower.DamagePerSecond` Multiply 1.12 | 0 | 3 |
| `Spell_RuneRadius` | Expanded Sigil | +10% lightning rune blast radius per rank. | `Spell.LightningRune.Radius` Multiply 1.10 | 1 | 3 |
| `Spell_OrbDuration` | Persistent Charge | +15% static orb duration per rank. | `Spell.StaticOrb.Duration` Multiply 1.15 | 1 | 4 |
| `Spell_ShieldDuration` | Enduring Guard | +15% ice shield duration per rank. | `Spell.IceShield.Duration` Multiply 1.15 | 2 | 1 |
| `Spell_IceFloorSlow` | Deep Freeze | Ice floor slows 5 points more per rank (up to 80%). | `Spell.IceFloor.EffectStrength` Add 0.1 | 2 | 2 |

Before writing them, check in the code that: the rune's blast radius is the `Radius` param (not `TriggerRadius`), the orb and shield read `Duration` through the resolved cast (so `Spell.<Id>.Duration` modifiers reach them), and the ice floor's slow is `Slow` (`MoveSpeed` Multiply 0.5) scaled by `EffectStrength` so that strength +0.1 = +5 points of slow (read how `UGBStatusEffectComponent` applies a strength scale to a Multiply modifier). If any of these is not true, tell me in the report and pick the param that matches the description; do not change gameplay code to make the table fit.
Check that the spell tree screen still fits (`AGBHUD::DrawSkillTrees`, now 5 rows in the spell tree) at 1280x720: boxes must stay readable (names abbreviated is fine); if a row would overflow the panel, fix the layout math, not the data.

## 2. Validation of modifier keys (so a typo can never silently do nothing)
In `UGBDataSubsystem`, after loading (and after `gb.ReloadData`), check every skill node modifier (and armor / status effect modifiers if they share the struct and it is cheap): a key `Spell.<Id>.<Param>` must name an existing spell id (or `All`) and a param that exists in that spell's `Params` (for `All`: in at least one spell). Otherwise `UE_LOG(LogGB, Warning, ...)` naming the node and the key. Do the same for `Weapon.<Id>.<Stat>` only if the weapon stat names are easy to enumerate; otherwise skip weapons and say so. The smoke run fails on LogGB warnings, so a wrong key in item 1 will show up.

## 3. Optional `Cap` per spell parameter
- `Data/GBDataTypes.h`: the scaled param struct gets `float Cap = 0.f;` (0 = no cap; comment: upper limit applied after QTE scaling AND modifiers).
- `Spells/GBSpellTypes.cpp`, `GBSpell::ResolveCast` and the single-param resolve below it: after `ApplySpellModifiers`, if `Cap > 0`, `Value = FMath::Min(Value, Cap)`. Put the clamp in one small pure function and use it in both places.
- `Spells.json`, `IceFloor.Params.EffectStrength`: add `"Cap": 1.6` (= 80% slow).
- Test `Glyphbound.Spells.ParamCap` (pure): a value above the cap is clamped, below is unchanged, cap 0 means no cap. Check the name rule.

## 4. Flamethrower: runs while its key is held
Today `AGBSpell_Beam` runs for `Duration` or until the slot key is pressed again (`UGBSpellbookComponent::TryCastSlot` -> `OnCastReleased`), and only `ETriggerEvent::Started` is bound for spell slots (`AGBPlayerCharacter`, around `BindAction(S->SpellSlot1 ...)`).
New behavior, as data-driven beam params (document them in the header's param list):
- `HoldToSustain` (1 = the beam runs only while the cast key is held; 0 = old behavior). Flamethrower: 1.
- `MinDuration` seconds: the beam always runs at least this long (so a cast whose key was released during the QTE is not wasted). Flamethrower: 0.75.
- `Duration` <= 0 means no time limit. Flamethrower: remove `Duration` from its params, or set Base/Min/Max 0 (no limit). Its QTE power still scales `DamagePerSecond` as today.
- `ManaPerSecond`: flamethrower 15 (Base/Min/Max 15, ScaledBy None). The beam already stops when mana runs out; keep that. `ManaCost` (paid at the cast): flamethrower 10.
- Description in `Spells.json`: "A stream of fire from your hand. Hold the spell key to keep it going; it drains mana."
- Pure function (testable), static on `AGBSpell_Beam`: `static bool ShouldBeamContinue(bool bHoldToSustain, bool bKeyHeld, float Elapsed, float MinDuration, float MaxDuration);` -> false when `MaxDuration > 0 && Elapsed >= MaxDuration`; true while `Elapsed < MinDuration`; otherwise `!bHoldToSustain || bKeyHeld`. Use it each damage tick (or each frame) to decide whether to `EndSpell()`. Test `Glyphbound.Spells.BeamHold` covering: held keeps going, released after the minimum stops, released before the minimum keeps going, max duration stops even if held, hold mode off ignores the key. Mana running out is handled where it is today.
- Is the key held? Add `bool AGBPlayerCharacter::IsSpellInputHeld(FName SpellId) const`: finds the loadout slot of that spell (`UGBSpellbookComponent`) and reads the current value of that slot's input action from Enhanced Input (keyboard / mouse: slot 1 = right click, slots 2-5 = keys 1-4; gamepad: L1 for slot 1, L2 + face button for slots 2-5: read the action value, which already includes those mappings and chords). If the caster is not the player, or the spell is not in the loadout, treat it as held (the beam then ends by mana, death, switch or MinDuration rules). The beam asks its caster through this function.
- `TryCastSlot`'s "press again to stop" stays (harmless: with hold mode the key is held while it runs).
- Weapon switch, death and the QTE rules are unchanged; the cooldown still starts at the cast.

## 5. Smoke script (`Core/GBDevCommands.cpp`)
- Where it unlocks a node: also log the number of skill nodes loaded (`SMOKE skills loaded=%d`), expect 18.
- Add a short step that casts the flamethrower with the QTE off while the smoke script cannot hold a key (the player's key is not held in an automated run): log `SMOKE flamethrower started=%d` right after the cast and `SMOKE flamethrower after=%.2fs alive=%d` about 1.5 s later; with the key not held it must have stopped after `MinDuration` (alive=0), else `UE_LOG(LogGB, Error, ...)`. Put it where it does not disturb later steps (the end is fine).

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 29` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the new SMOKE lines, and no `LogGB` warning. Open `5_skills.png` and say whether the spell tree fits and is readable.
Never make an error disappear by deleting behavior. All JSON files stay valid. No gameplay numbers in code.

## Final report (plain text)
1. The four result lines verbatim, plus the new SMOKE lines.
2. What you changed, with file names; the result of the checks asked in item 1.
3. Your description of `5_skills.png`.
4. Anything you could not do as described, and why.
5. What a person should check by playing.
