# Review: general weapon perks back on the hub, per-gun perk balance — Glyphbound (UE 5.9)

You are reviewing, read-only. Run `git diff HEAD -- Glyphbound/Content/Data Glyphbound/Source Tools/Balance` and read
what it touches. Ignore Devlog files; `Design/GDD.md` only to check that its new §18 "Weapon tree, v0.41" entry
matches the data.

What changed (owner's decisions, Nov 13):
1. `Skills.json`: Marksman (`Weapon.All.HeadshotMult`), Precision Recovery and Arcane Conduit
   (`Weapon.All.EnchantBuildUpMult`) are general again and have no `Branch` (they sit on the weapon hub page);
   Arcane Conduit now needs only Heavy Rounds. Deadeye stays in the Pistol branch (stacks with Marksman). Each gun
   keeps its own Quick Trigger, Rapid Reload (now x0.85 per rank, was x0.9) and Steady Grip. Tight Choke (shotgun
   spread) is now x0.85 per rank. Heavy Rounds moved to column 2 on the hub.
2. `UI/GBHUD_Menus.cpp`: the gun emblems (pseudo-nodes) are placed right under the hub's root perk (the hub node with
   no requirement), centred on its column; emblem links come only from such root perks; the grid height is the
   tallest of the plain rows and the emblems' extended rows; emblem numbers are their order, not their column.
3. Smoke (`Core/GBDevCommands.cpp`): Arcane Conduit is checked with the pistol again; Rapid Reload expects
   0.55 x base at rank 3.
4. `Tools/Balance/balance_model.js`: a new table "what one point buys"; a provisional accuracy model
   (`MissFromRecoil`, `MissFromSpread`) so recoil and spread perks change accuracy; pellets and magazine size now go
   through the perk modifiers like the game's `GetStat`.

## What to look for (ranked)
1. Data: unreachable nodes, wrong keys, descriptions that do not match the modifiers, rank stacking that does not
   give the smoke's 0.55 (how do several ranks of Multiply 0.85 combine in `UGBStatsComponent`?), a hub layout where
   two nodes share a cell (columns 0-4: Marksman col 0 rows 1-2, emblems cols 1-3 row 1, Arcane Conduit col 4 row 1,
   Heavy Rounds col 2 row 0).
2. UI: the root detection (`Requires.Num() == 0` on a hub node) with other data (no root, two roots), an emblem
   overlapping Precision Recovery (emblems are 2.3 rows tall), links drawn through other boxes, `FirstColumn`
   arithmetic with more or fewer branches.
3. Balance tool: is the accuracy formula sound (accuracy can never pass 1 or drop below its base for a perk that
   lowers recoil or spread)? Does the "what one point buys" table compare like with like? Did the pellets / magazine
   change alter any earlier output that had no such perk (it should not)?
4. Smoke: point arithmetic in the perk and Arcane Conduit steps with the new prerequisites; the removed rifle swap
   leaving anything behind.

Do not suggest style rewrites or new features, and do not judge the design choices. For each finding give:
file:line, what goes wrong, a concrete scenario, and a fix. If you find nothing real, say so plainly.
