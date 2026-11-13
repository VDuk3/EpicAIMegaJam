# Task: the shotgun in the game: its 3D model, a pump after each shot, and a shell-by-shell reload

Project: **Glyphbound** (Unreal Engine 5.9.0, C++), in `Glyphbound/`. Read `Design/ARCHITECTURE.md` and `Prompts/implementer_rules.md` first. You DO build and run things in this task; the rest of "Hard rules" applies. Constraints:
- One build or run at a time. Other chats use this machine: if a build says another one holds the mutex, wait for it. If a build fails in files that are not yours, wait a few minutes and try again; if it still fails, stop and say so.
- Do not open the Unreal Editor and do not launch PIE. If a build fails with "Unable to build while Live Coding is active", someone has the Editor open: stop and say so in your report.
- Do not commit. Do not run `Tools/Jam/Import-*.ps1`, and do not touch `Content/` except `Content/Data/*.json`.
- Another chat has uncommitted work in this tree (the Entity: `AI/GBEntity*`, `AI/GBTentacle*`, `Combat/GBCombatLibrary.*`, `Combat/GBProjectile.*`, `Tests/GBEntityTests.cpp`, and **its own blocks inside `Core/GBDevCommands.cpp`**). Touch only the files listed under "Files you own"; in `GBDevCommands.cpp` add your shotgun block and change nothing else.
- The pistol and the rifle are approved by the owner: what they do and how they look must not change. Every existing smoke line for them must stay as it is.

Owner's decisions (Spanish, translated; `Design/GDD.md` v0.33, section 18 "Reload"):
- The shotgun gets the same treatment as the pistol and the rifle: a real 3D first-person model, complete and working.
- Reload: "Cartucho a cartucho" ("shell by shell").
- Pump: "Sí, con corredera" ("yes, with a pump"): the forend pumps after every shot.

## What exists today
- `Weapons/GBGun.h/.cpp`: the pistol and the rifle are built by one code path from a per-model art description (`FGBGunModelArt`, constants `PistolArt` and `RifleArt`, and the list in `FindModelArt`). `Definition.ViewModel` in `Content/Data/Weapons.json` picks the model. The shotgun (`"Id": "Shotgun"`) has no `ViewModel`, so it shows placeholder shapes.
- The reload is a magazine swap: `Reload()` starts a timer of `ReloadTime`, `FinishReload()` takes one spare magazine and fills the gun. `FGBAmmoState` (in `Weapons/GBHeldItem.h`) holds `InMagazine` and `SpareMagazines`.
- The reload animation (`GBReloadAnim`, `ApplyViewModelPose`, `ApplyReloadMagazine`, `SetReloadPreview`) is driven by `GetReloadAnimProgress()`; the HUD's 2D hands follow it (`UI/GBHUD.cpp`, `DrawHands`).
- The shotgun's art is imported: `/Game/Glyphbound/Weapons/Shotgun/` holds `SM_Shotgun_Body`, `SM_Shotgun_Trigger`, `SM_Shotgun_Magazine`, `SM_Shotgun_WeakPointLight`, `SM_Shotgun_AmmoDisplay` and a new part, `SM_Shotgun_Pump`. The materials take the same parameters as the rifle's (`ElementColor`, `ImbueRemaining`, `Flash`, `DisplayTexture`); the energy bar is inside the body's material (no separate mesh), 13 cells.
  - `SM_Shotgun_Magazine` is **one shell**, not a magazine. Its rest place is inside the loading port on the gun's underside, where the body hides it. It has a brass base at its rear end and a band that glows.
  - `SM_Shotgun_Pump` is a ribbed sleeve around the forend. It slides back along one vector and returns.
- Offsets, from `Glyphbound/SourceArt/Weapons/Shotgun/export_report.json`, in centimetres in the body's frame (X forward, Z up), same convention as the rifle's values:

| What | Value (cm) |
|---|---|
| Trigger hinge (`TriggerOffset`) | (5.8, 0, 0.6) |
| Shell at rest (`MagazineRest`) | (18.47, 0, -5.04) |
| Shell's way out of the port (`MagazineOut`) | (-5.0, 0, -1.81) |
| Pump at rest | (49.9, 0, -10.45) |
| Pump's slide to the back of its travel | (-4.11, 0, 0.88) |
| Muzzle tip (`MuzzleTip`) | (65.48, 0, -1.24) |
| Ammo display's centre (for reference; it is in the sight's rear window, a 2:1 pane 5 cm wide) | (18.85, 0.85, 20.45) |
| The gun is 101.2 cm long (the rifle is 100 cm and uses `ViewScale` 0.62) | |

## Part A: the model
- Add `ShotgunArt` and list it in `FindModelArt`; set `"ViewModel": "Shotgun"` on the shotgun in `Weapons.json`.
- The pump is a new optional part of a model: give `FGBGunModelArt` what it needs (whether the model has one, its rest offset, its slide vector). A model without a pump must behave exactly as today.
- Idle glow: the gun's own cyan, linear (0.10, 0.85, 1.00). `bDrawDisplayFrame` true (the pane is plain, like the rifle's): with 6 shells the display shows the count and 6 pips.
- Framing (`ViewScale`, `RootOffset`, `RootYaw`), kick and reload-pose values are yours to tune from the smoke screenshots. Start from the rifle's. Goals: the gun sits in the lower right like the rifle; the barrel's line runs to the crosshair; the sight's rear window with the ammo hologram and the energy bar just behind the sight (on top of the receiver) are both in view; nothing clips the near plane (3 cm) or covers the crosshair. This gun has a tall thumbhole stock that rises 15 cm above the grip: keep it from filling the lower right corner.
- The weak-point light is a lens on the sight's rear lower panel, under the window: it must flash on a head or weak-point hit like the other guns'.

## Part B: the pump
After every shot the pump slides back and returns, inside the time between shots. It is only the look: `FireInterval` and every gameplay timing stay as they are.
- Timing as shares of the gun's current `FireInterval` (so a faster fire rate plays it faster), in named constants next to the other viewmodel constants: provisional values start 0.22, fully back 0.45, home 0.72.
- While it moves, the gun gives a small rack motion through `ApplyViewModelPose` (the one place that writes the root's transform): a short dip and roll, per-model art values, provisional. It adds to the shot kick; it does not replace it.
- The HUD's left hand rides the pump: back and forward with it by a per-model amount in sprite pixels. The right hand stays as it is.
- No pump while a reload is running. A weapon switch resets it to rest at once.
- A dev-only pin like the reload's (`SetPumpPreview(float Amount)` / `ClearPumpPreview()`, or one shared mechanism) so the smoke can photograph the pump at the back of its travel.
- Accessors the smoke needs: whether the model has a pump, and how far the pump is from rest in cm.

## Part C: the shell-by-shell reload (gameplay)
`Design/GDD.md` section 18, "The shotgun loads shell by shell", is the specification. In short:
- A gun reloads per round when its data says so. Add a stat for it (name it in `Core/GBStatNames.h`, one line, e.g. `ReloadPerRound`; 1 on the shotgun in `Weapons.json`, absent = 0 on the others).
- **Reserve.** For such a gun `FGBAmmoState::SpareMagazines` counts spare **rounds** (write that in the comment on the field). Its maximum is the `SpareMagazines` stat times `MagazineSize` (4 x 6 = 24, the same total as before). `AddSpareMagazines(Count)` adds `Count` magazines' worth of rounds, clamped, and still returns how many magazines' worth it added, rounded up, so callers that test "did it add anything" keep working. `Weapons/GBEquipmentComponent.cpp` builds a full ammo state for a gun from its stats (around line 117): it must give 24, not 4, to a per-round gun. Find every reader of `SpareMagazines` (grep) and make sure none is left treating rounds as magazines.
- **Cycle.** One shell takes `ReloadTime / MagazineSize` (0.33 s; `ReloadTime` already carries the Rapid Reload perk). When a cycle ends one round moves from the reserve into the gun. If the gun is not full and the reserve is not empty, the next cycle starts at once; otherwise the reload ends. A full reload from empty therefore takes `ReloadTime`, as the balance tables assume. A frame longer than one cycle must not lose time or shells.
- **Stopping part-way.** Pressing fire during the reload stops it, and the gun shoots as soon as its normal fire rules allow. The shell on its way in is not lost: it stays in the reserve. With an empty gun, fire pressed during the reload waits for the first shell to land, then stops the reload and shoots. A weapon switch cancels the reload; shells already loaded stay loaded.
- Nothing is lost by reloading early (unlike the magazine guns). An empty shotgun still starts reloading by itself.
- `Reload()`, `FinishReload()` and the rest must behave exactly as today for the pistol and the rifle.
- `GetReloadProgress()` for a per-round reload: the whole reload's progress, 0 to 1 over the shells it set out to load.
- The ammo hologram shows the live count during a per-round reload (not `--`), and a pip lights as each shell goes in.
- HUD (`UI/GBHUD.cpp`, the ammo panel around line 980): for a per-round gun the reserve label reads `x%d shells`, not `x%d mags`. The HUD must not need to know the weapon's id: give `AGBHeldItem` a small virtual (default false) that the gun overrides.
- **Pure logic, with tests.** Put the stepping of a per-round reload (time in, rounds moved, when it ends) in a small plain struct or free functions with no world and no actor, and test it in `Tests/GBWeaponsTests.cpp` (rules in `implementer_rules.md`; a test name must not be a prefix of another). Tests that can each fail:
  - a full reload from empty moves exactly `MagazineSize` rounds and ends at `ReloadTime`, not a cycle early or late;
  - a reserve smaller than the empty space stops when the reserve is gone, with nothing negative;
  - one step longer than three cycles moves three rounds and keeps the remainder of the time;
  - stopping part-way keeps the rounds already moved and puts none back.

## Part D: the shell-by-shell reload (look)
The same pose mechanism as the magazine guns, but the middle repeats once per shell.
- The gun leaves rest for its reload pose during the first shell's first share, holds it while shells go in, and eases back to rest after the last shell (that last ease may run a little past the gameplay end of the reload, like a cancelled reload's blend; the gun must be able to fire when gameplay says so).
- The reload pose must show the loading port: it is on the underside, in front of the trigger guard, so the gun rolls and rises until its underside faces the camera.
- Per shell: the shell (the `Magazine` part) appears below the view, rises to the end of the `MagazineOut` path, slides in along it to rest, fastest at the end, and a short bump seats it. Then it is hidden until the next one. At rest, outside a reload, the shell is hidden (it is inside the port).
- The left hand follows each shell: down to fetch, up with it, the push. The right hand follows the gun as it does today.
- A stopped or cancelled reload must not snap: shell hidden at once, gun blends to rest in about 0.12 s, as today.
- The dev pin must be able to hold one shell's cycle still at a given progress for the smoke.
- Keep it one code path where that is natural: the per-shell cycle should reuse the existing beat helpers rather than copy them.

## Files you own
- `Glyphbound/Source/Glyphbound/Weapons/GBGun.h`, `GBGun.cpp`
- `Glyphbound/Source/Glyphbound/Weapons/GBHeldItem.h` (the comment on `SpareMagazines`, the new virtual)
- `Glyphbound/Source/Glyphbound/Weapons/GBEquipmentComponent.cpp`: only the full ammo state of a gun
- `Glyphbound/Source/Glyphbound/Core/GBStatNames.h`: one new stat name
- `Glyphbound/Source/Glyphbound/UI/GBHUD.cpp`: `DrawHands` and the reserve label of the ammo panel only
- `Glyphbound/Source/Glyphbound/Tests/GBWeaponsTests.cpp`: your new tests
- `Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp`: only a new shotgun block in `gb.Smoke`
- `Glyphbound/Content/Data/Weapons.json`: only the shotgun entry
- If another file reads `SpareMagazines` in a way that breaks for rounds (`Items/GBInventoryComponent.cpp`, `World/GBPickup.cpp`), say what you found; change it only if the fix is one or two lines, and list it.

## The smoke run
`gb.Smoke` has a block per gun in `Core/GBDevCommands.cpp` (search for `"rifle:"`). Add a shotgun block after the rifle's, in the same style, with screenshots `15a` onwards. Checks, each able to fail:
- the shotgun is in hand with its real model: 6 meshes (body, trigger, shell, weak-point light, ammo display, pump), not placeholders; the display shows `6`; screenshot `15a_shotgun_hip`;
- one shot: the display shows `5`, the kick and the muzzle flash happen, and the pump leaves rest and comes back to rest before the next shot is allowed; with the pump pinned at the back of its travel, screenshot `15b_shotgun_pump`;
- weak-point flash on a head hit, 0 on a body hit; the energy bar at half an enchant; screenshot `15c_shotgun_enchanted`;
- reload from 3 shells with 24 in reserve: after one cycle 4 and 23, the display follows; it ends at 6 and 21 after about half of `ReloadTime`; with a shell's cycle pinned mid-way (the shell visible and away from rest, the root away from rest), screenshot `15d_shotgun_reload_shell`;
- fire pressed during a reload from 2 shells: the reload stops, the shot is fired, and the counts add up (nothing lost, nothing created);
- a reload cancelled by switching to the staff and back keeps the shells already loaded;
- after all of it the pump and the shell are at rest and the root is at rest.
Every other smoke line must stay as it is.

## How to verify (from `C:\GameJam`)
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Build-Jam.ps1`: must end in `Result: Succeeded`.
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Test-Jam.ps1` with the anchored filter of the weapons tests (read the script's header for its parameters; filters look like `^Glyphbound.Weapons.`): all green.
- `powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Smoke-Jam.ps1`: must end in `done=True crashed=False LogGB errors=0`.
- Open your four screenshots in `Glyphbound/Saved/Screenshots/Smoke/` (the PNG files; enlarge the gun's area if you need to) and tune the framing and the poses until: the gun reads like the rifle's framing, the hologram and the energy bar are visible in `15a`, the pump is clearly back in `15b`, and `15d` shows the underside with the port and a shell on its way in. The first screenshot after a material change can come out tan and grainy (shaders compiling): run the smoke again before judging it.
- Do not package.

## Final report
The usual five points of `implementer_rules.md`, plus: the art values you ended with for the shotgun; what each of the four screenshots shows; every reader of `SpareMagazines` you found and what you did about each; the test names; and anything you could not judge from still images (the pump's and the reload's timing and feel need a person playing).
