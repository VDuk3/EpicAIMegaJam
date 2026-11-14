# 2026-11-14, block: future base, the owner's fifth walk (corners again, a glass wall, a hand ladder, a darker hall)

Architect: Claude Code (Fable 5.2) on CENTRAL. Owner's notes in Spanish, translated here. Continues `2026-11-14_future_base_fourth_walk_block.md`.

## What the owner sent
Seven screenshots from the package and seven notes, and later one more picture:

> 1. Revisa nuevamente las esquinas, algunas quedaron mal.
> 2. Cambialo por un muro de vidrio opaco.
> 3. Colocale colision a esos objetos blancos.
> 4. Que sea escalable.
> 5. Cambia las escaleras por escaleras de mano que tenga una animacion sencilla de subir y bajar. Esas escaleras estan en el asset de Deep Water Station.
> 6. Que sirva como plataforma.
> 7. Revisa las esquinas tambien de esta sala y la sala debe ser mas oscura
>
> (later, with a picture of a vertical ladder with hoops at its top) La escalera de manos que te digo es esta

Translation: (1) Check the corners again, some came out wrong. (2) Change it [the breakable shutter, cracked] for a wall of opaque glass. (3) Give those white objects [the shellfish and stones] collision. (4) Make it climbable [the gas tanks]. (5) Change the stairs for hand ladders with a simple animation of going up and down; those ladders are in the Deep Water Station asset. (6) It should work as a platform [a deck tile left tilted in the air]. (7) Check the corners of this hall too, and the hall must be darker [the tentacle boss's]. Later: the hand ladder I mean is this one.

## What was done, note by note
| Note | Cause or reading | Change |
|---|---|---|
| 1, 7. Corners | The kit's corner pillar is finished on two sides only, its own -X and +Y (seen where its author uses it: the two outer corners of the hangar's branch). The first fix stood all four pillars of a hall the same way, so one corner was right and three showed the pillar's open back | Each corner turns its pillar so the two finished sides face the room (SW -90, SE 0, NE 90, NW 180 degrees), in every hall built from wall modules (sub-boss hall, stairwell, small room, tentacle hall). Four review spots look into the sub-boss hall's corners |
| 2. Glass wall | He saw the grate with the crack web over it and did not want a grate | The breakable wall is one sheet of frosted glass in a dark steel frame that stays (`M_GB_FrostedGlass`: opaque, glossy, grey-green). The cracks are pale now: glass cracks white. The first glass was blown out white by the bay's lamps; its colour came down from 0.3 to 0.07 |
| 3. Collision | The architect had left the shellfish and stones without collision so they would not trip the player | They block now (the shells are 0.5 to 0.9 m high) |
| 4. Tanks | The racks are 2.2 m and 3.5 m high: over one jump | Crates beside each: one for the low rack, a single one and a stack of two for the tall one. Every step under the player's 2 m jump |
| 6. Platform | Two fallen deck tiles had been left tilted in the air "as wreckage" | Each lies flat on a crate, 1.4 m up: a platform |
| 7. Darker | | The tentacle hall's lamps are at about half of what they were (40, 34 and 22 cd; red 8; the shaft on the rubble 160). A first cut to a third left the far corners black in the review shots and was raised |

## 5. The hand ladder: three tries
**The mechanic first.** A ladder is two `AGBLadder` actors (new, `World/GBLadder`), one at each end, each standing where the player stands at that end. The Interact key on one starts `AGBPlayerCharacter::StartLadder`, which reuses the ledge climb's own machinery (hands grip, the held item drops, the view turns to the ladder) with three additions: the path can run from the top down; it can be one straight slanted line; and a small lean and dip from rung to rung, computed from the height climbed (`GBMantle::LadderSway`: zero on every rung, one way and then the other between them). The HUD says "Climb up" or "Climb down". The speed is the ladder's own (3 m/s).

**Which ladder.** The owner said the ladders were in the Deep Water Station pack.
1. The pack has three meshes named `SM_Ladder`. Stacked upright under the landing they turned out to be **short ship's stairs** (2 m up for 1.6 m along, with handrails): three loose stair pieces on a wall.
2. So they were laid as what they are, one steep run from the landing to the floor, and the climb was made to follow the slope. It worked and looked like a ship's ladder.
3. The owner then sent a picture: a **vertical** ladder with hoops, on a railed platform. No mesh of the pack is that ladder. Twelve candidate pieces were stood in a lit row far from the map and photographed in the game: the ladder is part of two larger pieces, a big tank platform (`SM_Props01`) and a small railed platform (`SM_PropsA09`, 1.5 x 4.7 m). Measured from its triangles, the small platform's ladder is 4.05 m from foot to deck.

**As built.** `SM_PropsA09` stands out from the landing's edge, level with it; its ladder goes down 4.05 m to a **dock**, a new step of the stairwell 4.5 m deep; from the dock it is a 1.95 m drop to the floor, with a crate at each side to get back up. So the way down is landing, ladder, dock, floor, and the ladder is the pack's own at its own size (stretching it to 6 m would have made every rung half as far again).

## Checked in the game
- The tour has a fifth check: on a spot named `laddertest` it uses the nearest ladder end and fails unless, before it moves on, the player has been carried the ladder's height, stands on its feet, and is at the other end. It runs twice, down and up.
- Tests: `Glyphbound.Player.LadderSway` (still on every rung, opposite lean on the next, no dip upwards, none without rungs) and `Glyphbound.World.LadderSeconds` (as long down as up, a zero speed does not hang the player, an end is usable from its own standing place and not from the floor under it).

## Code review
Sol 6.1 (medium, read-only, through the logged wrapper; prompt `Prompts/review_ladder_corners_glass.md`), two findings, both fixed:
1. A ladder end could be used from anywhere the interact query reaches (2.5 m, through railings, from the floor under its head) and the player was pulled onto it through whatever was between. An end is usable only from its own standing place now: within 1.6 m, level, and on its own floor.
2. The tour's ladder check only looked at height and "not climbing". It also needs the player on the ground and at the other end's place.

## Exception to the implementer rule
The architect wrote the C++ directly (the ladder actor, the three additions to the player's climb, the checks and tests): the climb is an extension of code read and understood in the same sitting, and each look at the ladder in the game needed the Editor that an implementer would have had to share.

## Checked
- Editor target builds; **54 tests green** (two new).
- **Tour: 81 spots pass** on the Editor build and on the package, with its five checks: the ice wall at full strength, the glass wall (stands three hits, breaks at the fourth), the two sockets, the energy gate, and the ladder down (403 cm) and up (408 cm), the player set down on its feet at the other end each time.
- Packaged full arena smoke clean; the runs leave no save file in the package's folder.
- `TeamTBD_Glyphbound.zip`: **1,949.3 MB**, still almost twice the jam's 1 GB.
- Nobody has played it by hand yet. The climb's feel (speed, sway) is the architect's guess until someone holds the key.

## Media (all from the package)
- `media/2026-11-14/futurebase_glass_wall_hand_ladder.png`: the frosted glass wall; the pack's platform at the landing's edge; the view while climbing down; the ladder from the dock; the tanks with their crates and the worn corridor's mouth; a deck tile as a platform in the last hall.
- `media/2026-11-14/futurebase_corner_pillars_turned.png`: the sub-boss hall's four corners.

## Left open
- The ladder has no sound and the hands are the ledge climb's pose, not a hand-over-hand cycle.
- Enemies cannot use the ladder: nothing that walks can follow the player down or up it.
- The dock is a plain dark block; its face could use a wall piece of the kit.
- Still open from the fourth walk: what drops the Power Cells, the Valkyrie's model, the bosses, the wing's enemies, the smoke, the package's size.
