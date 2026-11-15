# Review: squad crowding relocation and hardened map smokes (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario:
a crash or null dereference, a relocation loop that never settles or spams squad requests, a castle or future enemy
that now behaves worse, a smoke check that cannot fail or fails on correct behaviour. No style notes. If nothing, say so.

The change (`git diff HEAD -- Glyphbound/Source`, uncommitted):
- `AI/GBSquad.*`: `NearestGroundMateDistance` (2D distance to the nearest living, present, non-flyer member of the
  enemy's own squad).
- `AI/GBSkeletalEnemy.*` (TickRanged): a ranged enemy in Hold within CrowdDistance (220 cm) of a squad mate for
  CrowdTime (1.5 s) asks for a new position (the squad's position search keeps SquadSpacing 350 cm from every mate;
  requests are bounded by RelocateCooldown). Fixes a flaky smoke: an orc whose path ran into a robot at its cover gave up
  moving and stood 198 cm from it for 4.5 s.
- `Core/GBDevCommands.cpp`: FogVisit now fails when the gas is not simulating, when the injected blob wrote no source
  entry, or when the shots wrote no entry; its lamp is removed before the gas-off frame sample. CastleVisit fails when a
  wave does not attack within 16 s or when fewer waves engaged than came.
