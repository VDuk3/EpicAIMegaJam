# Review: the tactics smoke check's close-pair limits and the crowd reaction time (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario, or a
check that can no longer fail on the behaviour it guards. No style notes. If nothing, say so.

The change (`git diff HEAD -- Glyphbound/Source`, uncommitted):
- `Core/GBDevCommands.cpp` (Enemies3D tactics sampling and E3TCheck): each pair's close interval (within 200 cm) is
  split per frame into "both moving" (both over 100 cm/s) and "one standing". The 3.5 s limit now applies to the
  longest close stretch with one standing (members stacked); a second limit of 8 s applies to two members moving
  together. Before, the 3.5 s limit applied to any close time, and it went red in full runs when two members ran to
  their places side by side (3.6 to 6.1 s), and once when one stood beside another (4.5 s).
- `AI/GBSkeletalEnemy.cpp`: CrowdTime default 1.5 -> 1.0 s (a ranged enemy within 220 cm of a ranged mate outside a
  peek asks for a new position after this long).
Is the split accumulated correctly (reset when apart, per pair)? Can a stacked pair escape the limit by jittering
above 100 cm/s while pinned against each other?
