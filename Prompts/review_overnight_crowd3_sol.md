# Review: crowd retry delay and the base Valkyrie check (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario. No
style notes. If nothing, say so.

The change (`git diff HEAD -- Glyphbound/Source`, uncommitted):
- `AI/GBSkeletalEnemy.*`: your last finding (failed crowd relocations retry forever in a narrow corridor): when the
  squad's position search returns nothing, `CrowdRetryTimer` = CrowdRetryDelay (6 s) and the crowd rule is off until it
  runs out.
- `AI/GBValkyrieEnemy.h`: `IsLifted()` (raised by Judgement, MOVE_Flying while she casts).
- `Core/GBDevCommands.cpp` (Enemies3DBase, FutureBase): the "she left the tube" check written before her new fight now
  accepts her raised by Judgement as standing (she may cast 3 s after waking) and needs her more than 150 cm from where
  she lay (her wake leap lands ~2.15 m out on the navmesh), inside the zone, encounter Active. It also takes two
  screenshots. Can it still fail when she stays in the tube?
