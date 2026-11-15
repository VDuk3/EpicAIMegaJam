# Review: the final squad crowding rule (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario:
a relocation loop that never settles or spams squad requests, an enemy that freezes or stops shooting, a castle or future
enemy that now behaves worse. No style notes. If nothing, say so.

The change: `git show 9ec38af -- Glyphbound/Source/Glyphbound/AI` (the final version; an earlier version you reviewed
counted melee mates and only applied in Hold).
- `AI/GBSquad.*`: `NearestRangedMateDistance` (2D distance to the nearest living, present RangedGround member of the
  enemy's own squad: the same members `FindPosition` keeps SquadSpacing 350 cm from).
- `AI/GBSkeletalEnemy.cpp` (TickRanged): outside a peek (Hold or MoveToPosition), within CrowdDistance 220 cm of such a
  mate for CrowdTime 1.5 s -> bRelocate (bounded by RelocateCooldown 1.5 s and the burst phase). Consider narrow
  corridors (FutureBase) where no candidate is 350 cm from every mate: the squad answers "nothing passed", the enemy
  holds where it stands, and could ask again every cooldown.
