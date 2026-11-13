# Code review: shotgun pump rack and reload pose, Glyphbound (UE 5.9, C++)

You are reviewing, read-only, a small change to the shotgun's first-person viewmodel. Run `git diff HEAD --
Glyphbound/Source/Glyphbound/Weapons/GBGun.cpp Glyphbound/Source/Glyphbound/Core/GBDevCommands.cpp` and read the
surrounding code in `GBGun.cpp` (struct `FGBGunModelArt`, `ApplyPump`, `ApplyViewModelPose`, `ApplyReloadMagazine`,
the Tick part with the visual timers) and `UI/GBHUD.cpp` (the gun branch of the hands drawing, around
`GetPumpHandShift`). Ignore other modified files (Devlog raw logs).

What changed:
1. The pump's "rack" motion of the whole gun was a dip (float cm) and a roll (float deg). It is now a full offset
   (`FVector PumpRackOffset`, camera frame) and rotation (`FRotator PumpRackRot`) scaled by the pump amount, so the
   gun can be pulled in, tip its muzzle up, turn and roll to show the forend. Only the shotgun sets them (the pistol
   and rifle initializers stop before the pump fields and must stay at zero).
2. New art values for the shotgun: rack offset/rotation, left-hand travel with the pump, reload pose offset and
   rotation (roll 130 degrees so the loading port on the underside faces the camera), left-hand reach and right-hand
   follow during the reload.
3. The pump timer now advances by the clamped `EffectStep` (max 1/30 s per frame) instead of the raw `DeltaSeconds`,
   like the kick and muzzle-flash timers, so a hitch right after a shot cannot skip the whole rack.
4. Smoke script (`gb.Smoke`, shotgun block): the shot step moved from SG+0.9 to SG+1.1 s, the mid-pump check from
   +1.27 to +1.47, the "home again" check from +1.85 to +1.8, so the shot no longer lands in the frame that pays for
   the previous step's screenshot (a ~0.5 s hitch that skipped the pump in a filtered run).

## What to look for (ranked)
1. Real bugs: wrong composition of the rack with the kick and reload pose (`Root->SetRelativeLocation/Rotation`),
   the rack or the reload pose not returning exactly to rest (the smoke checks the root is within 0.05 cm and 0.5 deg
   of rest after the pump, after a reload and after a cancel), a pistol or rifle that now gets a non-zero rack, a
   reload magazine drop direction that breaks with a 130 degree roll (`ApplyReloadMagazine` unrotates "camera down"
   through the root rotation), FRotator scaling (`FRotator * float`) or wrap-around issues at large angles.
2. Timing: with the clamped step, can the pump still be moving (or stuck back) when the next shot is allowed, or
   when a reload begins, a weapon switch happens, or the game is paused? Is anything else reading `PumpElapsed` with
   the assumption that it tracks real time?
3. Smoke step timing: after the move, are the step comments and checks still true (the "home again before the next
   shot is allowed" step at 0.7 s after the shot, with FireInterval 0.8 and the pump home at 0.72 of it), and does
   anything later in the block depend on the shot's old time?

Do not suggest style rewrites, renames or new features, and do not judge the art values themselves (they were
tuned from screenshots). For each finding give: file:line, what goes wrong, a concrete scenario, and a fix. If you
find nothing real, say so plainly.
