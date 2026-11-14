# Block: the three guns handed to the LAPTOP for the pixel-art hands (Nov 13-14, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## Where it started
The owner: the LAPTOP chat that does all the pixel art needs the three weapons' assets to animate the pixel-art hands. Fix it, handle it, and give me the prompt for the people on the LAPTOP. (Translated from Spanish.)

## What the LAPTOP needs, and why it is not the 3D files
In first person the hands are 2D sprites that `AGBHUD::DrawHands` draws over the 3D gun. One sprite pixel is `ClipY / 120` screen pixels (9 at 1080p), so the whole screen is 213 x 120 art pixels. The hands in the game today are 18 x 20 placeholder fists at fixed places on the screen: they do not hold the gun. To draw hands that grip it, the artist needs to see the gun as the game's camera sees it, at the hands' pixel scale. The `.blend` files would not help in Pixelorama.

## What was done
- **Screenshots from the packaged game** (Editor left alone: another chat had it open), with the smoke run at 1920 x 1080: `Smoke-Jam.ps1 -Packaged -Offscreen -ResX 1920 -ResY 1080`. Two runs:
  - the first with the placeholder hands, to show where they sit today;
  - the second without them. The game reads its data as loose files, so `Tools/Jam/blank_hand_sprites.js` made `hand_right`, `hand_left` and `hand_cast` transparent in the packaged copy of `Sprites.json` only, and the copy was restored afterwards (checked identical).
- 14 poses kept: pistol idle, three reload beats (30, 45, 70 %) and enchant; rifle idle, fire and three reload beats; shotgun idle, pump fully back, a shell going in (55 %) and enchant.
- **Plates:** `Tools/Jam/Make-HandPlates.ps1` turns each screenshot into a 213 x 120 plate, area-averaged, to trace 1:1 in Pixelorama. Example: `media/2026-11-13/hands_plate_rifle_idle_1080.png` and `hands_plate_rifle_idle_213x120.png`.
- Side views of each gun: the pistol and the shotgun from earlier review renders; the rifle's right side rendered again with `rifle_render_review.py`.
- Everything went to the LAPTOP's mailbox: `Incoming/LAPTOP/_para_LAPTOP/20261114-0400_manos-armas.md` and its folder (53 MB; `Incoming/` is not in git).

## Decisions (architect)
- **Every frame of a hand is the whole screen:** 213 x 120, pivot [0, 0], one angle. The hand is drawn where it belongs on the plate, and CENTRAL crops and places it. Nobody has to measure coordinates, and the hand matches the gun the game shows.
- **One sprite per hand and gun** (`hands_<gun>_right`, `hands_<gun>_left`): the left hand moves on its own (magazine, pump, enchant glyph).
- The 2 px margin rule does not apply to the bottom edge of `hands_*`: the sleeve comes in from off screen.
- Priority: pistol first (the first gun the player gets), idle before anything else.
- The scale stays at 9 screen pixels per art pixel unless the artist finds it too coarse for fingers on a detailed gun. Then CENTRAL changes `HandPixelDivisor` and makes new plates.

## Found on the way
The first 1080p smoke run reported two timing errors: the pistol's energy bar did not blink with 2 s left, and the shotgun's shell count was off by one after one shell. The second run, on the same build, was clean. They look like timing flakes at the higher resolution; to watch.

## The LAPTOP's answer and the scale (Nov 14, 04:20 UTC)
The LAPTOP agent prepared the material (35 files checked by SHA-256) and changed `spritelab.py check` so `hands_*` may touch the bottom edge (tested with synthetic one-pixel frames). It raised one point: the first-person staff hand already approved there, `fp_hand_staff`, is drawn on a 480 x 270 screen (4 screen pixels per art pixel), so gun hands at 9 px would be 2.25 times coarser. The person on the LAPTOP left the choice to the team.

**Decision (architect): every first-person hand at 4 px, a 480 x 270 canvas.** It keeps the approved staff hand as it is: 6 px would mean scaling it by 1.5, which bends pixel art. `Make-HandPlates.ps1` got a `-Scale` parameter (default 4); new 480 x 270 plates and the style guide went to the mailbox (`_para_LAPTOP/20261114-0420_manos-escala.md`), with a scope cut for the time left: idle for the three guns first, then fire, pump and enchant, reloads last. The game's hand scale (`HandPixelDivisor`) changes when the first hands arrive.

Noted for the owner: the LAPTOP transcript shows `fp_hand_staff` partly built by a generator script, while the pipeline agreed on Nov 13 says the AI there writes only tooling and never makes pixels. The LAPTOP was asked to say in its `NOTES.md` which parts were drawn by hand and which came from a script.
