# Block: the player climbs ledges (Nov 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Opus 5.5). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's request
Original prompt (Spanish):

> *"Quiero que cuando el personaje salta y queda en el borde de una estructura, haga la animacion de escalar y suba dicha estructura. Tambien cuando el personaje esta pegado a un bloque/estructura y presiona el salto, si el salto es lo suficientemente alto, que haga la animacion de escalar dicha estructura/bloque en vez de saltar."*

Translation: "When the character jumps and ends up at the edge of a structure, it should play the climb animation and climb that structure. Also, when the character is right against a block/structure and presses jump, if the jump is high enough, it should play the climb animation and climb that structure/block instead of jumping."

## Design decisions (architect, provisional; GDD v0.44, §18 "Ledge climb")
- **First person, so the "climb animation" is first-person.** The player has no body mesh: the hands are 2D pixel-art sprites drawn by the HUD and guns are 3D models under the camera. The climb is the camera dipping 10° (with 3° of roll) as it goes over the edge, the held item dropping out of view, and both pixel hands coming up from the bottom of the screen with their sleeves and gripping the ledge where it projects on screen. No new art: the existing `hand_left` / `hand_right` sprites, with the cuff rows tiled downward as sleeves.
- **Two triggers.**
  - In the air, rising or falling: the climb starts on its own when the view faces a wall (within 60°), and the wall's top is 15 cm to 1.6 m above the feet.
  - Jump pressed on the ground: the climb happens instead of the jump when the top is between 0.5 m and one jump's rise (2.04 m) plus 1.6 m. A higher wall is a normal jump, and the double jump can still end in an air climb.
- **"Facing the wall" is required.** Jumps commit to their direction (air control 0), so the view is the clearest sign of intent. Without it, strafing along a wall would grab it.
- **The test arena's gates had to be re-sized.** The climb adds up to 1.6 m of reach on top of every jump. Measured with the arena's own formulas:
  - The course's last step (1.5 jump rises = 3.06 m) became reachable with one jump plus a climb (3.64 m). It now rises 1.5 jump rises **plus** the reach (4.66 m): halfway between "one jump + climb" and "double jump + climb".
  - From the course top, a double jump plus a climb reached 12.8 m, over the 11 m outer walls. The walls are now 14 m.
  - The past-era raised platform (6 m, future-only ramp) stays out of reach: a double jump plus a climb gives 5.68 m. A new warning fires if the numbers ever change that.
- All the numbers are stats in `Player.json` (`Mantle*`), read with modifiers like the other movement stats. Only the view feel (dip, roll, pull, item drop) is code constants.

## How it was built
1. **Brief** `Prompts/mantle_climb.md` (architect): the full design, ledge detection, trigger rules, the view and HUD animation, the arena changes, two pure-logic tests and a `mantle:` smoke block.
2. **Muse Spark 1.3 Contributor (High) via OpenCode** (run `191856-opencode-faf360`, default implementer) implemented it in about 25 minutes. New pure module `Player/GBMantle.*`, the player state machine, `AGBHUD::DrawMantleHands`, the arena, the tests and 8 smoke steps.
   - It found a real bug on its own: the forward wall sweep started flush with the floor and hit it, so the jump-press climb never started. It lifted the sweep 5 cm.
   - It reported that its screenshots were unreliable in filtered runs.
3. **The architect checked the screenshots itself** (rule: self-review art before delivering). The two climb shots showed no hands at all. A state log at the moment of each shot said the climb was running with the hands at full grip, so the problem was in the drawing. There were two causes:
   - **The hands were hidden behind the RPG status bar.** At 68% of the climb the ledge is already below the view, the clamp put the hands at the screen bottom, and the bar is drawn over them. Also, the shot was taken too late.
   - **The camera never dipped.** The camera follows the pawn's control rotation (`bUsePawnControlRotation`), and the engine overwrites its rotation every view (`UCameraComponent::GetCameraView`, checked in the 5.9 source), so any relative rotation is thrown away. **This also means the existing view shake (spell interrupts, GDD 18) has never been visible since it was added.** The view offsets now go through the camera's additive offset, which the engine applies after the control rotation. The shake works too.
4. **Review: GPT Sol 6.1** (Codex CLI, medium, read-only, run `194720-codex-635804`, prompt `Prompts/mantle_climb_review_sol.md`). Six findings, all accepted and fixed:
   - P1: the climb only checked the capsule at the corner and at the end, so a thin ceiling in between could be passed through. Both path segments are now capsule sweeps.
   - P2: the camera dip was overwritten. Same finding as the architect's, found independently.
   - P2: a thin beam can pass the top check with nothing under the capsule's centre. The climb now needs walkable floor under the end point.
   - P2: releasing jump during a climb (the movement mode is Flying) resets the jump count, so a cancelled climb refilled the double jump. The count is saved and restored on cancel.
   - P3: the tests would pass a "hands never show" or a "teleport across" implementation, and the limits only used default numbers. Asserts were added for mid-path progress, another rise share, interior grip values, non-default reach numbers and another jump.
   - P3: the smoke accepted any non-ground mode as a "normal jump", and a climb still in progress as finished. It now needs Falling plus 10 cm of rise, and walking on top after the climb.
5. **HUD placement, judged on screenshots.** The hands first sat over the status bar's centre crest (only the knuckles showed). They moved out to 27% / 73% of the width, and their lowest grip line keeps the whole hand and some sleeve above the bar. Final shots: `media/2026-11-13/ledgeclimb_grip_ground_720.png` (climbing the 2 m wall from a jump press) and `ledgeclimb_grip_air_720.png` (air grab at the platform edge).
6. **Smoke screenshots in filtered runs.** A filtered run starts its first step 1 s after the map loads, while frames still hitch, and a screenshot can capture an older frame. The `mantle:` block now waits 3 s first, and each climb shot freezes time (world time dilation 0.01) until the next step, so the shot shows the climb at a known moment.

## Exception to the default implementer
The fixes after the review were made by the architect chat itself, not sent back to Muse. They were six small, precise changes that the architect had already diagnosed from engine source and screenshots, plus HUD placement judged image by image. Writing a brief would have cost more than making them.

## Checks
- Iteration: Editor target only, `^Glyphbound.Player.` tests (3 green), `Smoke-Jam.ps1 -Only mantle -Offscreen` (about 22 s per run). Climb results: wall top feet 202 cm (expected 200 ± 10), platform 602 cm (600 ± 10), the 6 m wall is a normal jump (138 cm of rise, not climbing), view and held item exactly at rest after the climb.
- End of block: game target build succeeded; full suite 47 tests green (0.89 s); full smoke clean (152 s, no LogGB errors or warnings). The arena logs step tops 122 / 245 / 711 cm with a climb reach of 160 cm.

## Tools
Claude Code (Claude Opus 5.5, architect), OpenCode with Muse Spark 1.3 Contributor (High), Codex CLI with GPT Sol 6.1 (review), Unreal Engine 5.9.0.

## Open for the owner
The reach (1.6 m), the speed (0.22 s + 0.18 s per metre) and the "must face the wall" rule are provisional. Playing it is the real test: agents do not run PIE.

## Package
- `TeamTBD_Glyphbound.zip` from commit `9bf924a`: 299.7 MB, BuildCookRun 153 s. The full smoke on the packaged game is clean (142 s, no LogGB errors). Climb numbers match the Editor run: wall 202 cm, platform 602 cm, the 6 m wall is a normal jump (145 cm of rise).
- In the full run the climb block runs in the future era with the shotgun in hand: the 3D gun drops out of view and the pixel hands grip, with the cyberpunk HUD around them (`media/2026-11-13/ledgeclimb_grip_packaged_cyber_720.png`). Sent to the owner to play.

## Second round: climbing in any direction (GDD v0.45)
After the package, the owner wrote, in Spanish:

> *"Implementa que se pueda escalar sin mirar el muro, estando de espalda por ejemplo."*

Translation: "Make it possible to climb without looking at the wall, with your back to it, for example."

**Design (architect, provisional).** The view no longer picks the wall. The player's intent does:
- **On the ground:** the held movement direction. With no direction held, the climb looks all around in 8 directions, the view's first, then +-45, +-90, +-135 and 180 degrees. So with your back to a wall and no keys held, a jump press climbs it. Holding S into a wall behind you climbs it. Holding W away from a wall behind you is a normal jump forward, never a climb backwards.
- **In the air:** the jump's own direction (jumps commit to it). When the horizontal speed is under 1.5 m/s (a vertical jump, or a jump the wall has stopped), the climb looks all around.
- **The view turns.** When the wall is more than 60 degrees off the view, the view turns to it during the rise, so the player ends on top looking across. The turn is applied in deltas, so the mouse keeps working during it.
- Stats: `MantleFacingMinDot` became `MantleApproachMinDot` (the probe has to meet the wall within 60 degrees of its face). New: `MantleAirSearchMinSpeed` (150) and `MantleTurnMinAngle` (60).

**Build.**
- Brief `Prompts/mantle_any_direction.md`, implemented by **Muse Spark 1.3 Contributor (High)** via OpenCode (default implementer).
- New pure functions `GBMantle::GetProbeDirections` and `TurnFraction`, test `Glyphbound.Player.MantleProbe`, and four new `mantle:` smoke steps: back to the 2 m wall, back to the platform in the air, and holding away from a wall behind.

**Review: GPT Sol 6.1** (run `203639-codex-a57072`, prompt `Prompts/mantle_any_direction_review_sol.md`). Four findings, all fixed by the architect (small, already diagnosed):
- **P2, a real input-order bug:** Enhanced Input fires the jump's `Started` before the same frame's `Move` event. So pressing W and jump together with your back to a wall read "no direction held" and would climb backwards. The climb now reads the Move action's current value (`UEnhancedPlayerInput::GetActionValue`), not the last Move event.
- **P3:** the "held away" smoke step only checked "not climbing" right after the press. It now also checks, 0.2 simulated seconds later, a real jump: falling, rising and moving away from the wall. That follow-up check caught a gap in the smoke's own input hook: the hook fed the climb decision but not the jump's take-off direction. The jump left with no speed and grabbed the wall behind it in the air. In the game, a held W gives the jump its speed (450 cm/s) by take-off. The hook now feeds both.
- **P3:** the probe test now checks the full priority order, and the turn test checks smoothstep values at a quarter and three quarters of the rise (a linear turn would pass the old one).

**Known trade-off for the owner:** a jump in place right next to a block now climbs it even if the player is not looking at it. From 0.5 m up it climbs on the press; lower blocks (15 to 50 cm) are grabbed once in the air, because the air search looks all around. That is what was asked, but in combat it may happen by accident.

**Checks.** Game target build succeeded; full suite 48 tests green (1.01 s); full smoke clean (155 s, no LogGB errors or warnings): back to the wall climbs and turns the view to -90, the air grab with the back to the platform turns it to 0, and holding away is a jump at 540 cm/s away from the wall.
