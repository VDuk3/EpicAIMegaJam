# Task: a reload animation for the pistol and the rifle

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first. You DO build and run things in this task; the rest of "Hard rules" applies. Constraints:
- One build or run at a time. Other chats use this machine: if a build says another one holds the mutex, wait for it.
- Do not open the Unreal Editor and do not launch PIE. If a build fails with "Unable to build while Live Coding is active", someone has the Editor open: stop and say so in your report.
- Do not commit. Do not run `Tools/Jam/Import-*.ps1`, and do not touch `Content/` except `Content/Data/*.json`.
- Other chats may be editing other files in this working tree. Touch only the files listed below; leave any other change you see alone.

Owner (Spanish, translated): "Add the reload animation to the pistol and to the rifle."

## What exists today
- `Weapons/GBGun.h/.cpp`: both guns are real 3D viewmodels built by one code path from a per-model art description (`FGBGunModelArt`, constants `PistolArt` and `RifleArt` in `GBGun.cpp`). A third model may be added by another chat; keep the table easy to extend.
- A reload lasts `ReloadTime` (stat; pistol 1.2 s, rifle 1.8 s, less with the Rapid Reload perk). `GetReloadProgress()` runs 0 to 1. The rounds are added at the end (`FinishReload`). `CancelReload()` ends it early (weapon switch, `SetAmmo`).
- The only motion during a reload is in `UpdateViewModel`: the magazine slides out along `Art.MagazineOut` in the first third, stays out, and slides back in the last third. The gun itself does not move, so it reads as a part sliding on a frozen model.
- `ApplyKickPose()` owns the root's pose: `Art.RootOffset`, `Art.RootYaw`, plus the shot kick.
- The hands are 2D pixel sprites drawn by the HUD, not part of the model: `UI/GBHUD.cpp`, `DrawHands`, the `EGBHeldSlot::Gun` branch. They do not react to a reload at all.
- The ammo hologram shows `--` and dims its pips while reloading. Leave that as it is.

## What to build
One reload animation for every real gun model, driven only by `GetReloadProgress()`, so a shorter reload plays the same motion faster. The rounds still arrive at the end of the reload: do not change any gameplay timing.

Beats, as shares of the reload. The shares and every angle and distance are provisional art values: put them in `FGBGunModelArt` (per model) or in named constants next to the other viewmodel constants, never inline.

| Share | Gun (root) | Magazine | Left hand (HUD) |
|---|---|---|---|
| 0.00 - 0.15 | Leaves its rest pose for the reload pose: canted and raised so the magazine well and the magazine face the camera, drawn a little towards the screen centre. Ease in and out. | In the gun. | Leaves its place towards the magazine. |
| 0.15 - 0.40 | Holds the reload pose, with a small dip as the magazine leaves. | Slides out along `MagazineOut`, then falls straight down **on screen** (camera down, whatever the gun's tilt), speeding up, until it is below the bottom of the view; then it is hidden. | Goes down with the magazine and off the bottom of the screen. |
| 0.40 - 0.50 | Holds. The gun is empty: the well is visible with no magazine. | Hidden. | Off screen. |
| 0.50 - 0.80 | Holds. | A fresh magazine (the same component, shown again) rises from below the view to the end of the slide path, then slides in, fastest at the end. | Comes up with it. |
| 0.80 - 0.86 | The magazine seats: a short bump (the gun jumps up a few millimetres and settles). | At rest. | Pushes up with the bump. |
| 0.86 - 1.00 | Eases back to the rest pose. | At rest. | Back in its place. |

Rules:
- The reload pose is per model. Give `FGBGunModelArt` what it needs (for example a reload offset in cm, and pitch, yaw and roll in degrees) and tune the values for each gun from the screenshots. The goal for both: during the swap the player clearly sees the magazine leave and a new one go in, and no part of the gun clips the near plane (3 cm) or covers the crosshair. The rifle is long: keep its stock from swinging through the camera.
- One function owns the root's pose: rest pose + reload pose + shot kick. Today that is `ApplyKickPose`; rename or extend it, but there must not be two places writing the root's transform.
- The right hand stays on the grip: move it with the gun so it does not look detached (a few sprite pixels, following the root's screen motion). The left hand follows the table. Hand offsets are layout pixels; use the existing `P` unit and the `GBHudLayout` constants style. The HUD reads the reload from `AGBHeldItem::IsReloading()` and `GetReloadProgress()`, which already exist; if it needs more, add a small public accessor on `AGBGun` and say so in the report.
- A cancelled reload (`CancelReload`) must not snap: the magazine goes back to rest and visible at once, and the gun blends from wherever it is back to the rest pose in about 0.12 s.
- A gun with placeholder shapes (no real model) keeps today's behaviour: nothing moves.
- The shot kick, the muzzle flash, the trigger pull, the glow, the energy bar and the ammo hologram must behave exactly as they do now.
- `GetViewModelMagazineOffsetCm()` keeps its meaning (distance of the magazine from rest, in the body's frame). Add what the smoke needs to check the new motion, for example whether the magazine is visible and how far the root is from its rest pose.

## Files you own
- `Glyphbound/Source/Glyphbound/Weapons/GBGun.h`, `GBGun.cpp`
- `Glyphbound/Source/Glyphbound/UI/GBHUD.cpp`: `DrawHands` only (and `UI/GBHudLayout` constants if that is where they live)
- `Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp`: only the pistol and rifle reload steps of `gb.Smoke`

## The smoke run
`gb.Smoke` has one reload step per gun today ("magazine out halfway": it checks the magazine offset at about half the reload and takes `13b_pistol_reload` / `14b_rifle_reload`). The new motion breaks those checks: replace them.
- Screenshots are captured a few frames after the request, and the run leaves a full second after each one. A reload lasts 1.2 to 1.8 s, so it cannot be photographed three times as it plays. Add a dev-only way to hold a reload still at a given progress (for example a public `SetReloadPreview(float Progress)` / `ClearReloadPreview()` on `AGBGun` that pins the animation's progress without touching the ammo), use it only from the smoke, and take for each gun:
  - `13b_pistol_reload_out` / `14b_rifle_reload_out`: about 0.30, the old magazine leaving;
  - `13f_pistol_reload_empty` / `14h_rifle_reload_empty`: about 0.45, no magazine;
  - `13g_pistol_reload_in` / `14i_rifle_reload_in`: about 0.70, the new magazine going in.
  - Keep `13b_...` and `14b_...` as prefixes of the first one so old references still sort next to them, or rename; say which in the report.
- Checks, each one able to fail:
  - at 0.30 the magazine is away from rest and visible; at 0.45 it is hidden; at 0.70 it is visible and away from rest;
  - at each of those the root is away from its rest pose;
  - after a real, unpinned reload runs to its end: magazine at rest and visible, root at rest, the display shows the full count again, one spare magazine fewer;
  - a reload cancelled halfway (switch to the staff and back) leaves the magazine at rest and visible and the ammo unchanged.
  - Keep the existing `--` display check.
- All other smoke lines must stay as they are.

## How to verify (from `C:\GameJam`)
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1`: must end in `Result: Succeeded`.
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1`: must end in `done=True crashed=False LogGB errors=0`.
- Look at your six reload screenshots in `Glyphbound/Saved/Screenshots/Smoke/` (open the PNG files; enlarge the gun's area if you need to). Tune the pose values until the three beats read clearly on both guns. Screenshot `14a` is tan and grainy on the first run after a material change; that is not yours to fix.
- Do not package.

## Final report
The usual five points of `implementer_rules.md`, plus: the reload pose values you ended with for each gun, what each of the six screenshots shows, and anything about the motion you could not judge from still images (timing and feel need a person playing).
