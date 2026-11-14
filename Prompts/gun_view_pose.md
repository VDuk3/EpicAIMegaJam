# Task: first-person gun pose in data, fixed weapon FOV, one shared motion for the 3D gun and the 2D hands

Read first: `Prompts/implementer_rules.md` (with the "Machine rules" below instead of its "do not build"),
`Weapons/GBGun.h/.cpp` (FGBGunModelArt, ApplyViewModelPose, UpdateAimFOV), `Weapons/GBHeldItem.h/.cpp`,
`Weapons/GBEquipmentComponent.cpp` (where the item is attached), `Player/GBPlayerCharacter.cpp` (Camera, HeldItemAnchor,
HeldItemOffset), `UI/GBHUD.cpp` (the first-person hands block, around lines 500-660, and the crosshair around 780),
`UI/GBPixelSpriteLibrary.*`, `Content/Data/Weapons.json`, `Content/Data/Game.json`, `Config/DefaultEngine.ini`,
and the request this answers: `Incoming/LAPTOP/_para_LAPTOP/20261114-1700_pov-armas.md` (Spanish; what we promised).

## Why
The LAPTOP artist will draw pixel-art hands that must grip the 3D guns. Today:
- the gun's pose is hard-coded per model in `GBGun.cpp` (`ViewScale`, `RootOffset`, `RootYaw`);
- aiming lowers the camera FOV, so the 3D gun moves on screen while the 2D hands stay put;
- the walk bob moves only the 2D hands, never the gun;
- the hands are scaled by screen height but placed by a share of the width, while the gun follows the horizontal FOV:
  on a non-16:9 screen they drift apart.
The artist will tune the pose inside our packaged Development build through console commands and send numbers back.

## What to build
1. **Pose in data.** Each gun in `Weapons.json` gets, in its `Stats`: `ViewOffsetX`, `ViewOffsetY`, `ViewOffsetZ` (cm,
   relative to `HeldItemAnchor`, camera frame), `ViewPitch`, `ViewYaw`, `ViewRoll` (degrees), `ViewScale`. Fill them with
   today's values (pistol 0,0,0 / 0,-6,0 / 0.30; rifle -12,-6,2 / 0,-0.4,0 / 0.62; shotgun -6,-5,-1.5 / 0,-0.4,0 / 0.55).
   The gun reads them raw from its definition at equip (NOT through perks/stat modifiers); the FGBGunModelArt constants
   stay only as a fallback when a key is missing. `ApplyViewModelPose` stays the one place that writes the root: rest
   pose from data + reload pose + kick + pump rack + the shared bob (point 4). Every other per-model value (magazine,
   muzzle, reload offsets, hand reach) stays in the constants.
2. **Fixed weapon FOV.** On the player camera: `bEnableFirstPersonFieldOfView = true`, `FirstPersonFieldOfView` from
   `Game.json` key `ViewModelFOV`. Every primitive of a gun viewmodel (body, parts, muzzle flash, placeholder parts too)
   gets `FirstPersonPrimitiveType = FirstPerson`. Aiming keeps changing the world camera FOV only. Check the exact names,
   and anything else the feature needs to render (a project or renderer setting, shadows), in the UE 5.9 sources
   (`Engine/Source/Runtime/Engine/Classes/Camera/CameraComponent.h`, `Components/PrimitiveComponent.h`) before using them.
   The staff and the sword are 2D sprites: untouched.
3. **Maintain Y-Axis FOV.** `AspectRatioAxisConstraint=AspectRatio_MaintainYFOV` in `DefaultEngine.ini` (find the right
   section in the engine's `BaseEngine.ini`). With it the FOV value becomes the vertical one, so convert every FOV so
   16:9 looks exactly as today: hip 90 -> 58.72, AimFOV pistol 70 -> 43.0, shotgun 75 -> 46.7, rifle 60 -> 36.0,
   `ViewModelFOV` = 58.72 (formula `2*atan(tan(h/2)*9/16)`). Find where the hip FOV comes from (the camera default or
   data) and put it in `Game.json` as `HipFOV` if it is not in data. Fix every FOV-to-pixels sum that assumes horizontal
   (at least the crosshair spread in `GBHUD.cpp`: with a vertical FOV it must use `ClipY`); grep for others
   (`GetFOVAngle`, `FieldOfView`, `FOVAngle`) in the game module and the GBPixelVfx plugin, fix the game ones.
4. **One shared motion.** One pure function (no world, testable) gives the view bob in ART pixels from time and the
   ground speed ratio. The 2D hands use it as today. The 3D gun gets the SAME shift, converted to cm in the camera plane
   at the gun's depth with the weapon FOV (`cm = artPx * PixelSize * 2 * Depth * tan(ViewModelFOV/2) / ClipY`, with
   PixelSize the screen pixels per art pixel and `Depth` the root's forward distance from the camera), so gun and hands
   move together. Round the hands' shift to whole art pixels; the gun follows the rounded value.
5. **Hands layer (for the LAPTOP art).** A full-screen hand overlay: a 480x270 PNG drawn with nearest filtering, scaled to
   the screen height (`ClipY / 270` screen px per file px, rounded to an integer, min 1), centred horizontally,
   bottom-aligned, offset by the same bob, kick, reload follow and pump shift the current hands get (in art pixels times
   the overlay's pixel size). Nothing in the shipped data uses it yet; it is for the dev command below.
6. **Dev console commands** (in `Core/GBDevCommands.cpp`, same style as the others; they must work in a packaged
   Development build):
   - `gb.ViewPose` without arguments logs the held gun's pose as the exact JSON lines to paste into `Weapons.json`.
   - `gb.ViewPose X Y Z Pitch Yaw Roll Scale` sets it live on the held gun (until re-equip).
   - `gb.ViewFreeze 0|1`: freezes bob and sway (gun and hands) for clean template screenshots.
   - `gb.HandOverlay <absolute path to PNG>` loads a PNG from disk (`FImageUtils::ImportFileAsTexture2D` or the 5.9
     equivalent) and draws it as the hands layer over the held gun; no argument clears it. On a bad file it logs why.
7. **Tests** (`Tests/GBWeaponsTests.cpp`, anchored names under `Glyphbound.Weapons.ViewPose.`; none may also be the folder
   of another test): the bob function (zero when still, bounded, periodic), the art-px-to-cm conversion (a shift of N art
   px projects back to N art px on screen at any depth and screen height), and the FOV conversion (90 at 16:9 -> 58.72).
   Each must be able to fail for a wrong implementation.
8. **Smoke**: add a step whose name starts with `ViewPose` that equips each gun, checks that the root sits at the pose
   from data, that the gun's screen position does not change between hip and full aim, and that `gb.HandOverlay` with a
   bad path logs an error and draws nothing. Then take one screenshot per gun (hip) to `Saved/Screenshots/viewpose_*`.

## Machine rules
- Other chats work on this machine at the same time: the castle (maps, packaging) and the enemies (`AI/*`, `Enemies.json`,
  the `Enemies3D` blocks of `GBDevCommands.cpp`). You own `Weapons/*`, the hands/crosshair parts of `UI/GBHUD.cpp`,
  the camera lines of `Player/GBPlayerCharacter.*`, your new blocks of `GBDevCommands.cpp`, `Tests/GBWeaponsTests.cpp`,
  the gun entries of `Weapons.json`, your keys in `Game.json` and the FOV line of `DefaultEngine.ini`. Small targeted
  edits in shared files; never reformat.
- You build and check yourself, one build or game run at a time on this machine: before every build, test or smoke run,
  wait while `C:\GameJam\Glyphbound\Saved\machine.lock` exists and is younger than 45 minutes or an `UnrealEditor.exe`,
  `UnrealBuildTool` or `cl.exe` process is running (check every 60 s; keep writing meanwhile); then write the lock
  (`Muse-viewpose | <what> | <UTC time>`), run, and delete it.
- If the build fails in files you do not own, do not touch them: wait two minutes and build again.
- Build: `powershell -File Tools/Jam/Build-Jam.ps1` (Editor target only). Tests:
  `powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.Weapons.'`. Smoke:
  `powershell -File Tools/Jam/Smoke-Jam.ps1 -Only ViewPose -Offscreen`.
- Iterate cheaply: only these filtered checks; never the full smoke, the full suite, the game target, the packager, PIE
  or the Editor. Do not commit. Do not launch other agents. English, ASCII, no gameplay numbers in code (data keys).
- UE 5.9: check engine APIs in `C:\Program Files\Epic Games\UE_5.9\Engine\Source` before using them.

## Final report
Files changed; keys added with values; the exact engine API names used for the first-person FOV; what each test and smoke
check catches; the exact last results of build, tests and the filtered smoke; the screenshot paths; what you did not do
or could not verify.
