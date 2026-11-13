# Block: the shotgun's pump and reload made readable (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## Where it started
The shotgun chat (thirty-third block, Nov 12 entry) ended with a list of weak points. The owner pasted that list into a new chat. Of its four items, two were work for the architect:
- **The pump** was under the gun, out of sight of the first-person camera. Its "rack" was a 0.5 cm dip and a 1.5 degree roll, which nobody could see.
- **The reload pose** showed the gun's side, not its underside, so the loading port was barely in view and the shell looked tiny. It still used the rifle's starting values.

The other two stay as they were. The energy bar on the receiver is the owner's decision. Only a person playing can judge the feel.

## What changed (`Weapons/GBGun.cpp`, `Core/GBDevCommands.cpp`)
- **The rack is now a full motion.** The two floats became an offset (`FVector PumpRackOffset`, camera frame) and a rotation (`FRotator PumpRackRot`), both scaled by how far back the pump is. While the pump is back, the gun is pulled in 2 cm, tips its muzzle up 7 degrees, turns 10 degrees to the left and rolls 12 degrees. That shows the forend with its glyph to the camera. The 2D left hand now moves down with the pull (art pixels `(-4, 6)`, before `(-6, 1)`). The pistol and the rifle leave these fields at zero.
- **Reload pose.** The gun rolls 130 degrees, which turns it almost over, and lifts its muzzle 15 degrees. The loading port on the underside, with its glowing ring and the shell going in, now faces the camera near the middle of the screen. The left hand reaches up under the port (`(12, -24)`, before `(0, -2)`).
- **Smoke timing (a flaky check fixed).** The first filtered run failed `shotgun pump: slid back during the interval` with the pump at 0.00 cm. This was not caused by the new values. The screenshot taken just before the shot stalled one frame for about 0.5 s. The shot was fired in that same frame, so the gun's next tick got the whole stalled time at once and the pump went back and home inside it. The shot step moved from 0.9 s to 1.1 s into the block, so it no longer shares that frame. The mid-pump check moved with it (0.37 s after the shot, as before). The "home again" check now runs 0.7 s after the shot: the pump is home at 0.576 s, and the next shot is allowed at 0.8 s.

## How the pose was found: eight build-and-screenshot rounds
Each round compiled the Editor target (13 to 43 s), ran `Smoke-Jam.ps1 -Only shotgun -Offscreen` (about 65 s) and cropped the gun's area. Each run held the shared `Glyphbound/Saved/machine.lock`. Round by round:
1. Roll 80, gun pulled towards the camera: huge on screen, still the side (`shotgun_pose_reload_try_it1.png`).
2. Roll 115, muzzle up 20: the trigger guard shows, but the gun covers the left half of the screen (`try_it2`).
3. Roll 105, pushed forward, turned left: the underside faces left, away from the camera, which sits to the gun's right (`try_it3`).
4. The geometry, worked out: the camera is behind, above and to the left of the gun. For the port to face it, the underside has to point back, up and left. That calls for a roll of about 130 degrees plus some muzzle-up pitch. Roll 130 with pitch 28 got the port into view, but the gun stood almost vertical (`try_it4`).
5. Pitch 15: the gun lies diagonally, the port and shell sit near the centre (`try_it5`), kept.
6. to 8. Hands: the right hand first slid onto the left one; the final values keep the right hand on the receiver and put the left hand under the port.

## Review: GPT Sol 6.1 (Codex CLI, medium, read-only, logged through `Invoke-LoggedAgent.ps1`)
Prompt: `Prompts/shotgun_poses_review_sol.md`. **One real finding, fixed.** The first version also made the pump timer advance by the clamped visual step (at most 1/30 s per frame, like the kick and the muzzle flash), so that a stall could not skip the rack. Sol showed what that breaks: at 15 fps the pump needs about 1.15 s to come home, but the next shot is allowed after 0.8 s, so the pump would snap back when the player fires again. The pump timer is back on real time. A comment in the code says why. Sol found nothing else: the pose returns to rest, the pistol and the rifle get no rack, and the falling shell still drops straight down on screen at a 130 degree roll.

## Checks
- Full smoke, once, at the end: `done=True crashed=False LogGB errors=0`, 58 screenshots, 142 s. Then, after Sol's fix, the shotgun's filtered smoke: pump 4.15 cm from rest at 0.37 s after the shot, everything back at rest after it, 0 errors.
- Tests: 44 run, 0 failed (1.00 s of test time, 27 s of wall time).
- Game target build: succeeded (41 s; 26 s again after the fix).
- **Not packaged in this block.** Nobody has played it yet.

## Implementer
The architect made these edits directly, not Muse Spark. It was a handful of art constants and a two-field struct change, tuned by looking at screenshots one round at a time. Handing each round to an implementer would have cost more than the change itself.

## Screenshots (`media/2026-11-13/`, the game's smoke run, editor build, gun area enlarged 1.4 times)
- `shotgun_pose_pump_before.png` / `shotgun_pose_pump_after.png`: the pump pinned at the back of its travel, before (Nov 12 run, old HUD) and after. The forend now turns towards the camera.
- `shotgun_pose_reload_before.png` / `shotgun_pose_reload_after.png`: a shell's cycle pinned mid-way, before and after. `shotgun_pose_reload_after_full.png`: the whole screen after.
- `shotgun_pose_reload_try_it1.png` to `try_it5.png` (half size): the rounds of the search above.

## Needs a person playing
Whether the rack's turn feels like a pump or like the gun waving, how the 130 degree turn plays when it repeats for every shell (it holds the pose between shells), and the hands.
