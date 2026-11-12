# Task: holographic ammo count and weak-point light on the pistol

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first (you DO build and run things in this task; the rest of "Hard rules" applies). One build or run at a time. Do not open the Unreal Editor. Do not commit. Do not run `Tools/Jam/Import-Pistol.ps1` or touch `Content/` except `Content/Data/*.json`.

Owner's notes after seeing the pistol (Spanish, translated):
1. "The panel at the back of the pistol, the one facing the player, has to show the number of bullets left, with a more or less holographic panel look."
2. "The lit bar on the side drains in the wrong direction; it must drain left to right." **Already done by the architect in the asset** (each of the 13 cells now has one `ImbueU` value and `M_GunEnergy` darkens them from the muzzle end). The code keeps sending `ImbueRemaining` exactly as today; do not change that.
3. "The other light next to the bar, closer to the muzzle, must light up when the player hits a weak point or makes a headshot. Otherwise it stays off."

The architect re-exported the pistol. In `/Game/Glyphbound/Weapons/Pistol/` there are now **six** meshes; the two new ones, both with their origin at the body origin (attach to the body at (0, 0, 0), like `SM_Pistol_EnergyBar`):

| Asset | What | Material and its parameters |
|---|---|---|
| `SM_Pistol_WeakPointLight` | the small light next to the bar, towards the muzzle (it used to be part of the bar mesh) | `M_GunIndicator`: `Flash` (scalar, 0 = off, 1 = full glow), `ElementColor` (same as the other slots) |
| `SM_Pistol_AmmoDisplay` | one quad just in front of the rear screen, facing the player; UV 0..1, U to the player's right, V down (a texture reads upright) | `M_GunHolo` (additive, unlit, scanlines and flicker already in the material): `DisplayTexture` (texture: what to show, white on black, the red channel glows), `ElementColor` |

Everything in `Weapons/GBGun.h/.cpp` from the previous task stays (look at `TryBuildPistolViewModel` and `UpdatePistolViewModel`).

## 1. Build the two new parts
- Load and attach both in `TryBuildPistolViewModel`, same settings as the other parts; include them in the "all meshes loaded" check (missing = one warning, placeholder shapes, as today) and in the dynamic material instances. Keep a pointer to the indicator's MID and the display's MID.
- `GetPistolMeshCount` now counts six.

## 2. Holographic ammo count
- Create a `UCanvasRenderTarget2D` at runtime (256 x 128, black clear colour), draw the text into it in its update delegate, and set it as `DisplayTexture` on the display MID.
- Text: the rounds left in the magazine (`Ammo.InMagazine`), e.g. `12`, `7`, `0`; while reloading, `--`. White, centred, as large as fits (about 80% of the height), drawn with an engine font (`GEngine->GetLargeFont()` scaled, or a better engine font if you find one that loads in a packaged build). Optionally a thin white frame line inside the edges if it looks good; nothing else.
- Redraw ONLY when the shown text changes (cache it; call `UpdateResource()` then), never every frame. The ammo changes in `PerformAttack`, reload start/finish/cancel, `SetAmmo`, `RefillAmmo`; a simple check in `UpdatePistolViewModel` comparing the wanted text with the cached one is enough.
- The display glows in `ElementColor` like the rest (it already gets it with all the MIDs).

## 3. Weak-point light
- Off (`Flash` = 0) all the time, except: when a hit that **this gun's owner** made is a head / weak-point hit (`FGBDamageInfo::bHeadshot`) **while this gun is the equipped one**, set `Flash` = 1, hold 0.12 s, then fade linearly to 0 over 0.3 s. A new weak-point hit restarts it. Put the three times / values as named constants at the top of `GBGun.cpp` like the other art values (`WeakPointFlashHold`, `WeakPointFlashFade`), commented "provisional, owner's note 3".
- Hook: the global damage event `UGBCombatLibrary::OnDamageDealt` that the player's Precision Recovery already uses (see `Player/GBPlayerCharacter.cpp`: how it binds in `BeginPlay` and unbinds in `EndPlay`). Bind in the gun only when the pistol viewmodel exists, unbind on `EndPlay`. It covers hitscan and projectile shots alike. Filter on the instigator being this gun's player owner and the gun being equipped. One shotgun-style multi-pellet hit would just restart the flash; that is fine.
- Only call `SetScalarParameterValue` when the value changed.

## 4. Smoke script (`Core/GBDevCommands.cpp`)
Extend the pistol steps (they exist: `13a`..`13c`):
- log `SMOKE pistol display text=%s` (the cached shown text) with the magazine full, then after one shot `SMOKE pistol display after shot text=%s`; the two must differ by one round (e.g. `12` then `11`), else `UE_LOG(LogGB, Error, ...)`. Log the text during the reload too (`--`).
- weak point: make a weak-point hit with the player as instigator on the training dummy or a fresh enemy the way the Precision Recovery smoke step does (`DealDamage` with `bHeadshot = true`), then log `SMOKE pistol weakpoint flash=%.2f` on the next step (must be > 0.5) and take a screenshot `13d_pistol_weakpoint`; one second later log `SMOKE pistol weakpoint flash after=%.2f` (must be 0).
- a body-shot hit (`bHeadshot = false`) must leave it at 0: `SMOKE pistol bodyshot flash=%.2f`.
- if the status effect component lets you set an active effect's `TimeRemaining` (or there is a clean way to do it for the smoke run only), set the enchant to half its duration and take `13e_pistol_half_bar`; log `SMOKE pistol half bar imbue=%.2f`. If there is no clean way, skip it and say so.
- Keep every existing step and screenshot working.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 36` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`, with `SMOKE path check passed`, the new SMOKE lines, `SMOKE pistol viewmodel meshes=6 ...`, and no `LogGB` warning.
Then open the new screenshots (and `13a`) and look at the rear display: is the number readable, upright and not mirrored, does the light glow in `13d`, and in `13e` are the lit cells at the grip end (right on screen) and the dark ones at the muzzle end (left)? If the number is mirrored or upside down, fix it in code (flip when drawing) and say so.
Never make an error disappear by changing the expected numbers or deleting behaviour: if a check fails, fix the code.

## Final report (plain text)
1. The four result lines verbatim, plus the new SMOKE lines.
2. What you changed, with file names.
3. What you saw in the screenshots.
4. Anything you could not do as described, and why.
5. What a person should check by playing.
