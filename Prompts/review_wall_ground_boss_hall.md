Review the uncommitted changes of this Unreal Engine 5.9 game-jam project (read-only; do not change anything).
Run `git diff -- Glyphbound/Source Tools/Jam/Build-FutureBase.py Tools/Jam/import_corridor.py Tools/Jam/corridor_blender_export.py` to see them.

What they are meant to do:
1. C++ (Glyphbound/Source/Glyphbound/Spells): a spell that stands on the ground must always appear. Before, AGBSpell_Wall
   searched the ground by tracing down from the wall's own height above the caster; in a corridor lower than the wall the
   trace started above the roof and the wall rose on the roof. New helper AGBSpellActor::FindGroundUnder(Point, Lift, Out,
   MaxDistance) traces up first and never starts through a ceiling; the wall now searches from the caster's height and
   stops short of anything solid in front of the caster; FindAimedGroundPoint (zone and trap spells) uses the helper.
   UGBSpellbookComponent::DebugCastAtFullStrength is for the smoke run only.
2. C++ (Core/GBDevCommands.cpp, namespace TourRun): on a review spot whose name contains "castwall" the tour casts the ice
   wall at full strength and fails the run unless exactly one wall has its base within 40 cm of the player's feet.
3. Python (Tools/Jam/Build-FutureBase.py, runs inside the Unreal Editor with top-level await): stage_lab (a kit room
   placed again from JSON without some pieces, plus copies of its own pieces), stage_boss (a hall built from 4 m wall
   modules, galleries, crates), two sliding door leaves at the end of stage_long, and check_walks, which now also traces
   each walk at chest height.

Look for real defects only: a case where the wall or a zone still ends up out of sight or inside geometry, a caster
without a world or without a collision capsule, a wall that can now spawn on top of its caster, wrong trace channel or
ignored actors, the tour check passing when it should fail (or failing on a healthy map), off-by-one or sign errors in
hall_wall / the gallery, rail and crate coordinates of stage_boss (inner faces x 1980..4780, y 7600..11200; galleries
4 m wide at z 400; crates 155 cm wide and 150 cm high at scale 1.38; the player's jump rises 204 cm), a crate route to a
gallery that is not actually climbable. Do not report style. Answer with a short list: file:line, the defect, the
concrete failure. If there is nothing, say so in one line.
