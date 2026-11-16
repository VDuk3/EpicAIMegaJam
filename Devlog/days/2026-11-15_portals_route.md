# 2026-11-15 — Portals for the whole route (portal session, Claude Opus 5.5)

Task from the orchestrator: leave everything about portals done, so the game can be finished from start to end.
Owner's fixed points: the first time the player enters the castle itself (the keep's door, not the outer gate) the
event that takes them to the future happens; coming back to the past, they appear at the portal in the dungeon.

## What was found
- The v0.70 portal plan (GDD §4, five portals P0 to P4, six alternating legs) needs castle places that are not built
  (the undercroft, the barred shortcuts). It was cut down to what the maps have: see `Design/2026-11-15_PortalRoute.md`.
- **A real bug that blocked the ending:** the seal's map travel (`AGBSeal::TravelMapPath`) opened FutureBase with a
  fresh pawn; the player's weapons, spells, items and skills stayed behind. The Rune Shard from the castle could never
  reach the future to be combined with the Cryo Core, so the final portal could never open.

## Decision (architect) and how
- `AGBPortal` gets a **map-portal mode configured by actor tags** (`GBMap=`, `GBArrive=`, `GBRequires=`,
  `GBOnceFlag=`, `GBSetFlag=`, `GBCutscene=`, `GBOnEnter`/`GBRadius=`, `GBNoFrame`). Why tags: new properties would
  change the header and need an Editor restart, which the shared Editor could not afford 1.5 hours before the deadline;
  tags are `.cpp` only, compiled with Live Coding.
- Every crossing hands over the whole player state through the progress checkpoint (the pattern of the final portal),
  sets the **new map's own era** (the snapshot carried the old one, which would have shown castle-only pieces in the
  base), places the player at the `GBArrival=<id>` target point, restores health, mana and ammo, and saves the
  checkpoint (portals are the safe points, GDD §4). The seal's map travel now uses the same hand-over.
- New caption `CastleDoor` (Cutscenes.json) for the keep-door event (the seal's lines spoke of a hand on the seal).
- Implemented directly by the architect, not by Muse: about 200 lines in one file against a 1.5-hour deadline, with
  the shared Editor's turns to coordinate. Review: Sol 6.1 (medium) through Codex CLI, logged wrapper.

## Looks
- Castle: the pixel portal `Portal_1` (the `Portal` clip baked from Portal FX `NS_Portal_9`, `castle_portal.py`)
  stays as it was; the logic actor stands at its foot.
- Base: the SciFiWorld gate frame (`SM_MGate01-500x350-1_GateFrame`) with Portal FX's own surface swirl
  (`SM_PortalSurface` with `MI_PortalSurface_4`, its background disc behind) in the opening, and a cold light.

## Third-party assets
- **Portal FX** by SHOGUN GAMES, Fab, https://www.fab.com/listings/0062e70e-3133-4c7e-8c13-a8232b87123a, Fab Standard
  License (from the owner's library; already logged in `2026-11-15_overnight_block.md`). Used now also in 3D: its
  surface mesh and materials in the base's portal.
- The gate frame is from the SciFiWorld kit already in the project (logged when that kit came in).

## Scripts (maps are not in git)
- `Tools/Jam/portals_future.py`: Portal_ToPast, Arrival_Base, FinalPortal in FutureBase's control room.
- `Tools/Jam/portals_castle.py`: Event_CastleDoor, Portal_Dungeon, Arrival_Dungeon in PastCastle.
Both back the map up to `Glyphbound/Saved/MapBackups/` first, probe the floors and abort without saving if a spot is wrong.

## The missing elemental chests (owner, same evening)
- Owner: place the two missing spellbooks and the two missing staffs. `Tools/Jam/castle_chests_elements.py` puts four
  `AGBChest` in PastCastle along the route after its fights: `Staff_Storm` at the back of the Front Hall, the Lightning
  book in the King's Hall, `Staff_Frost` and the Ice book in the Side Hall (before the Top Level: the past Valkyrie takes
  more ice damage). Spots chosen by traces against the live map (one first guess was inside a wall, one on a barrel);
  each chest stands against a wall, away from the doorways. The cell chests and the Mimic are untouched.
- Valkyrie check (owner asked): both fights have their encounter, room, AI (strafe and bursts, combos, dash, Judgement
  circles, lances, phase two under half health) and drops (Rune Shard; Cryo Core and the hilt). Not checked by walking:
  that the castle's Top Level is reachable on foot (the navmesh path stops near +15 m, between two nav volumes).
