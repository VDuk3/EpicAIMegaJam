Review the uncommitted changes of this Unreal Engine 5.9 game-jam project (read-only; do not change anything).
See them with `git status --short` and `git diff`; the new files are:
  Glyphbound/Source/Glyphbound/World/GBBreakable.h, GBBreakable.cpp      (a wall that breaks after N hits)
  Glyphbound/Source/Glyphbound/World/GBEnergyGate.h, GBEnergyGate.cpp    (AGBEnergyGate and AGBKeySocket)
  Tools/Jam/make_wing_materials.py, Tools/Jam/Make-CrackTexture.ps1
and changed: Core/GBDevCommands.cpp (namespace TourRun: checks on review spots named breakwall / gatetest / gatecheck),
Tests/GBWorldTests.cpp, Content/Data/Items.json (item PowerCell), Tools/Jam/Build-FutureBase.py (stages lab, valk, deep,
boss; helpers ramp, decal, breakable, shell, floor_fit, roof_fit, hall_corners, crate), Tools/Jam/corridor_blender_export.py.

What they are meant to do:
- AGBBreakable: a neutral thing with attributes (same pattern as AGBSpell_Wall::SetupHealth). Every damage event of at
  least MinHitDamage that comes at least HitCooldown after the last counted one is one hit; at HitsToBreak it sets its
  progress flag, spawns a burst and destroys itself. A crack overlay material gets "Stage" = hits / hits to break.
  Projectiles in this game only overlap WorldDynamic, hence the BlockAllDynamic box with overlap events.
- AGBEnergyGate: blocks until every flag of RequiredFlags exists in UGBProgressSubsystem, then fades and stops blocking.
- AGBKeySocket: when the player comes within Reach carrying ItemId, removes one and sets Flag.
- The builder script runs inside the Unreal Editor's Python (top-level await allowed; `unreal`, `asyncio` imported).

Look for real defects only: a hit that is counted twice or never, a breakable that can be left unbreakable or that
breaks in one shotgun blast, use of a destroyed actor, a gate that can never go out or that goes out with a flag
missing, a socket that eats the item without setting the flag (or sets it without the item), state that does not
survive a reload the way the flags do, a tick that never stops, the tour checks passing when they should fail, the
tour leaving flags set after it ends, sign or off-by-one errors in ramp(), floor_fit(), roof_fit(), shell() and
hall_corners(), a doorway of stage_deep left walled up (WELL east wall, NEST west and east walls, PIT west wall), and
anything in stage_deep or stage_valk that would raise when the whole map is rebuilt. Do not report style.
Answer with a short list: file:line, the defect, the concrete failure. If there is nothing, say so in one line.
