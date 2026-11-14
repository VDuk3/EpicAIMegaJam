# Block: the castle's map starts, with the plan (Nov 14, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Fable 5.2). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's request
Original prompt (Spanish):

> *"Tenemos que hacer el mapa del mundo medieval, del castillo. Ya hicimos un trabajo con el mundo cyberpunk y se logró un diseño de niveles aceptable. No es todo un solo pasillo, tampoco es solo un rectángulo gigante conectado con cámaras, sino que hay cierto dinamismo, se sube, se baja, las áreas donde se combate con los enemigos tienen dos niveles, hay obstáculos, está todo diverso. Entonces, ese mismo estilo tenemos que llevarlo para hacer el castillo. Evidentemente no va a ser exactamente igual porque el castillo es diferente. Un castillo a lo mejor puede tener una escalera circular para una torre. A lo mejor un castillo puede tener un piso encima de otro y no están conectados. Un castillo a lo mejor tiene un pasillo largo entre dos torres. Puede ser más variado. Entonces empecemos con el plano. Analiza y mira las decisiones que se tomaron con el diseño del mapa cyberpunk para que aprendas de él y vamos con los planos, con el plan para el mundo medieval. Hay un asset ya en el proyecto que es de un castillo. Ese castillo pues ya tiene cierta estructura que podríamos utilizar. Los assets de ese castillo son modulares, entonces se puede seguir armando con él. Pero pues ya aprendamos un poco de lo que ya está hecho."*

Translation: We have to make the map of the medieval world, the castle. The cyberpunk world reached an acceptable level design: not one corridor, not one giant rectangle connected to chambers, but ways up and down, fight areas on two levels, obstacles, variety. Carry that style to the castle. It will not be the same, because a castle is different: it may have a spiral stair for a tower, one floor above another that are not connected, a long passage between two towers. Start with the plan. Analyse the decisions of the cyberpunk map's design and learn from them. There is already a castle asset in the project; it already has a structure we could use, and its pieces are modular, so we can keep building with it.

## What was read
- `Design/Levels/FUTURE_BASE_SLICE1.md` (the plan and what each version really built), the seven dev log blocks of the future base, the block of the rejected first maps, the enemy tactics block, and the GDD (§3 art rule, §4, §6, §19 enemies and encounters).
- The player's reach in `Player.json`: single jump until the air zone, ledge climb up to 160 cm, 7 m dash.

## What the future base taught (summary; the full list is in the plan, section 2)
- The place came first, and the kit's own demo map was the structure: whole sections placed as their author built them, our level design on top.
- Fight spaces with two walkable heights, loops, a different way to change height in each room, a view of the fight before entering, squeeze then release.
- The owner's walks turned into rules: thresholds, clear and lit stair feet, a way out of every pit with a single jump, collision from both sides, doors wide enough for the enemies.

## A finding: the castle's structure is not in the project
The owner's premise was that the castle asset, with its structure, is in the project. It is not, and that is this project's own doing:

- The pack is Medieval Kingdom (Hivemind, Fab, Professional license; declared in `2026-11-13_fab_assets_block.md`).
- On 2026-11-13 the first planner kept 31 of its meshes (`Design/Levels/castle_keep_list.txt`) and the other 1,697 files were sent to the Windows Recycle Bin together with the town.
- **They are still in the Recycle Bin** (listed read-only through the Windows shell, 1,697 files with their original folders). Among them:
  - the castle as its author assembled it: `LI_Full_Castle`, `LI_Front_Gate`, `LI_Front_Hall`, `LI_Kings_Hall`, `LI_Side_Hall`, `LI_Top_Level`, the curtain wall's tower, stairs and corners, and three sub-levels;
  - about 95 castle kit meshes, with exactly what the owner named: `SM_Spiral_Stairs`, a tower set, a curtain wall set with a turret, a gate, an arched walkway, the hall's main stairs;
  - a second interior set of 41 meshes (vaulted corridor, stair corridor, cells with bars, a pillared hall, rubble).

Nothing was restored in this block: another chat held the machine (`machine.lock`, "final checks and package") with its Editor open, and 1,700 files appearing in `Content` under a running package is not something to try.

## The plan
`Design/Levels/PAST_CASTLE_PLAN.md`, version 0. A proposal on paper, with no size trusted until the kit is restored and measured.

- **The place:** Blackwall Castle (provisional; the future's station took its name), a keep standing over the crypt that holds the seal, which is why the future's Anomaly wing is underground. Orcs are camped in it: their camp and the castle's decay are the obstacles.
- **In section:** crypt, lower ward, upper ward, the keep's ground floor, its upper floor (reached only along the wall), the wall walk (reached by a tower's spiral stair), tower tops.
- **Slice 1 = the game's opening:** gatehouse, the two wards (first fight, with the wall walk above as a third height seen and not yet reached), the keep's Front Hall with a guard room and a buttery, the crypt stair past the cells, the seal's hall.
- **The rest, in outline:** west tower with the spiral stair, the long wall between two towers, the upper floor that does not connect to the King's Hall below except by a drop through a broken floor, the cellars with the well, a top room for the Valkyrie.
- **Three loops,** so it is neither a corridor nor a rectangle with chambers.

## Decisions (architect)
- **Nothing of the deleted world plan is reused** (the standing rule): not its layout, its route or its zone names.
- **The castle is its own map** (`PastCastle`), built by a script with the base builder's stages, walk check and tour.
- **Restore before drawing to scale.** The first plan failed for placing pieces by name and size; this one is not drawn to scale until the author's castle has been looked at from inside.
- **Exception to the implementer rule:** none needed; this block is reading and a plan. No code changed.
- GDD v0.59, §4.

## Not done in this block
- The restore, the kit's contact sheets, the measurements, and the plan to scale.
- Nothing built, no build or smoke run, nothing packaged.

---

## Second part: the owner's "yes, go ahead"; the castle restored, read and measured; the plan to scale
The owner's answer to the plan (Spanish): *"Sí, adelante"* (Yes, go ahead).

### The restore
- `Tools/Jam/Restore-CastlePack.ps1` reads the Recycle Bin's own records (its `$I` files: original path, size, date) and moves each matching item back where it was. It overwrites nothing.
- **Restored:** 939 items, 4,378 MB: the author's assembled castle levels, the castle kit, the second interior set, props, blueprints, materials and textures.
- **Left in the bin:** 760 items (the town's houses and docks, landscape, water, scanned foliage, demo mannequins).
- Done while the other chat's packaged game was still running, since moving files into `Content/CastleTown` touches neither its package nor its run; the Editor was opened only after it released the machine (17:50), under this chat's own lock.

### Reading the author's castle (Editor, read-only)
Tools written for it, all in `Tools/Jam/`:
- `castle_read.py`: opens each level of the author's castle and writes every piece (mesh, place, turn, scale, bounds), every light and every level instance to JSON; measures every static mesh of the kit (size, pivot, triangles, materials, collision).
- `castle_piece_pictures.py`: one picture of each piece, alone on a floor, with a 1 m and a 1.8 m post beside it.
- `castle_plan_svg.js`: a level's JSON as a top-down plan to scale.
- The existing `capture_views.py` for views and for plan cuts (an orthographic camera looking down from a chosen height).

The first read took ten minutes: the Editor rebuilt the Nanite data of every castle mesh on first load. The caller's HTTP request gave up after five; the script went on inside the Editor and its files were watched instead.

**What was looked at:** 15 levels (11 castle sections, 3 sub-levels, the author's construction level); 336 meshes measured; the keep from four sides and from above; twelve plan cuts of the keep, from 2.5 m below its floor to 56 m; twelve views from inside; a picture of each of 103 pieces (the whole second interior set, stairs, gate, curtain wall, spiral stair, doors).

**Pictures** (`media/2026-11-14/`):
- `castle_author_keep_exterior.png`, `castle_author_keep_top.png`: the keep as the pack's author assembled it.
- `castle_author_keep_cuts_a.png`, `_b.png`: its plan at twelve heights.
- `castle_author_keep_inside_a.png`, `_b.png`: views from inside (unlit; the offscreen capture does not wait for texture streaming, so they are soft).
- `castle_pieces_0.png` to `_3.png`: the pieces, with scale posts.
- `castle_plan_v1.png`: the plan and section of this block.

### What the castle turned out to be
- **A keep in a T, 92 x 59 m and 55 m high, in five sections:** Front Hall (the entrance, between two round towers), Main Hall behind it (a stair hall: a grand stair of six flights, 25 m), King's Hall and Side Hall as wings, and a Top Level of three more floors.
- **Everything the owner named is already in it:** six spiral stairs (two in the entrance towers, four turrets on the Top Level); galleries 9 m up in both wings that have no stair of their own, reached only through a door from the Main Hall's stair; a roof walk between the two entrance towers.
- **Storeys are 9 m apart.** The plan's guess from piece names had been 5 m.
- **The second interior set has no materials at all** (every slot empty): plain low meshes for a vaulted corridor, a stair, cells and a hall. It will take whatever look the past is given.
- **The author's curtain wall** is a ring of 150 x 175 m on a hill, partly built by spline blueprints. It is not reused; its sections (gatehouse, wall tower, wall stair, corner towers, wall pieces) are.
- **Most kit pieces have no simple collision** (they need "complex as simple", as the base's kits did), and **tower and room doors are about 1.3 m wide.**

### The plan, version 1
`Design/Levels/PAST_CASTLE_PLAN.md`, drawn by `Design/Levels/make_castle_plan.js`: the keep's own floor plan (the Editor's cut at 2.5 m, embedded to scale) with what the plan adds around and under it, and a section on the same axis.

- **The keep is placed whole**, as the base's control room, corridor and hangar were.
- **Added in front:** the author's gatehouse, curtain walls 52 m apart with a wall tower at each end, a lower ward (the orcs' camp, the first fight) and an upper ward 2.1 m higher, joined by a wide stair and by a breach with rubble steps; a wall stair in a side yard, shut in this slice.
- **Added below:** the crypt, from the second interior set: a stair of four flights from a rear corner of the Main Hall, a corridor with three cells, the seal's hall under an 11 m vault in the rock behind the keep, 12 m below the keep's floor.
- **Version 0 against the real castle** is a table in the plan (section 3.4): the Front Hall has aisles, not side rooms; the crypt stair leaves from the Main Hall; every height changed.

### Decisions (architect)
- **Restore from the bin, not from Fab:** it was already on disk, it is exactly what the project had, and it needs no download.
- **Only the castle part comes back.** If a castle level turns out to need something left behind, that one thing is restored.
- **The moss is left out of the records and of the map** (35,000 instanced clumps): foliage for a photographic look.
- **The crypt goes behind the keep, not under its halls:** the wings stand on pillars that go 5 m below their floors, and the crypt's vault is 11 m high.
- **No level of the pack was changed or saved.** A review level (`/Game/Glyphbound/Maps/Dev/Review_Castle`) was created for the piece pictures.
- **Exception to the implementer rule:** the architect wrote the reading tools directly; they are small Editor scripts judged on pictures at every step. No game code changed.
- GDD v0.60, §4.

### Mistakes on the way
- The first plan drawing put the crypt stair on top of the grand stair's first flight; it showed once the keep's own floor plan was under the drawing, and the stair moved to a rear corner.
- A one-line patch of the drawing script typed straight into the shell broke its quoting and the file; the script was rewritten whole. The project's rule (one-off scripts in a file) exists for this.

### Checks
- The Editor was opened with `Start-EditorMcp.ps1` (MCP ready in 105 s) and closed with `Stop-EditorMcp.ps1` (6 s, log collected). The machine lock was taken and released.
- No build, no smoke run, no tests: no code changed.

### Not done
- Nothing is built. The next step, after the owner's word on the drawing: `Build-PastCastle.py`, slice 1.
- The pixel-art look of the castle's stone is not decided; a test on one section goes to the owner first.
- The keep's entrance was not looked at up close (how its floor meets the upper ward), nor the exact spot of the crypt stair.
- `Content/CastleTown` is 4.7 GB again; what the map does not use can go back to the bin once the map exists.
