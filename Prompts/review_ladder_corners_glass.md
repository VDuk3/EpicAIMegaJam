Review the uncommitted changes of this Unreal Engine 5.9 game-jam project (read-only; do not change anything).
See them with `git status --short` and `git diff`; new files: Glyphbound/Source/Glyphbound/World/GBLadder.h and GBLadder.cpp.
Changed: Player/GBPlayerCharacter.h/.cpp (StartLadder and the ladder branches of TickMantle and CancelMantle),
Player/GBMantle.h/.cpp (LadderSway), Tests/GBPlayerTests.cpp, Tests/GBWorldTests.cpp, Core/GBDevCommands.cpp
(namespace TourRun: spots named "laddertest"), Tools/Jam/Build-FutureBase.py (hall_corners, the glass breakable in
stage_hall, the ladders, tanks, platforms and lamps of stage_deep), Tools/Jam/make_wing_materials.py.

What they are meant to do:
- AGBLadder is one end of a ladder; two are placed per ladder (foot and head). Interact starts
  AGBPlayerCharacter::StartLadder, which reuses the ledge-climb (mantle) state: MantleStart is always the foot,
  MantleEnd the head; bMantleReversed runs the path from head to foot; bMantleStraight makes the path one straight
  line (a slanted ship's ladder) instead of up-then-across; bMantleLadder adds a small sway and eases the player in
  from where it stood. The view turns to the yaw the climber faces on the ladder.
- hall_corners(): the kit's corner pillar is finished on its own -X and +Y faces; each of the four corners turns it so
  those two faces look into the room. Corners in order: SW (-90), SE (0), NE (90), NW (180). Pillar local bounds:
  x -140.6..1.2, y -101..41.

Look for real defects only: a climb that can leave the player stuck in the Flying movement mode or with input held,
a climb that starts while another one runs or while dead, state not reset on cancel or at the end (the next ledge
climb must not be reversed or straight), the view turn going the long way round, division by zero, the ladder being
usable from the wrong end or from anywhere within reach in a way that teleports the player far, a wrong sign in
GetOtherEnd or in the yaw passed to StartLadder, the tour's ladder check passing when it should fail, and wrong
maths in hall_corners (the yaw per corner, or the pillar's offset after rotation). Do not report style.
Answer with a short list: file:line, the defect, the concrete failure. If there is nothing, say so in one line.
