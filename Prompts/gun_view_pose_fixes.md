# Task: fixes to the first-person gun pose (review round 1)

Your earlier task is `Prompts/gun_view_pose.md`; its change is still uncommitted in the working tree (read `git diff` on
the files it lists). The same "Machine rules" apply, with the lock name `Muse-viewpose-fix`. Fix these eight defects,
nothing else.

**One change of plan first:** the hand overlay is no longer for the LAPTOP. The hands are now drawn on CENTRAL, at 3x:
one art pixel = 3 file pixels of the 480x270 PNG = `ClipY / 90` screen pixels (12 at 1080p). Fix the comments that say
"LAPTOP art".

1. **One motion grid (architect).** The gun moves by `bob * ClipY/120` screen pixels (`GBGun.cpp` ~963), the overlay by
   `motion * ClipY/270` (`GBHUD.cpp` DrawHandOverlay): the hands slide 2.25x less than the gun. Make ONE pure function in
   `GBViewBob` that turns a motion in old hand art pixels (bob, kick, reload follow, seat push, pump shift: the values
   are tuned in units of `ClipY/120`) into a **screen-pixel** shift: multiply by `ClipY/120`, then, while a hand overlay
   is shown, round to whole overlay art pixels (multiples of `3 * round(ClipY/270)`), else to whole `ClipY/120`
   pixels. The HUD (old hands and overlay) and the gun (bob only, as now) both use that screen shift; the gun converts
   it to cm with `cm = screenPx * 2 * Depth * tan(ViewModelFOV/2) / ClipY`. Test it under `Glyphbound.Weapons.ViewPose.`:
   the result is always a multiple of the active grid, and a gun shift projects back to exactly the hands' shift.
2. **Live scale.** `gb.ViewPose ... Scale` must resize `VMBody` and move the muzzle flash; apply the current rest-pose
   scale wherever the pose is applied, not only at construction (`GBGun.cpp` ~620).
3. **Override cleared on re-equip.** Switching gun -> staff -> gun reuses the same actor: clear the live override and
   re-read the data pose in the equip lifecycle (`OnEquipped`), not only at spawn (~328).
4. **Old gun hands anchored to the centre.** With Maintain Y the gun's offset from the screen centre scales with height:
   place the gun hands (right and left, `GBHUD.cpp` ~610) at `ClipX/2 + k * ClipY`, with k chosen so 16:9 stays exactly
   as today (0.66 -> `0.5 + 0.16*16/9`, 0.54 -> `0.5 + 0.04*16/9`).
5. **Bob depth.** Use the root's full current forward distance (rest + reload + kick + rack) for the cm conversion, not
   the rest depth (~985).
6. **Nearest filter really applied.** After setting `TF_Nearest` on the imported overlay texture call `UpdateResource()`
   (`GBDevCommands.cpp` ~2963).
7. **Failed load clears the overlay.** A bad or missing file must clear any overlay already shown (~2947); the smoke
   checks it in that order: load a valid PNG first (write a small 480x270 test PNG into `Saved/` from the smoke), then a
   bad path, then `HasHandOverlay() == false`.
8. **Smoke must be able to fail.** The aim check projects with the same data FOV twice (~6955), so it passes even with the
   feature off. Add checks: the camera has the first-person FOV enabled at the data value, and EVERY primitive of the
   held gun (all attached components, including the muzzle flash) has `FirstPersonPrimitiveType == FirstPerson`.

Checks: the build, `-Filter '^Glyphbound.Weapons.'` and `Smoke-Jam.ps1 -Only ViewPose -Offscreen`, plus once
`-Only pistol`, `-Only rifle`, `-Only shotgun` (or the exact names of the existing gun smoke steps) to confirm the old
reload and pump checks still pass. Do not commit.

## Final report
Per defect: what changed (file:line). The exact last results of every check run. What you could not verify.
