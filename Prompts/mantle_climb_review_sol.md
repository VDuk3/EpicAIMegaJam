# Code review: ledge climb (mantle) in Glyphbound (UE 5.9 C++)

Review the uncommitted changes in this repository (`git diff` and new untracked files under
`Glyphbound/Source/Glyphbound`, plus `Glyphbound/Content/Data/Player.json`). Read-only: do not edit anything.

The spec the implementer followed is `Prompts/mantle_climb.md`; the design rule is in `Design/GDD.md` §18 "Ledge climb".
The game is first person: the climb is a camera dip, the held item lowered, and 2D pixel hands drawn by `AGBHUD::DrawHands`.

Look for concrete defects, with a failure scenario for each:
- Ledge detection: wrong traces/channels, climbing pawns, climbing from behind/sideways, false positives on slopes or
  steps, start-penetrating hits, the capsule ending inside geometry or a ceiling, wrong heights (feet vs centre).
- Trigger rules: jump press on the ground climbs only when one jump + reach covers the top; the air grab, rising or
  falling, not when moving away; never while dead, dashing, in the QTE, frozen or already climbing.
- State: movement mode restore (Flying -> Walking), jump count, cancel paths (death, respawn, load, freeze, control
  off, EndPlay), inputs left blocked or held after the climb, time dilation.
- View: camera relative rotation/location and `HeldItemAnchor` must return exactly to rest and must compose with the
  existing view shake without residue; aim (control rotation) must not change.
- HUD: projection fallback, divide-by-zero, sprite UV sub-rect math, drawing when the sprite is missing.
- Pure functions in `Player/GBMantle.*` and the two tests `Glyphbound.Player.MantleReach` / `MantlePath`: could a wrong
  implementation pass them?
- Smoke steps `mantle:` in `Core/GBDevCommands.cpp`: do they really check what they claim, and do they leave the player
  where later steps expect?
- Test arena changes in `World/GBTestArena.cpp`.

Report findings ranked by severity as: [P1/P2/P3] title — file:line — failure scenario — fix. If there are none, say so.
