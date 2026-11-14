# The castle (past): plan, version 0

> Written by the architect (Claude Fable 5.2) on 2026-11-14, at the owner's request.
> Status: **a proposal on paper.** Nothing is built and nothing is measured yet: the castle kit has to be restored and looked at first (section 3). Every size here is provisional until then.
> Nothing of the deleted world plan (2026-11-13) is reused: not its layout, its route or its zone names.

## 1. What the owner asked for (his words, translated)
- "We have to make the map of the medieval world, the castle. We already did the cyberpunk world and reached an acceptable level design."
- "It is not all one corridor, and it is not one giant rectangle connected to chambers. There is a certain dynamism: you go up, you go down, the areas where you fight have two levels, there are obstacles, it is all varied. That same style has to be carried to the castle."
- "It will not be exactly the same, because a castle is different. A castle may have a spiral stair for a tower. A castle may have one floor above another that are not connected. A castle may have a long passage between two towers. It can be more varied."
- "Let's start with the plan. Analyse the decisions taken in the cyberpunk map's design so you learn from it."
- "There is already a castle asset in the project. That castle already has a certain structure we could use. Its assets are modular, so we can keep building with it."

## 2. What the future base taught
Read from `FUTURE_BASE_SLICE1.md`, its seven dev log blocks and the owner's six walks.

**What made it work**
1. **The place comes first.** "Blackwall Station, the Anomaly wing, built to contain and watch the seal": every room had a job, and the job said what stands in it. The rejected maps were sizes and parts lists.
2. **The kit's own demo map is the structure.** Whole sections were placed again exactly as their author assembled them (shell, lights, exposure), and our level design went on top, piece by piece. The plan gave way wherever the kit was different (the corridor came out 10 m wide, not 5).
3. **Every piece was looked at before it was used** (a picture of each mesh, the demo from inside). A piece is never trusted by its name: the pack's "ladder" was a ship's stair.
4. **Small slices, each walked by the owner.** Arrival, a corridor with two rooms, one combat hall; then one request at a time: a door, a long corridor, a sub-boss hall, a wall that breaks, a wing 6 m down, a ladder, a boss hall that is a dead end.

**The level design decisions**
- A combat space has **two walkable heights, three at most**, and the high one is a reward: a catwalk with a stair at each end, a bridge, a lookout with a drop that closes the loop.
- **Loops, no dead ends**, except a boss hall.
- **More than one way to change height:** stairs, a ramp, crates as steps, a hand ladder, a drop, a ledge climb. Each room uses a different one.
- **The player sees the fight before entering it** and enemies start on the far side, in their zone, out of sight.
- **Squeeze, then release:** a low lock or a long narrow corridor before a large hall. The corridor bends so the hall is revealed.
- **Cover belongs to the place** (crates where a loader leaves them). Tall blocks break long sight lines.
- **One hero object per room,** one accent colour per zone, light in pools, every light with a fixture, a darker exposure for the deep wing.
- **Things that work:** doors that open, a gate that needs two keys dropped by enemies, a wall that breaks, glass that shows the boss from the first visit.

**What the owner's walks corrected** (each is now a rule)
- No holes where two kits meet: a threshold in every doorway, and the builder walks every connection.
- A stair's foot is clear, lit, and not behind anything.
- A pit always has a way out on foot with a single jump (steps of 1.5 m or less against a 2 m jump).
- Stacked things stand flush.
- Collision is what is seen, from both sides (enemies shot through one-sided walls).
- A door an enemy must use is wide enough for it (a 1.3 m door kept large enemies in).
- Assets reserved for one fight are not spent elsewhere.

**What the enemies now need from a map** (tactics block, 2026-11-14)
A zone per encounter that they never leave, posts and patrol routes, hidden points for reinforcements, cover with a firing spot beside it, and navmesh joined across every height.

## 3. What there is to build with
**The owner's premise needs one correction.** The castle pack is Medieval Kingdom (Hivemind, Fab, Professional license). On 2026-11-13 the first planner kept 31 of its meshes and the other 1,697 files went to the Windows Recycle Bin, with the town. What is in the project today is 31 loose pieces: two hall walls, one tower wall, two tower floors, a crenel, the portcullis, two stairs, low walls, a podium, a door, props.

**The structure the owner means is in the Recycle Bin, and it is still there** (checked 2026-11-14, read-only):
- **The castle as its author assembled it:** `LI_Full_Castle`, `LI_Front_Gate`, `LI_Front_Hall`, `LI_Kings_Hall`, `LI_Side_Hall`, `LI_Top_Level`, `LI_CurtainWall_Tower_01`, `LI_CurtainWall_Stairs_01`, `LI_CurtainWall_Corner_01/02`, and the sub-levels `SL_Castle`, `SL_Castle_Walls`, `SL_Courtyard`.
- **The castle kit, about 95 meshes:** a tower set (walls, door walls, 16 window walls, floors, wall tops, crenels, pillars), **`SM_Spiral_Stairs`** and its cap, a curtain wall set with a turret, a gate set (three arches, portcullis frame), an arched walkway (side and corner), the hall's main stairs, wooden internal floors, cone and large roofs, doors and windows.
- **A second interior set, 41 meshes** (folder `Candle`): vaulted corridor with arches, a stair corridor, cell walls with bars and doors, a pillared hall with its floor, end wall and ceiling, broken floor and rubble.
- **Props:** furniture, crates, kegs, chains, candles, weapons, chests, a cannon.

**First step, before any drawing to scale** (waits for the machine: another chat is packaging):
1. Restore the castle part from the Recycle Bin (no town, water, foliage or mannequins).
2. Write the author's castle to `Design/Levels/kits/castle_demo.json` with `extract_level_pieces.py`, as was done for the base's kits.
3. Look at it: a picture of every piece, and views from inside each assembled section.
4. Measure what the plan hangs on: storey height, stair rises, door widths, the spiral stair's width and rise per turn, the wall walk's width.

## 4. The place
**Blackwall Castle** (provisional name; the station of the future took it). A border castle on a rock, built around something older than itself: **the seal**, in a crypt under the keep. The keep stands on the crypt to guard it. That is why the future base's Anomaly wing is underground: it was dug around the same crypt.

**Its state when the mage arrives:** the garrison is gone and **orcs are camped in it**. That gives the castle its obstacles without scattering props: the orcs' camp in the ward (fire, kegs, a cart, barricades), and the castle's own decay (a fallen stair, a broken floor, rubble that can be climbed).

**Look:** stone and timber, lit by fire (torches, braziers, chandeliers, candles) and by daylight through windows and gates. The crypt is the dark place; the seal is the one cold, bright thing in it, as in the future. The pixel-art look of the past (§3 of the GDD: structures are 3D with pixel textures) is a separate step, tested on one section and shown to the owner before it is applied.

## 5. The castle in section
A castle changes height in more ways than a base. Each way is used once before any is repeated.

| Height (provisional) | Place | How the player gets there |
|---|---|---|
| about -6 m | the crypt, with the seal | a vaulted stair corridor from the keep; later, the cellars |
| 0 | gatehouse and lower ward | the start |
| +2 m | upper ward, the terrace before the keep | a wide stair and a narrow side stair |
| +2 m | the keep's ground floor (Front Hall, King's Hall) | the keep's door |
| +5 to +7 m | the keep's upper floor and the King's Hall gallery | **not from inside**: only along the wall, through a tower door |
| +8 to +10 m | the wall walk | **the spiral stair** of a tower |
| above | tower tops | the spiral stair again; the highest places wait for the double jump |

## 6. The places
### Slice 1: from the gate to the seal (the game's opening)
The castle's version of the future slice (arrival, a corridor with a couple of rooms, a hall). It is the GDD's opening sequence: start, a small group of orcs, the seal, the future.

**6.1 Gatehouse (the start, no fight).** The mage walks in through the gate passage: three arches in a row, the portcullis jammed half way up. Low and narrow on purpose: the squeeze before the ward. Through the last arch he sees the ward and the keep above it.

**6.2 The wards (first fight).** One open space on two heights, with a third above it.
- **Lower ward (0):** the orcs' camp. A fire in the middle (the hero object), kegs, a cart, a barricade of crates: low cover that belongs there.
- **Upper ward (+2 m):** the terrace in front of the keep, held by a low wall. Two ways up: the wide stair in the middle, where the fight funnels, and a narrow stair at one end, which flanks it.
- **The wall walk above** is seen from here and not reached yet: the stair to it is in a tower whose door is shut. A shaman can stand up there in a later fight.
- **Fight:** `Opening_Past`, about 8 enemies. The player enters with a view over the whole ward from the gate; the orcs are at the fire and on the terrace.

**6.3 Front Hall of the keep.** The keep's entrance hall, as the pack's author built it. A place to breathe, with two small rooms off it, each for a reason:
- **Guard room:** weapon stands, a table, a bench. Provisional pickup: a mana potion.
- **Buttery:** kegs and sacks, 0.5 m down three steps. Provisional pickup: a health potion.
- The big door to the King's Hall is **barred from the other side** for now. The stair down to the crypt is open.

**6.4 The crypt stair and the cells.** The second interior set: a vaulted corridor that goes down in two flights with a turn, then runs past **cells** with bars. One cell is open (a chest), one has a broken floor. The turn hides the crypt hall; its cold light shows at the end of the corridor before the hall does.

**6.5 The seal's hall (no fight).** The pillared hall of the same set. The seal lies on the floor at the far end, between four standing stones cut with runes: the same four points where the future's emitters stand around it. Interacting with it starts cutscene 1 and the future.

### The rest of the castle (outline, provisional)
Built after slice 1 is walked, one request at a time.

- **West tower.** A round tower at the upper ward. **The spiral stair** goes up through its floors; each floor is a round room, and fights happen in the rooms, not on the stair (too tight for the camera and for enemies).
- **The long wall.** The wall walk from the west tower to the east tower: long, narrow, open to the sky, crenels on one side and a drop into the ward on the other. A fight in a line: a rush along the wall that the ice wall can plug and the lightning bolt goes through, with a shaman at the far tower. Jumping down into the ward is a one-way shortcut.
- **East tower and the keep's upper floor.** From the wall, a tower door leads into the upper floor of the keep. This is **the floor above that does not connect to the one below**: the hall's own stair has fallen.
- **King's Hall.** The large combat hall: the floor (long tables as low cover, pillars to circle around) and the gallery above it (enemies who shoot). The player first sees the gallery from below, on the other side of the barred door; later he arrives on it from the wall and **drops through the broken floor** into the hall, which opens the barred door from inside and closes the loop.
- **The cellars.** Under the upper ward, entered from the lower ward: a low, cold store with the castle's well. They join the crypt from behind (a second loop). Provisional link to the future: this is the stone that has come through the roof of the tentacle boss's hall.
- **The top.** The highest room of the keep or a tower top, for the medieval Valkyrie, bound in runes.

**Where the elements could live** (the owner's call; a starting proposal only): lightning at the top of the west tower, air on the long wall, ice in the cellars and the well, fire at the King's Hall's great hearth.

### Loops
1. Lower ward and upper ward, by the two stairs.
2. Upper ward, west tower, long wall, east tower, upper floor, down through the broken floor into the King's Hall, Front Hall, upper ward.
3. Lower ward, cellars, crypt, Front Hall.

## 7. Rules for building it
The rules of `FUTURE_BASE_SLICE1.md` section 5 and of section 2 above, and:
- **The author's sections are placed as he assembled them** wherever one exists for the place; our level design goes on top.
- **Fights in the past are mostly melee and swarm.** The castle gives them funnels (stairs, doors, the wall) and room to move in the wards and the hall; the spells that wall and pierce are meant to matter there.
- **A door an enemy must pass is at least 2 m wide**, 3 m on a sub-boss's path.
- **Steps for a single jump are 1.5 m or less** (kegs, crates, rubble).
- **Every light has its fixture:** a torch, a brazier, a chandelier, a candle, a window.

## 8. Technical decisions (architect)
- **Its own map,** `/Game/Glyphbound/Maps/PastCastle`, built by `Tools/Jam/Build-PastCastle.py` with the base builder's stages, walk check and tour spots.
- **The seal joins the two maps.** How the game travels between two maps while keeping the player's state is to be checked in the era code before slice 1 is called done.

## 9. Open, for the owner
1. Is this the castle (a keep over the seal's crypt, wards, towers joined by a wall, orcs camped in it)?
2. Is slice 1 the right first piece (gate, wards, Front Hall, crypt, seal)?
3. Where each element lives, and where the medieval Valkyrie is bound.
