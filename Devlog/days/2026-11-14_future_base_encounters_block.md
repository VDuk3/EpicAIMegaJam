# 2026-11-14, block: future base, the fights are placed (at the enemies chat's request)

Architect of the map: Claude Code (Fable 5.2) on CENTRAL, the "future base" chat. Continues `2026-11-14_future_base_sixth_walk_block.md`.

## Where the request came from
Not from the owner directly. The chat that is integrating the 3D enemies ("Integrar assets de personajes y enemigos", another Claude Code session on this machine) wrote that the owner had told it that night to "incorporate the enemies and leave them playable" and not to wait for him, that it had written four encounters into `Content/Data/Encounters.json`, and asked this chat, as the map's owner, to place them through the builder (anything put in the map by hand is wiped by the next rebuild). The two chats agreed by message who touches which files; both commit only their own hunks.

What the owner had said to this chat about fights, kept as he said it: in the medium room (the lab) "no fight for now": **none is placed there**. Of the deep wing's small room: it "will be infested", and the architect would not place the enemies. The enemies chat's order is later, so its encounter is placed there as provisional, and both chats have told the owner that which enemies infest that room is his call.

## What was built
`Tools/Jam/Build-FutureBase.py`, new stage `encounters`, from a table (`ENCOUNTERS`; `ENCOUNTERS_ON = False` builds the map without any). Each row is an `AGBEncounter` that exists in the future only: its id, the middle and half size of its trigger box, and where its enemies appear.

| Encounter | Place | Trigger box (middle; half size) | Enemies appear |
|---|---|---|---|
| `Future_Bay` | transfer bay | (3700, -75, 150); 1200 x 400 x 300 | 4 points at the bay's east end, 3 in the north branch |
| `Future_Spiders` | long corridor | (3380, 5600, 150); 160 x 450 x 200 | 4 points ahead, towards the far door |
| `SubBoss_Future` | sub-boss hall | (3380, 9550, 150); 1350 x 1450 x 300 | 6 points on the floor before the exit door; clearing it opens that door and gives a Power Cell |
| `Future_DeepWing` | deep wing, small room | (11654, 0, -450); 540 x 540 x 250 | 3 points in the room, 3 up the dark corridor; gives the second Power Cell |

The rules behind the places: a trigger starts where the player is already inside the room, never in its doorway (the sub-boss's starts once the player is out from under the entry bridge); the enemies appear on the far side from the way in, on the floor, since none of them can use crates, ladders or galleries; the room's cover is between them and the player.

## Checked
- **Navigation, in the Editor:** each of the 23 spawn points projects onto the navmesh, and from each there is a complete path to where the player stands when the fight starts (6 to 27 m). The same call returns nothing for a point in the void, so the check can fail.
- **The tour does not fight.** `TourRun::Start` removes every encounter before the tour begins (a spot inside a trigger would start the fight, and a dead player fails every later check). With the four encounters in the map: **tour 84 spots pass**; 55 tests green (54 of this chat's and the enemies chat's working tree).
- **Not checked by this chat:** the fights themselves. The enemies' code and the encounter definitions are the other chat's, uncommitted in the working tree when this was built; its own smoke visits the four places.
- **The package on disk does not have the encounters** (`TeamTBD_Glyphbound.zip`, 1,906.5 MB, made before them). The enemies chat packages after its round.

## How the two chats worked on one machine
One `machine.lock` for builds, Editor sessions and game runs; messages between the sessions for who goes next; the builder as the only way into the map; the shared file `Core/GBDevCommands.cpp` committed by hunk (this chat's is six lines in `TourRun::Start`; the other four hunks in the file at that moment are the enemies chat's and stay uncommitted for it to commit).
