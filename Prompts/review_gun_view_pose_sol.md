# Review: first-person gun pose (read-only)

You review an uncommitted change in this Unreal Engine 5.9 C++ game (Glyphbound, a jam game). The task the implementer
was given is `Prompts/gun_view_pose.md`; read it first. Then read the changed code in the working tree (use `git diff`
on these paths): `Glyphbound/Source/Glyphbound/Weapons/GBGun.h/.cpp`, `Player/GBPlayerCharacter.h/.cpp`, `UI/GBHUD.h/.cpp`,
`Tests/GBWeaponsTests.cpp`, `Content/Data/Weapons.json`, `Content/Data/Game.json`, `Config/DefaultEngine.ini`, and in
`Core/GBDevCommands.cpp` ONLY the new `gb.ViewPose`, `gb.ViewFreeze`, `gb.HandOverlay` commands and the `ViewPose*`
smoke steps (other hunks in that file belong to someone else: ignore them).

Engine sources are at `C:\Program Files\Epic Games\UE_5.9\Engine\Source`.

## Check especially
1. The implementer added `r.UseLegacyMaintainYFOVViewMatrix=1` and claims that without it, in 5.9, Maintain Y-Axis FOV
   treats the FOV value as horizontal at the camera's fixed AspectRatio (`CameraStackTypes.cpp` around 326-340). Verify
   against the engine source: is that true, is the cvar a real, non-deprecated one that works in a packaged Development
   and Shipping build from `[ConsoleVariables]` in DefaultEngine.ini, and do the converted values (hip 58.72, aim 43.0 /
   46.7 / 36.0, ViewModelFOV 58.72) then give the same 16:9 image as the old 90 / 70 / 75 / 60 with Maintain X?
2. First-person FOV: does the gun really stay put on screen while aiming (FirstPersonFieldOfView fixed, world FOV
   changing)? Is any viewmodel primitive left without `FirstPersonPrimitiveType` (parts spawned later, muzzle flash,
   placeholder parts, re-equip)?
3. The bob: the hands and the 3D gun must get the same rounded art-pixel shift; check the cm conversion uses the right
   depth, FOV and pixel size, and that it is right at any screen height and aspect.
4. Pose from data: read raw (not through perk modifiers), falls back to constants, live override cleared on re-equip.
5. `gb.HandOverlay`: file loading in a packaged build, texture lifetime (GC), nearest filter, scale and placement as the
   brief says.
6. Anything that crashes, leaks, or breaks the old reload/pump/kick hand alignment.

Report only real defects, most severe first, each with file:line, what goes wrong in a concrete case, and the fix. Say
plainly if you find none. Answer in English.
