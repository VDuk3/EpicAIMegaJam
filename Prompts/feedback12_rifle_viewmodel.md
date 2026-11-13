# Task: the assault rifle viewmodel (and the same gun-model code for both guns)

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first. You DO build and run things in this task; the rest of "Hard rules" applies. Constraints:
- One build or run at a time.
- Do not open the Unreal Editor and do not launch PIE.
- Do not commit.
- Do not run `Tools/Jam/Import-*.ps1`, and do not touch `Content/` except `Content/Data/*.json`.

Owner (Spanish, translated): "This is the game's assault rifle. It has all the work ahead of it: the animations, the colour change, the ammo panel. Everything as you already know: it has to end up as a complete and fully functional weapon, with everything a game of this kind needs."

The architect already did the art. `/Game/Glyphbound/Weapons/Rifle/` holds five meshes, all imported like the pistol's: muzzle +X, origin of the body at the hand point (top of the pistol grip), source in metres, so 1 unit = 1 cm unscaled; the body is 100 cm long. The parts:

| Asset | What | Attach to the body at (cm, body's unscaled frame) | Material |
|---|---|---|---|
| `SM_Rifle_Body` | everything that does not move. **The energy bar is inside the body material**: the side neon strip is split into 13 cells by `M_RifleAtlas` and drains with `ImbueRemaining` exactly like the pistol bar (same "+ half a cell" rule, 13 cells). The neon off the strip glows in `ElementColor` all the time. | (0, 0, 0) | `M_RifleAtlas`: `ElementColor`, `ImbueRemaining` |
| `SM_Rifle_Trigger` | trigger blade, origin at its hinge (top) | (6.5, 0, 0.3) | `M_RifleAtlas` |
| `SM_Rifle_Magazine` | curved box magazine under the well in front of the trigger guard, origin at its top centre; a glowing slit on each side (`M_GunSolid` instance with `Glow`) | (15.8, 0, -2.8) | 3 slots, `M_GunSolid` instances: `ElementColor` |
| `SM_Rifle_WeakPointLight` | the two oval lenses on the sides of the sight | (0, 0, 0) | `M_RifleIndicator`: `Flash` (0 = off, 1 = full), `ElementColor` |
| `SM_Rifle_AmmoDisplay` | a 2:1 quad just behind the sight's eyepiece, facing the player; same UV convention as the pistol's | (0, 0, 0) | `M_GunHolo` (shared with the pistol): `DisplayTexture`, `ElementColor` |

Rifle art values; source: `Glyphbound/SourceArt/Weapons/Rifle/export_report.json`, written by `Tools/Blender/rifle_export_fbx.py`. Name them like the pistol's and mark them provisional:
- Magazine slide-out vector on reload: (2.0, 0, -17.0) cm in the body's unscaled frame. Use the same out / hold / in smoothstep as the pistol.
- Trigger pull: pitch -8 deg, 0.12 s (same as the pistol).
- Idle glow (no enchant): mint (0.05, 0.9, 0.6), linear colour. This is the model's own neon colour, as green is the pistol's.
- Muzzle tip: (69.0, 0, 6.6) cm unscaled. Pistol muzzle tip: (82.5, 0, 20.2) cm unscaled (measured in Blender).
- View scale, a starting value: 0.6 (the pistol uses 0.3 on a ~100 cm model). The body reaches 31 cm behind the hand point, so at 0.6 the stock ends about 11 cm in front of the camera: `HeldItemOffset` is (30, 15, -15) in `Player/GBPlayerCharacter.cpp`.
  - Tune the scale and a per-model root offset (cm, relative to the anchor) from the smoke screenshots so that:
    - the rifle reads as a rifle in the lower right (stock towards the camera, muzzle towards the crosshair);
    - it does not clip the near plane;
    - the rear ammo display is visible.
  - Keep the pistol's look exactly as it is.
- Ammo display layout: the rifle's pane has no frame line and no dots, so its text layout differs from the pistol's.
  - Provisional: text height share 0.75, width share 0.85, centre Y 0.47.
  - The magazine holds 30, so the display shows `30`, `29` ... and `--` while reloading.

## 1. One gun-model code path for both guns
- `Weapons/GBGun.h/.cpp` today has `TryBuildPistolViewModel`, `UpdatePistolViewModel`, `Pistol*` members and `GetPistol*` accessors. Turn this into one code path driven by a small per-model art description (a struct, one constant instance per model in `GBGun.cpp`). It holds:
  - the asset folder and name prefix;
  - which parts exist: the rifle has no separate energy-bar mesh;
  - offsets, magazine vector and scale;
  - idle glow, muzzle tip and display layout;
  - the root offset and yaw.
- `Definition.ViewModel` picks the model: `"Pistol"` or `"Rifle"`. Any other value, or missing meshes, gives one warning and the placeholder shapes, as today.
- Everything the pistol does today must stay byte-for-byte the same in behaviour:
  - trigger, magazine, glow, the "+ half a cell" imbue rule;
  - weak-point flash (`PistolWeakPointFlashHold` / `Fade` in `Game.json` now apply to both guns; keep the key names, they are data);
  - the holographic display drawn only when the text changes.
- Rename members and accessors to neutral names (`ViewModel*`, `GetViewModelMeshCount`, ...) and update every caller (`Core/GBDevCommands.cpp`).
- All MIDs get `ElementColor` and `ImbueRemaining` as today.
- `Weapons.json`: add `"ViewModel": "Rifle"` to the `Rifle` entry, nothing else.

## 2. New for both guns: shot kick and muzzle flash
Every gun in a shooter needs these, and neither gun has them yet.
- **Kick:** on each shot, the viewmodel root moves back 1.2 cm and pitches up 1.5 deg, then returns over 0.09 s (ease out).
  - The values are provisional constants per model: rifle 1.2 cm / 1.5 deg, pistol 1.6 cm / 3 deg.
  - Automatic fire at 0.1 s just keeps restarting it; that is fine.
  - It must not move the aim, the muzzle location used for traces, or the camera; it is the look only.
- **Muzzle flash:** on each shot, a small glowing shape at the muzzle tip in the current glow colour, visible for 0.05 s with a random roll and a slight random scale.
  - Reuse `GBPlaceholder::SetupShape` with `bGlow`, or a one-quad plane; no new assets.
  - Hidden the rest of the time.
  - It must not cast shadows or collide.

## 3. Smoke script (`Core/GBDevCommands.cpp`)
Keep every existing step, line and screenshot working, including all `SMOKE pistol ...` lines. After the pistol steps, add rifle steps with the same pattern. Equip with `GiveWeapon(TEXT("Rifle"), ...)` the way the pistol step does, with a full magazine.
- `SMOKE rifle viewmodel meshes=%d mids=%d`: meshes must be 5, else `UE_LOG(LogGB, Error, ...)`. Screenshot `14a_rifle_hip`.
- `SMOKE rifle display text=%s`, then `after shot text=%s`, one round less, else Error. Then `reloading text=--`.
- During the reload: `SMOKE rifle magazine offset=%.1f`, more than 5 cm at mid reload, else Error. Screenshot `14b_rifle_reload`.
- A weak-point hit with the player as instigator: `SMOKE rifle weakpoint flash=%.2f` (more than 0.5), screenshot `14c_rifle_weakpoint`. Then `flash after=%.2f` (0), and `bodyshot flash=%.2f` (0).
- Enchant at half its duration (the pistol step's way): `SMOKE rifle half bar imbue=%.2f`, screenshot `14d_rifle_half_bar`.
- One shot, then on the same frame or the next: `SMOKE rifle muzzle flash visible=%d kick=%.2f` (both must show the effect), screenshot `14e_rifle_fire`.
- One equivalent line for the pistol: `SMOKE pistol muzzle flash visible=%d kick=%.2f`.

## Verification (all must be true when you finish)
1. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1` -> `Result: Succeeded`.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound` -> `Result: Succeeded`.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1 -MinTests 36` passes.
4. `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1` ends with `Smoke: done=True crashed=False LogGB errors=0 timedOut=False`. It must have:
   - `SMOKE path check passed`;
   - every pistol line as before (`meshes=6`);
   - all the new rifle lines;
   - no `LogGB` warning.

Then open the screenshots `14a`..`14e` and `13a`, and look at them.
- Is the rifle well framed and not clipped?
- Is the number on the rear display readable, upright and not mirrored?
- Do the lenses glow in `14c`?
- In `14d`, are the lit strip cells at the stock end and the dark ones at the muzzle end?
- Is the muzzle flash at the muzzle in `14e`?
- Does the pistol in `13a` look exactly as before?

Iterate the rifle scale and offset until it looks right. Never make an error disappear by changing the expected numbers or deleting behaviour: if a check fails, fix the code.

## Final report (plain text)
1. The four result lines verbatim, plus every new SMOKE line.
2. What you changed, with file names, and the final rifle scale and offset.
3. What you saw in each screenshot (paths).
4. Anything you could not do as described, and why.
5. What a person should check by playing.
