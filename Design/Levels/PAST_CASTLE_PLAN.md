# The castle (past): plan, version 1

> Written by the architect (Claude Fable 5.2) on 2026-11-14, at the owner's request. Version 0 was a proposal on paper; the owner answered "yes, go ahead" and this version is drawn **to scale over the castle as the pack's author assembled it**, now restored and measured.
> Status: **slice 1 is built** (owner: "yes, build it"): section 10 says what the map really has and where it differs from this plan. It waits for the owner's first walk.
> Drawing: `Devlog/media/2026-11-14/castle_plan_v1.png` (plan and section, made by `Design/Levels/make_castle_plan.js`).
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
The castle pack is Medieval Kingdom (Hivemind, Fab, Professional license). On 2026-11-13 the first planner kept 31 of its meshes and the rest went to the Windows Recycle Bin with the town. **On 2026-11-14 the castle part was restored** (`Tools/Jam/Restore-CastlePack.ps1`: 939 items, 4.4 GB; the town, landscape, water, foliage and mannequins stay in the bin), then read and measured in the Editor.

**Records** (`Design/Levels/kits/castle/`): one JSON per level of the author's castle (every piece with its place, turn and scale; the moss clumps are left out), `pieces.json` (336 meshes: size, pivot, triangles, materials, collision), and the keep's floor plan cut at 2.5 m.

### 3.1 The keep, as its author built it
Five sections placed together (`LI_Full_Castle`): a **T, 92 m wide and 59 m deep, 55 m to the top**.

| Section | Size | What it is |
|---|---|---|
| Front Hall | 24 x 28 m inside | the entrance: a gate arch 10 m wide between **two round towers, each with a spiral stair** from the ground to the roof (doors at three floors); arcades along both sides; an upper floor 9 m up; a roof walk between the two towers |
| Main Hall | 24 x 28 m | **a stair hall**: the grand stair climbs in six flights from the floor to 25 m, with a gallery of arches 9 m up; two arched doorways from the Front Hall; a door to each wing on the ground **and another on the gallery level** |
| King's Hall | 28 x 24 m, 15 m high | the great hall of one wing: an arcade around the floor (pillars every 4 m), **a gallery all round, 9 m up, that has no stair of its own**, two long tables with high-backed chairs |
| Side Hall | 28 x 24 m | the other wing: the same hall, with rooms on its gallery level and a stair up to **a roof terrace** with a turret |
| Top Level | 26 x 26 m, from +24 m | three more floors over the Main Hall, **four turrets with spiral stairs** at its corners, a walkway between them |

- **Storeys are 9 m apart** (walking floors at +0.9, +10.2, +19.2, then +25, +34.2, +43.4 m above the keep's zero).
- **The spiral stair** is 7 m across and climbs 4.5 m per piece, two pieces to a storey.
- **Doors:** the big doorways are 4 m pieces; **tower and room doors are about 1.3 m wide**. Towers are for the player and for small enemies; fights with big enemies stay in the halls and the wards.
- **Under both wings** the pillars go on down 5 m to a ring of arches with no floor: the castle stands on legs, meant for a hillside.

### 3.2 The author's walls and gate
- **Gatehouse** (`LI_Front_Gate`, 27.5 x 10.5 m, 24 m high): a front and a back arch 10 m wide, a portcullis, two door leaves, an upper room over the passage and two spiral stairs inside.
- **Wall tower** (`LI_CurtainWall_Tower_01`, 7 x 7.7 m, 25 m): doors onto the wall walk at +10 m, two more floors, crenels.
- **Wall stair** (`LI_CurtainWall_Stairs_01`): a wide first flight (2.1 m) and three narrow ones (1 m wide, 5 m each) up to +10 m.
- **Corner towers** (two sizes, 17 and 23 m across) and **curtain wall** pieces: 4 m long, 4.3 m thick, 13.6 m high. The author's own ring is 150 x 175 m, laid on a hill we do not have; it is not reused, its sections are.

### 3.3 The second interior set
41 low, plain meshes with **no material of their own** (folder `Candle`): 8 m corridor tiles with walls 7.5 m high and arches, **a stair tile that drops 3.05 m in 8 m** with its own walls and vault, cell walls with a door arch and bars, vaulted ceilings, a hall (floor 12 x 16 m, side walls 14 m, an apse 21 m high), pillars whole and broken, rubble. They take any material, which suits the pixel-art look of the past (GDD §3: structures are 3D with pixel textures).

### 3.4 What this changes from version 0
| Version 0 said | The author's castle has | Version 1 |
|---|---|---|
| a tower with a spiral stair, somewhere on the wall | six spiral stairs: two in the entrance towers, four on the Top Level | the entrance towers are the first spiral stairs the player meets |
| an upper floor reached only along the wall | galleries 9 m up in both wings with no stair of their own, and a door to them from the Main Hall's stair | kept: the King's Hall gallery is the floor above that does not connect to the one below |
| a long wall between two towers | a roof walk between the two entrance towers 18 m up, and the wall walk at +10 m | both, in later slices |
| Front Hall with a guard room and a buttery | one large hall with an arcade along each side and no side rooms | the two "rooms" are the two aisles behind the arcades |
| the crypt stair leaves from the Front Hall | the Main Hall, a stair hall, lies behind it | the crypt stair is in a rear corner of the Main Hall |
| storeys of about 5 m | storeys of 9 m | every height in section 5 |

## 4. The place
**Blackwall Castle** (provisional name; the station of the future took it). A border castle on a rock, built around something older than itself: **the seal**, in a crypt under the keep. The keep stands on the crypt to guard it. That is why the future base's Anomaly wing is underground: it was dug around the same crypt.

**Its state when the mage arrives:** the garrison is gone and **orcs are camped in it**. That gives the castle its obstacles without scattering props: the orcs' camp in the ward (fire, kegs, a cart, barricades), and the castle's own decay (a breach in a wall, a broken floor, rubble that can be climbed).

**Look:** stone and timber, lit by fire (torches, braziers, chandeliers, candles) and by daylight through windows and gates. The crypt is the dark place; the seal is the one cold, bright thing in it, as in the future. **The pixel-art look of the past is a separate step,** tested on one section and shown to the owner before it is applied: the keep's stone is finely sculpted and photographic, and how it becomes pixel art is not decided.

## 5. The castle in section
Heights above the lower ward. The keep's zero sits 1.5 m above it, so that its floor meets the upper ward with three steps.

| Height | Place | How the player gets there |
|---|---|---|
| -9.8 m | the crypt: cells and the seal's hall | four flights of the crypt stair, from the Main Hall |
| 0 | gatehouse passage and lower ward | the start |
| +2.1 m | upper ward and the two side yards | the wide stair, or the rubble at the breach |
| +2.4 m | the keep's ground floor: Front Hall, Main Hall, King's Hall, Side Hall | the keep's gate arch |
| +12 m (the plan first said +10; measured when built) | the wall walk | the wall stair in a side yard |
| +11.7 m | the galleries of the wings; the Front Hall's upper floor | the grand stair, a spiral stair, or the wall walk through a wall tower |
| +20.7 m | the roof walk between the entrance towers; the Side Hall's terrace | the spiral stairs; the Side Hall's stair |
| +26.5 m and up | the Top Level: three floors and four turrets | the grand stair's last flight, then the turrets' spiral stairs |

## 6. The places
### Slice 1: from the gate to the seal (the game's opening)
The castle's version of the future slice (arrival, a corridor with a couple of rooms, a hall). It is the GDD's opening sequence: start, a small group of orcs, the seal, the future. About 170 m on foot.

**6.1 Gatehouse (the start, no fight).** The author's gatehouse, whole. The mage walks in under two arches, the portcullis jammed half way up. It is the squeeze before the ward; through the inner arch he sees the ward, the fire and the keep's towers.

**6.2 The wards (first fight).** One open space 52 m wide between the curtain walls, on two heights, with a third above it.
- **Lower ward (0), 28 m deep:** the orcs' camp. A fire in the middle (the hero object), a cart, four barricades of crates and kegs: low cover that belongs there, set so that no line runs clear from the gate to the stair.
- **Upper ward (+2.1 m), 25 m deep,** held by a retaining wall. Two ways up: **the wide stair** in the middle (8.2 m, where the fight funnels) and **a breach** at one end, where the wall has fallen and the rubble makes steps of a single jump (the flank).
- **Two side yards** run on along the Front Hall, 8.5 m wide: one holds the wall stair, whose gate is shut in this slice; the other the orcs' stores (provisional pickups: a health and a mana potion).
- **The wall walk (+10 m)** is seen from the whole ward and not reached yet.
- **Fight:** `Opening_Past`, about 8 enemies: at the fire, behind the barricades and on the terrace.

**6.3 Front Hall.** The author's hall, whole: 24 x 28 m, an arcade along each side. A place to breathe after the ward. The two aisles behind the arcades are its "rooms": weapon stands in one, kegs in the other. The doors of the two spiral stairs are there to be seen, shut for now.

**6.4 Main Hall.** Through one of two arched doorways into the stair hall: the grand stair goes up out of sight, 25 m of it. The doors to the wings are barred. **In a rear corner, a stair goes down:** the crypt stair, 8 m wide, under the back wall of the keep. Its exact spot is fixed on the model, clear of the grand stair's flights.

**6.5 The crypt stair and the cells.** From the second interior set. Two flights straight back, a turn, two more: 12 m down in all, and the two turns hide what comes next. Then a vaulted corridor past three **cells** with bars: one open (a chest), one with a broken floor. The cold light of the seal shows at the end of the corridor before the hall does.

**6.6 The seal's hall (no fight).** A pillared hall, 24 x 24 m, under an 11 m vault, in the rock behind the keep. The seal lies on the floor at the far end, between four standing stones cut with runes: the four points where the future's emitters stand around it. Interacting with it starts cutscene 1 and the future.

### The rest of the castle (outline, provisional)
Built after slice 1 is walked, one request at a time. Everything named here already stands in the author's keep.

- **The entrance towers.** The two spiral stairs: ground, the Front Hall's upper floor, the roof.
- **The roof walk.** Between the two entrance towers, 23 m long and 20.7 m up, over the gate: the passage between two towers.
- **The wall walk.** From the wall stair along the curtain wall to a wall tower, and through its door into the King's Hall gallery.
- **King's Hall.** The large combat hall on two levels: the floor (two long tables as low cover, the arcade's pillars to circle around) and **the gallery at +11.7 m, which the floor cannot reach.** The player first sees the gallery from below; later he arrives on it from the wall walk or from the grand stair, and drops into the hall.
- **Side Hall** and its roof terrace.
- **The Top Level.** Three floors and four spiral turrets above the Main Hall; the highest room for the medieval Valkyrie, bound in runes.
- **Under the wings.** The ring of arches 5 m below the hall floors, entered from outside the keep: a low, cold undercroft that can join the crypt from behind (a second loop).

**Where the elements could live** (the owner's call; a starting proposal only): lightning in a turret of the Top Level, air on the roof walk and the wall walk, ice in the undercroft, fire in the King's Hall.

### Loops
1. Lower ward and upper ward, by the wide stair and the breach.
2. Upper ward, wall stair, wall walk, wall tower, King's Hall gallery, a drop to the hall floor, Main Hall, Front Hall, upper ward.
3. Front Hall, a spiral stair, the roof walk, the other spiral stair, Front Hall.
4. Main Hall, crypt, undercroft, the outside of the keep, upper ward.

## 7. Rules for building it
The rules of `FUTURE_BASE_SLICE1.md` section 5 and of section 2 above, and:
- **The author's sections are placed as he assembled them** wherever one exists for the place; our level design goes on top.
- **Fights in the past are mostly melee and swarm.** The castle gives them funnels (stairs, doorways, the wall) and room to move in the wards and the halls; the spells that wall and pierce are meant to matter there.
- **Fights stay where the doors are wide** (wards, halls, the big doorways). The 1.3 m doors of towers and rooms are for the player.
- **Steps for a single jump are 1.5 m or less** (kegs, crates, rubble).
- **Every light has its fixture:** a torch, a brazier, a chandelier, a candle, a window.
- **Collision:** most of the kit's pieces have no simple collision at all; like the base's kits, they block as they look (complex collision as simple, from both sides).

## 8. Technical decisions (architect)
- **Its own map,** `/Game/Glyphbound/Maps/PastCastle`, built by `Tools/Jam/Build-PastCastle.py` with the base builder's stages, walk check and tour spots.
- **The seal joins the two maps.** How the game travels between two maps while keeping the player's state is to be checked in the era code before slice 1 is called done.
- **The moss** of the author's castle (about 35,000 instanced clumps) is left out: it is foliage for a photographic look.
- **Frame:** +X is the way the player advances, from the gate to the keep; the lower ward is Z = 0.

## 9. Open, for the owner
1. Is slice 1 right as drawn (gate, two wards, Front Hall, Main Hall, crypt stair, cells, the seal's hall)?
2. The pixel-art look of the castle's stone: which way to go, from a test on one section.
3. Where each element lives, and where the medieval Valkyrie is bound.

## 10. What is built (version 1 of the map, 2026-11-14) and what is not
Map: `/Game/Glyphbound/Maps/PastCastle`, built by `Tools/Jam/Build-PastCastle.py` in nine stages (world, ground, keep, gate, walls, wards, camp, crypt, gameplay). **The game now starts on this map**, and the seal opens the future base.

| Place in the plan | Version 1 | Differs from the plan |
|---|---|---|
| 6.1 Gatehouse | The author's gatehouse, whole (316 pieces), its passage on the map's axis. The player starts on the road 13 m outside it | Its passage is an arch about 3 m wide, not 10 |
| 6.2 The wards | Curtain walls 52 m apart (50 pieces of the pack's wall, crenels outwards) with the author's wall tower at the four corners; the lower ward paved with the pack's cobbles; the upper ward 2.1 m higher on a retaining wall of twelve 4 m modules with its coping, paved in stone; the wide stair in the middle; the breach (one module gone, an unseen slope under rocks) at one end; the camp: a fire with a flickering light, five barricades of kegs, barrels and crates, a cart; the orcs' stores and a weapon stand in the far side yard; the author's wall stair in the other, shut by a portcullis | **The wall walk is 12 m up, not 10** (measured from the author's own stair and towers: it is level with the keep's galleries). No pickups placed yet |
| 6.3 Front Hall, 6.4 Main Hall | The author's keep, whole: 5,190 pieces from six of his levels, with a warm light at each of his 50 torches | **Nothing in the keep is shut.** The doors to the wings, the grand stair and the spiral stairs are all open, so the owner can walk every hall on his first visit; they are barred on his word |
| 6.5 The crypt stair and the cells | From the second interior set. The stair's mouth is an open flight in the Main Hall's rear corner on the King's Hall side, beside the grand stair's first flight, with a parapet on its two long sides; it passes under the keep's back wall (a low doorway, about 2 m), then a second flight, a landing, a turn, two more flights; a vaulted corridor of three bays with a cell off each (two barred, the middle one open, with a chest) | Four of the hall's floor tiles are left out for the mouth. The broken floor, the arches between bays and the chest's content are not done |
| 6.6 The seal's hall | Three by three vaulted bays (24 x 24 m) on four pillars, 12 m below the keep's floor (Z -960). The seal (`AGBSeal`) at the far end between four standing blocks, under a cold light that is seen from the corridor before the hall is | The vault's crown is 7.5 m up, not 11. The standing stones are plain blocks |

**Light.** A low warm sun, a sky light, fog; the eye adapts between the ward and the halls within limits. Inside: one light per torch of the author's. In the crypt: a candle's worth of warm light at each turn and bay, and the seal's cold light. Plain slabs of rock stand around and over the crypt so that no daylight reaches it.

**The seal joins the two maps** (new: `AGBSeal::TravelMapPath`). After its caption the seal opens `/Game/Glyphbound/Maps/FutureBase`, which starts in the future. Only the progress flags cross; at this point of the game the player carries nothing else.

**The fight.** `Opening_Past` is placed: it starts when the player is out of the gate passage; eight points for its enemies, at the fire, behind the barricades and on the terrace. Not yet seen with its enemies.

**Materials.** Every piece keeps the material its author gave it. The second interior set has none: it wears the castle pack's wall and floor stone. The upper ward's tiles are the author's terrace tiles, paved in stone instead of his timber. The pixel-art look of the past is not applied (section 9).

**Collision.** Kit pieces block as they look (complex collision as simple, both sides), set on 108 mesh assets of the pack; small props and what hangs overhead do not block.

**Review.** `gb.Smoke tour` stands the player on 28 spots, from the road to the seal, and checks that the seal's map can be opened. The builder walks seven routes for holes and for anything across them.

**Not built, or left open**
- The land outside is a flat plain; behind the keep the crypt's rock is plain slabs, not seen from this slice.
- Pickups (potions, the chest), the cells' broken floor, the arches between the corridor's bays.
- Closing the keep for the opening (wings, grand stair, spiral stairs), if the owner wants the route enforced.
- The wall walk, the towers, the upper floors as places of play: later slices.
