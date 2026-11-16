# Portal route (2026-11-15, final build)

What is built for the jam build, start to end. It follows the v0.70 plan in `GDD.md` §4 ("Portals and the route
between the eras") cut down to what the maps have today: the castle's ground is open, so the castle legs C2 and C3
are one visit, and the base's P1/P2/P3 portals are folded into one home portal by the seal (P0).

Owner's fixed points: the first entry into the castle itself (the keep's door, not the outer gate) takes the player to
the future; coming back to the past, the player appears at the dungeon portal.

| # | Where | Portal / event (actor) | Goes to (arrival) | Needs | Leaves done |
|---|---|---|---|---|---|
| 1 | PastCastle: gate, wards, wide stair | `Event_CastleDoor` at the keep's door (walk-in, once, caption `CastleDoor`) | FutureBase, player start by the seal | nothing | `CastleDoorEvent`, `SealUsed` |
| 2 | FutureBase: control room, corridor, bay (first gun), sub-boss hall (Power Cell 1), deep wing / tentacle hall (Power Cell 2), energy gate, the future Valkyrie | `Portal_ToPast` in the control room, behind the seal (cyber frame) | PastCastle, `Arrival_Dungeon` before the pixel portal | nothing (open from the arrival) | Cryo Core (`QuestItem_Future`) and the hilt, from the future Valkyrie |
| 3 | PastCastle: dungeon, Main Hall, King's Hall, Side Hall, Top Level: the past Valkyrie | `Portal_Dungeon` (pixel portal `Portal_1`) in the dungeon by the seal's hall | FutureBase, `Arrival_Base` before `Portal_ToPast` | `SealUsed` | Rune Shard (`QuestItem_Past`) |
| 4 | either map, inventory | combine Rune Shard + Cryo Core | | both quest items | `FinalPortalOpen` |
| 5 | FutureBase control room | `FinalPortal` (south side of the control room) | EntityArena (`Game.json` EntityMapPath), the reaper | `FinalPortalOpen` | the end |

Order is free between steps 2 and 3: the player can come back to the past any time, and go
forward again from the dungeon portal. Every map crossing hands over the whole player state (weapons, ammo, spells,
items, skills), sets the new map's own era, restores health, mana and ammo, and saves the checkpoint there.
