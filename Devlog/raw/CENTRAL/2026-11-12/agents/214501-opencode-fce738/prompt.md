# Task: the real 3D pistol replaces the placeholder viewmodel

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit. Do not run `Tools/Jam/Import-Pistol.ps1` or touch anything in `Content/` except `Content/Data/*.json`.

The owner approved putting the first real gun model in the game. The architect already exported and imported it (do not redo that). These assets exist on this machine, in `/Game/Glyphbound/Weapons/Pistol/`:

| Asset | What | Origin (pivot) | Bounds in cm at scale 1 (min .. max) |
|---|---|---|---|
| `SM_Pistol_Body` | the whole gun that does not move (slide, grip, sight, panels, rails) | the hand point (top of the grip, under the slide) | (-17.5, -12.1, -45.1) .. (82.5, 9.0, 31.2) |
| `SM_Pistol_Trigger` | the trigger | its hinge | (-10.9, -2.6, -14.1) .. (2.6, 2.1, 0.1) |
| `SM_Pistol_Magazine` | the magazine | its own centre | (-11.4, -4.7, -17.3) .. (8.1, 4.7, 11.9) |
| `SM_Pistol_EnergyBar` | the energy bar on the side (cells that light up) | the hand point, same as the body | (19.6, -8.7, 3.8) .. (63.8, -6.3, 4.9) |

All four already face Unreal's convention: muzzle to +X, up +Z, the energy bar on -Y (the side the camera sees, like the old `Neon` part). The model is real-size-ish (1 m long), so the viewmodel scales it down.

Materials are already assigned on the meshes. Two parameters are for the game to set, on dynamic material instances:
- `ElementColor` (vector): exists on most slots (atlas glow, panel graphics, reticle, energy bar). Setting it on a slot that lacks it is harmless.
- `ImbueRemaining` (scalar 0..1): only on the energy bar's material. Cells light up from the grip end towards the muzzle as it grows; 0 = bar dark, 1 = full bar.

## 1. Which guns use it (data)
- Add `FString ViewModel` to `FGBWeaponDefinition` (`Data/GBDataTypes.h`), loaded from `Weapons.json` like the other fields (empty = the placeholder shapes, as today). Set `"ViewModel": "Pistol"` on the `Pistol` entry only. Shotgun and Rifle keep their placeholder shapes for now.

## 2. Building it (`Weapons/GBGun.h/.cpp`)
- In `BuildViewModel`, when `Definition.ViewModel == "Pistol"`: create four `UStaticMeshComponent`s (no collision, no overlap events, no shadow casting, like `AddPart`), load the meshes by path (`/Game/Glyphbound/Weapons/Pistol/SM_Pistol_Body.SM_Pistol_Body` etc.) and set them up:
  - `Body` attached to `Root`, relative scale `PistolViewScale` = 0.3 (uniform), location (0, 0, 0).
  - `EnergyBar` attached to `Body`, at (0, 0, 0).
  - `Trigger` attached to `Body`, relative location (28.84, 0, 0) (cm in the body's unscaled frame; the body's scale applies through attachment).
  - `Magazine` attached to `Body`, relative location (0, 0, -29.31).
  - Keep `Root->SetRelativeRotation(FRotator(0, -6, 0))` as today.
  - Put the numbers (scale, the two offsets, the motion values below) as named `constexpr` values in an anonymous namespace at the top of `GBGun.cpp`, with a one-line comment each saying where they come from (`Glyphbound/SourceArt/Weapons/Pistol/Export/export_report.json`, made by `Tools/Blender/pistol_export_fbx.py`). They are art tuning, not game balance, so they stay out of the JSON.
- If any of the four meshes fails to load (other machines do not have `Content/`), log ONE warning (`LogGB`) and build the placeholder shapes instead, so the game still runs.
- For every material slot of the four components, create a `UMaterialInstanceDynamic` (`CreateDynamicMaterialInstance`) and keep them in an array so the gun can set parameters every frame cheaply.
- Keep the placeholder path working exactly as today for the other guns.

## 3. Moving parts (Tick, only while the pistol viewmodel exists)
- **Trigger pull:** on every shot (`PerformAttack` that fires), play a short pull: the trigger's relative rotation goes from 0 to `FRotator(-8, 0, 0)` (pitch -8: the bottom of the trigger moves back towards the grip) and back to 0, over `TriggerPullSeconds` = 0.12 s in total (linear in, linear out). A new shot restarts it.
- **Magazine during reload:** drive it from `GetReloadProgress()` (0..1) while reloading. Offset from its rest location, in the body's frame, `MagazineOutOffset` = (-12.15, 0, -24.11) cm (back and down along the grip). Progress 0 to 1/3: slide out from rest to the full offset; 1/3 to 2/3: stay out; 2/3 to 1: slide back in. Smoothstep each slide. When not reloading (including a cancelled reload), it sits at rest. This mirrors the Blender animation (frames 24-40 out, 40-56 held, 56-72 in).
- **Glow colour and energy bar** (GDD 6, weapon enchant): every tick, ask `GetActiveEnchant(Element, Effect)`.
  - Enchanted: `ElementColor` = `GBPlaceholder::ElementColor(element)` on every slot, and `ImbueRemaining` = `TimeRemaining / TotalDuration` of that enchant's status effect (find the active effect whose `EffectId` starts with `Enchant_`, the way `UI/GBHUD.cpp` does; clamp to 0..1; if `TotalDuration` is 0, use 1).
  - Not enchanted: `ElementColor` = the gun's own colour `PistolIdleGlow` = (0.02, 1.0, 0.07) (the model's original green), `ImbueRemaining` = 0 (bar dark).
  - Only call `SetVectorParameterValue` / `SetScalarParameterValue` when the value changed (cache the last ones), not every frame.
- The old `Neon` placeholder part is not created for the pistol: the energy bar replaces it.

## 4. Cooking
- The meshes are loaded by path, so the cooker will not find them by reference. Add `+DirectoriesToAlwaysCook=(Path="/Game/Glyphbound/Weapons")` next to the other `DirectoriesToAlwaysCook` lines in `Config/DefaultGame.ini`.

## 5. Smoke script (`Core/GBDevCommands.cpp`)
- When the pistol is in hand (the smoke run already equips a gun; make sure it is the `Pistol`, or switch to it for these steps and back after):
  - log `SMOKE pistol viewmodel meshes=%d mids=%d` (components with a loaded `SM_Pistol_*` mesh, must be 4; dynamic material instances made, must be > 4), else `UE_LOG(LogGB, Error, ...)`.
  - take a screenshot `13a_pistol_hip` (pistol in hand, not enchanted).
  - start a reload (spend a few rounds first if the magazine is full), wait until progress is about 0.5, log `SMOKE pistol magazine offset=%.1f` (distance in cm of the magazine from its rest location, in the body's frame: must be about 27.0, +-1.0), take a screenshot `13b_pistol_reload`.
  - with the enchant active (the smoke run already casts it at "weapon enchant"; reuse that or cast again), log `SMOKE pistol glow element=%s imbue=%.2f` (the element name and the ImbueRemaining value sent to the material; imbue must be > 0 and <= 1) and take a screenshot `13c_pistol_enchanted`.
- Keep every existing smoke step and screenshot working; shift the timeline if needed.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 36` passes (no new test is needed: the moving parts are seen by playing).
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the new SMOKE lines, and no `LogGB` warning.
Then look at the three new screenshots yourself and say in the report what you see (is the pistol visible in the lower right, pointing forward, the right size, the bar glowing when enchanted). If it is clearly off-screen, far too big or far too small, adjust only `PistolViewScale` and the body location and say so.
Never make an error disappear by changing the expected numbers or deleting behaviour: if a check fails, fix the code. All JSON files stay valid.

## Final report (plain text)
1. The four result lines verbatim, plus the new SMOKE lines.
2. What you changed, with file names.
3. What you saw in the three screenshots, and any tuning value you changed.
4. Anything you could not do as described, and why.
5. What a person should check by playing.
