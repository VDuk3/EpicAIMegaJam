# Task: a strong hit interrupts the flamethrower, with a camera shake

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit.

This follows the previous batch (`Prompts/feedback6_perks_flamethrower.md`, already done): the flamethrower (`Spells/GBSpell_Beam.*`) now runs while its key is held. Owner (translated): "The flamethrower should be interrupted if the player takes a hit that removes 30% or more of their health, with a shake effect when that happens. Add it to the spell description, but say 'a strong hit' or similar, not the percentage." GDD v0.21, section 18 "Strong hit interrupts the flamethrower". Implement exactly this:

## 1. The rule (data-driven, any sustained spell can use it)
- New optional beam param `InterruptHitFraction` (document it in the header's param list): > 0 means a single hit on the caster whose damage actually taken is >= `InterruptHitFraction * caster max health` ends the spell at once. 0 / absent = never interrupted. Flamethrower in `Spells.json`: Base/Min/Max 0.3, ScaledBy None.
- "Damage actually taken" = after the ice shield, armor and resistances: listen to the damage event the caster's `UGBAttributeComponent` already broadcasts when health goes down (find it; the HUD uses one for the player). Measure against **maximum** health, not current.
- Pure function (testable), static on `AGBSpell_Beam`: `static bool IsInterruptingHit(float DamageTaken, float MaxHealth, float InterruptHitFraction);` (false when the fraction <= 0 or max health <= 0; true when damage >= fraction * max health). Test `Glyphbound.Spells.BeamInterrupt`: below, exactly at, and above the threshold; fraction 0 never interrupts; a hit that kills also counts. Check the name rule.
- Subscribe when the beam starts and unsubscribe when it ends (no dangling delegates; the beam can end by release, mana, death, switch, interrupt).
- When interrupted: `EndSpell()`, show a short HUD message `Interrupted!` the way other cast messages are shown (find how the HUD shows cast failures / messages; reuse it, do not add a new UI system), and start the camera shake below. Mana already spent is not refunded; cooldown unchanged.

## 2. Camera shake (code only, no assets, no plugins)
- Do NOT use `UCameraShakeBase` patterns from the EngineCameras plugin (it may be disabled; check `Glyphbound.uproject` / `Tools/Jam/trim_plugins.js` and do not enable plugins).
- Add `void AGBPlayerCharacter::StartViewShake(float Duration, float AmplitudeDeg, float Frequency);` that offsets the **first-person camera component's relative rotation** (not the control rotation, so the aim does not move) with a decaying shake for `Duration` seconds, then returns it exactly to zero. Pitch and yaw offsets from a cheap deterministic noise (e.g. sines at slightly different frequencies, or `FMath::PerlinNoise1D`), amplitude fading linearly to 0. Calling it again while shaking restarts it with the stronger amplitude.
- Make sure the pixel-art hands / held item still look right during the shake (if they are attached to the camera they move with it, which is fine).
- New `Game.json` `Values`: `InterruptShakeDuration` 0.35, `InterruptShakeAmplitude` 2.5 (degrees), `InterruptShakeFrequency` 22 (Hz). The beam reads them when it is interrupted.

## 3. Description
`Spells.json` Flamethrower `Description`: "A stream of fire from your hand. Hold the spell key to keep it going; it drains mana. A strong hit interrupts it." (No number anywhere in player-facing text.)

## 4. Smoke script (`Core/GBDevCommands.cpp`)
Next to the flamethrower step of the previous batch: start the flamethrower again (QTE off), and while it is running deal the player one hit through `UGBCombatLibrary::DealDamage` of 35% of max health (from a harmless source; make sure the ice shield is not raised and the player does not die; restore health afterwards), then log `SMOKE flamethrower interrupt alive=%d health=%.0f`; alive must be 0, else `UE_LOG(LogGB, Error, ...)`. Also deal a 10% hit to a second running flamethrower and check it is still alive (`SMOKE flamethrower small hit alive=%d`, must be 1) **before** its MinDuration ends, so the check is meaningful (the key is not held in an automated run, so the beam stops after MinDuration anyway). Take a screenshot `11_interrupt_shake` one frame-safe moment after the strong hit (about 0.1 s later, while the shake is visible; the HUD message should show).

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 30` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the new SMOKE lines, and no `LogGB` warning.
Never make an error disappear by deleting behavior. All JSON files stay valid. No gameplay numbers in code.

## Final report (plain text)
1. The four result lines verbatim, plus the new SMOKE lines.
2. What you changed, with file names; which damage event you used and why it is "after the shield".
3. Anything you could not do as described, and why.
4. What a person should check by playing.
