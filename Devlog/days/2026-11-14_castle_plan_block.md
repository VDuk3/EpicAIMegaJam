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
