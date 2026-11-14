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

---

## Third part: slice 1 is built
The owner's answer to the drawing (Spanish): *"Sí, constrúyelo"* (Yes, build it).

### How it was built
`Tools/Jam/Build-PastCastle.py`, the future base's method in nine stages, each one rebuilt alone while it was being looked at: world, ground, keep, gate, walls, wards, camp, crypt, gameplay.

- **The author's sections, placed again piece by piece** from the records of the second part: the keep (5,190 pieces from six of his levels), the gatehouse (316), a wall tower at each of the four corners (416), the wall stair (36).
- **Ours, each thing for a reason in the plan:** the curtain walls (50 pieces of the pack's wall), the pack's cobbles in the lower ward and on the road, the upper ward on a retaining wall with the wide stair and a breach, the orcs' camp, and the crypt from the pack's second interior set.
- **Collision:** complex as simple, both sides, set on 108 mesh assets of the pack (most had no simple collision at all).

### What the renders and the game showed, and what was changed
Each stage was looked at before the next one: offscreen views in the Editor, then the game's own tour.

| Seen | Cause | Change |
|---|---|---|
| Half of the lower ward's cobbles were missing, in a checkerboard | The cobble piece's pivot is on its edge; turning every other piece by 90 degrees moved it out of its cell | Not turned |
| The upper ward was a timber deck | The author's terrace tile is planked: his terraces are the keep's roofs | The same tile, paved in the pack's stone |
| The curtain walls showed their crenels to the ward | The piece's crenels are on its own -Y side, the opposite of the guess | The runs are turned round |
| The land was one rock picture stretched over 300 m | One box, one texture | 20 m squares |
| The walk into the Main Hall hit a pillar | The two doorways are 4 m to either side of the axis, not where the plan had guessed | Walk and review spot moved |
| The Front Hall glowed orange | 50 torch lights of 55 cd, saturated | 34 cd, a paler flame |
| The crypt stair's first tiles were not the ones removed from the hall's floor | The author turned many floor tiles, so a tile's pivot can be any of its four corners | Tiles are chosen by their middle |
| A wall of rock stood inside the seal's hall | The slab that keeps daylight out of the crypt followed the keep's back wall, and the hall reaches further west | The rock follows the crypt's real outline |
| The crypt's vaults were smeared white | The brick material stretched on the vault pieces' UVs | The wall's stone |
| A glare on the wall in front of the first flight; the seal's hall burnt out | A candle light a metre from the wall; the seal's light too strong and too low | Moved and dimmed |

Before placing the crypt, the shapes of fifteen pieces of the second interior set were read from their vertices (a 4 x 4 grid of heights per piece): which way the stair tile rises, where a vault springs, where a cell wall's door is. Nothing in the crypt was placed from a name.

### The plan against the map
In the plan's new section 10. The larger differences: the wall walk is 12 m up, not 10 (level with the keep's galleries); the gate passage and the keep's entrance are arches about 3 m wide, not 10; the crypt's mouth is beside the grand stair's first flight; the seal's hall is three by three vaulted bays, 7.5 m to the crown.

### Code (architect, directly)
- **`AGBSeal::TravelMapPath`** (`World/GBSeal.h/.cpp`): a seal can open another map after its caption instead of changing the era in place. The plan had left open how the castle and the base, two maps, would be joined; this is the smallest thing that does it. Only the progress flags cross to the new map, which is enough for the opening (the player carries only what he starts with).
- **`HasTravelMap()`** asks the package store whether that map exists, not the file system: in a packaged build a map is inside a container file. The existing final portal checks a file on disk; its check was not reused.
- **A tour check, "sealmap"** (`Core/GBDevCommands.cpp`): on a spot with that word in its name, every seal of the map must have a map that can be opened, or the tour fails. Without it a seal whose map is not cooked would fall back to the travel in place and nobody would notice.
- **Exception to the implementer rule:** about thirty lines tied to the map work, written by the architect.

### Start map and launchers
- `GameDefaultMap` is `/Game/Glyphbound/Maps/PastCastle` (it was the future base, so the owner could walk it): the game begins in the castle, as the GDD's opening sequence says, and reaches the base through the seal. The castle is in `MapsToCook`.
- `Glyphbound/Play-Castle.bat` and `Glyphbound/Play-FutureBase.bat` start the packaged game on either map; the second is for whoever is working on the base and its enemies.

### Pictures
`media/2026-11-14/castle_v1_tour_0.png` to `_3.png`: the 28 review spots through the game's camera, from the road to the seal (Editor build).

### Checks
- Editor target: `Result: Succeeded` (88 s, then 54 s after the second change).
- Whole test suite: `65 run, 0 failed` (1.35 s of tests).
- `Smoke-Jam.ps1 -Map /Game/Glyphbound/Maps/PastCastle -Only tour -Offscreen`: `SMOKE tour block passed: 28 spots, 0 without floor`, `the seal's map /Game/Glyphbound/Maps/FutureBase can be opened`, no LogGB errors, no engine errors, 123 s.
- Builder: `walks: clear on 7 walks` (road to ward, wide stair to the keep's door, the breach, Front Hall to Main Hall, terrace to side yard, Main Hall down to the landing, landing to the seal's hall).

### Code review
Sol 6.1 (medium, read-only) through Codex CLI and the logged wrapper; prompt `Prompts/review_castle_slice1.md`. It was given the measured facts of every piece the builder relies on, and it recomputed the floor tiles left out over the crypt stair from the records (the same two tiles). **Two findings, both real, both fixed:**

| Finding | What it meant | Fix |
|---|---|---|
| The barricade of the orcs' stores in the side yard was placed at Z 0 | The yard's floor is 2.1 m up: its kegs, barrels and crates stood buried in the terrace. The tour's picture of that yard showed the wall and no stores, and the architect had read it as "not in view" | `barricade()` takes the floor's height |
| `HasTravelMap()` accepted any package that exists, not only a map | A seal pointed at a mesh's package would pass the tour's check, and then fail to travel after being spent | The asset registry is asked what the package holds: a package it knows must hold a world. (While the registry has not read the package yet, its existence is all there is to go on.) New module dependency: `AssetRegistry` |

It also confirmed from the engine's source that `FPackageName::DoesPackageExist` sees packages inside IoStore containers, which is why the seal does not use the final portal's file check.

Editor target after the fixes: `Result: Succeeded` (65 s). Builder after the fix: `walks: clear on 7 walks`.

### The package, and a pack material that does not compile
- **First package** (before the review's fixes): `BUILD SUCCESSFUL` in 181 s. `TeamTBD_Glyphbound.zip` is **2,668 MB** (it was 2,117 MB with the future base alone); `Package-Jam.ps1` reports it as a failure because the jam's limit is 1 GB. The owner set that limit aside on 2026-11-13; it has to be dealt with before the submission (the kits' textures cook at 4096 px).
- **Second package** (with the fixes): the packaged tour passed on 28 spots and the seal found its map inside the package, but the log had three engine errors: `Loading a material resource None with an invalid ShaderMap` (twice) and `Tried to access an uncooked shader map ID in a cooked application`.
  - **Cause**, from the cook's log: the pack's `MI_Stab_Algae` fails to compile in this project ("Error on property Normal" in two of its layer functions), and the cook says the default material will be used. Two gatehouse meshes use it on two trim slots each: `SM_Middle_Arch` and `SM_Front_Top`.
  - **First fix, not enough:** the builder replaced it on the placed pieces. The mesh assets still named it, so the cook would still have taken it.
  - **Fix:** `Tools/Jam/fix_castle_materials.py` replaces it in the two mesh assets themselves with the pack's plain slab stone (`MI_Slabs_03`): 2 meshes, 4 slots. The builder keeps its own replacement for any piece that names a material of that list.
- **Third package, the one on disk:** `BUILD SUCCESSFUL` (89 s), `TeamTBD_Glyphbound.zip` **2,621.6 MB**. On it: `Smoke-Jam.ps1 -Packaged -Map /Game/Glyphbound/Maps/PastCastle -Only tour -Offscreen`: `SMOKE tour block passed: 28 spots, 0 without floor`, `the seal's map /Game/Glyphbound/Maps/FutureBase can be opened`, no LogGB errors, **0 engine errors**, 110 s. The cook's log no longer names the material. The 28 screenshots taken on the package: `media/2026-11-14/castle_v1_packaged_0.png` to `_3.png` (the side yard's stores are in the picture now).

### Not checked, and left open
- **Nobody has walked it by hand.** The tour teleports: it does not prove that the breach, the stairs or the low doorway under the keep's back wall feel right on foot.
- **The seal's travel itself was not run.** The check proves the future base's map can be opened from the package; using the seal, the caption and the arrival in the base are for the owner's walk.
- **The opening fight was not seen with its enemies** (the tour removes encounters before it starts).
- **The full arena smoke was not run** in this block: the game's code changed only in the seal's new, opt-in path and in one tour check.
- **The package is 2.6 GB.** The jam's limit is 1 GB.
- The keep's other doors are open; pickups and the pixel-art look are not done (plan, section 10).

---

## Fourth part: the owner's first walk of the castle
The owner played the package and sent eight screenshots with eight notes (Spanish):

> *"1. Arregla el hueco en la muralla al lado de la entrada. 2. La carreta esta flotando. 3. El ultimo escalon de la entrada funciona como pared y no como escalon. 4. Quita esa estructura marron oscuro. 5. Pegalo a la pared para que se pueda subir al muro. 6. Esa parte del piso esta rara, el piso deberia continuar y ya. Eso es en la entrada a la derecha. 7. Revisa los lados de la puerta, completa ese hueco con el piso. 8. Revisa que por ninguna ventana se pueda pasar."*

Translation: (1) fix the gap in the wall beside the entrance. (2) The cart is floating. (3) The last step of the entrance works as a wall, not as a step. (4) Remove that dark brown structure. (5) Put it against the wall, so that one can get up onto the wall. (6) That part of the floor is odd, the floor should simply go on; at the entrance, on the right. (7) Check the sides of the door, fill that hole with floor. (8) Check that no window can be passed through.

And, while the fixes were being made, a ninth with a screenshot taken in the Editor:

> *"las torres de los pasillos de la muralla están bloqueados por la ventana con un pedazo de pared. Le pasa a ambos"*

Translation: the towers of the wall's walkways are blocked by the window with a piece of wall. It happens to both.

| Note | What was wrong | What was done |
|---|---|---|
| 1 | The front wall stopped at the widest point of the gatehouse's round towers, and a round tower leaves a slit there | The front wall runs 1.8 m into the towers on both sides of the gate |
| 2 | **The whole lower ward floated, not only the cart.** The cobble piece's stone tops are 3 to 9 cm under its own pivot (measured from its vertices), and the pieces were set 12 cm down: the stones lay 10 to 20 cm under the plane the cart, the kegs and the player stand on | The cobbles come up 9 cm: nine tenths of the stone tops are now within 2 cm of that plane |
| 3 | The retaining wall ran on behind the wide stair. Its face stands 1 m in front of the terrace's edge, in the middle of the stair's run, where the stair is 1.4 m up: the last 70 cm were a wall | No retaining wall behind the stair (the stair is the wall there) |
| 4 | The portcullis the architect had put across the wall stair's foot to shut it | Removed: the wall stair is open |
| 5 | The author's wall stair stood 1.08 m off the wall (a guess at where its wall side is). And its landing meets the wall's inner parapet, 54 cm thick and 1.34 m high, not the wall walk | The stair stands against the wall. First answer: a mounting step of the kit's stone blocks over the parapet (measured with traces: landing 1210, walk 1203, merlons 1344). The owner asked for better, and the pack had it: see "Note 5 again" below |
| 6 | The upper ward's paving stopped where the architect thought the Front Hall began. The hall is narrower: a bare strip was left along its walls, with a white cube on it, a leftover unit cube that the author's level holds at its origin | The paving runs on under the keep. Unit cubes of the engine at a level's origin are no longer placed (four in the keep's levels) |
| 7 | The same gap, at the two sides of the door | The same paving |
| 8 | **Far worse than a window: 207 wall pieces of the keep blocked nothing at all.** The builder's list of things that never block had `Window_A_0`, meant for the pack's small loose window frames; it also matched `SM_CastleTower_Window_A_01` to `_09`, the walls that have a window | The entry is gone. And a new stage, `windows`, answers the owner's request as asked: every wall piece with a window (350) is crossed by straight lines on a 15 cm grid, from both sides; wherever a line goes through (184 pieces), an unseen blocking pane is set in the opening (239 panes, most 40 x 160 cm, the largest 80 x 100); then every piece is crossed again and must stop every line. Result: `still open after that: 0` |
| 9 | Three faults in the towers the wall stair now leads to. The two towers at the keep's end stood 3.5 m inside the wings' front wall: a piece of the King's Hall's wall, window and all, filled the tower's room. At the front corners the front wall ran through the corner tower and its parapets crossed the wall walk. And a tower's doorway, 1.45 m of wall thickness, had no floor | No wall runs through a tower any more: each run ends at a tower's face (42 wall pieces, were 50). The keep-end towers stand against the wings' wall, not in it. A stone threshold in each of the eight doorways |

**Note 5 again: the stair belongs to the wall.** The owner saw the render of the mounting step and wrote:

> *"Acomoda esa escalera que quede naturalmente integrada a seguir hacia el muro de la muralla. Por ejemplo, los dientes que tiene la muralla a ambos lados, quítalos por el lado de la escalera."*

Translation: arrange that stair so that it goes on naturally onto the wall. For example, the teeth the wall has on both sides: take them away on the stair's side.

- A mesh's teeth cannot be taken off, but the pack has the piece for it. The author's map has three wall stairs and exactly three pieces of `SM_Curtain_Wall_02`, and the first reading of the kit had not asked why. Its top was read cell by cell from its vertices beside the plain piece's: **the same wall, with no teeth on one side over 2.5 m (its own X 100 to 350) and a sill there at 1214**, the height the author's stair arrives at (1215).
- That piece now stands in the wall where the stair arrives, turned so its open side faces the ward, and the stair moved 1.6 m along the wall so its landing lies under the opening. The mounting step is gone. From the landing the player walks onto the sill and steps 11 cm down onto the wall walk.
- New walk in the builder's check: from the landing through the opening onto the walk. `walks: clear on 11 walks`.
- Screenshots through the game's camera: `media/2026-11-14/castle_notes_wall_stair.png`, `castle_notes_wall_stair_landing.png` (from the landing: the walk straight ahead, teeth only on the far side), `castle_notes_wall_walk.png`.

**A mistake on the way.** The architect lowered the four towers by 50 cm on a guess about their floor tile's thickness. The builder's walk check, which now goes along both wall walks from inside one tower through the other, reported the floor 50 cm under the walk; the towers went back up. The same check then found the missing floor in the doorways.

**Not placed from names.** The wall's cross-section (a walk 2.1 m wide at 12.0 m between two parapets of 1.6 m), the two stairs' direction of rise, the cart's wheels, the cobbles' height distribution and each window wall's direction (the towers' pieces run at 45 degrees in their own frame) were all read from the meshes' vertices or from traces in the level.

**What the wall walk is now.** Up the wall stair on the King's Hall side, over the parapet, and along the wall: to the keep-end tower (a room; its far door faces the wing's wall, where a door into the King's Hall gallery belongs in a later slice) and to the front corner tower (a room; its far door opens on the outside of the castle, 12 m up). The front wall and the other side wall are not reached from it.

**Checks**
- Builder: `walks: clear on 10 walks` (three new: both wall walks from tower to tower, and across the terrace in front of the keep); `windows: ... still open after that: 0`.
- `Smoke-Jam.ps1 -Map /Game/Glyphbound/Maps/PastCastle -Only tour -Offscreen`: `SMOKE tour block passed: 41 spots, 0 without floor`, no LogGB errors, 0 engine errors, 186 s (Editor build). Fourteen new spots at the places of the nine notes; their screenshots were looked at one by one.
- No C++ changed in this part.
- After "Note 5 again": builder `walks: clear on 11 walks`; tour `passed: 42 spots, 0 without floor` on the Editor build (244 s).
- **The package on disk** has all of it: `BUILD SUCCESSFUL`, `TeamTBD_Glyphbound.zip` 2,628.6 MB (still over the jam's 1 GB). On it: `Smoke-Jam.ps1 -Packaged -Map /Game/Glyphbound/Maps/PastCastle -Only tour -Offscreen`: `SMOKE tour block passed: 42 spots, 0 without floor`, the seal's map can be opened, no LogGB errors, 0 engine errors, 165 s.
- **Still not checked by a person:** the corrected places on foot (the tour teleports), the seal's travel itself, the opening fight with its enemies.

**Pictures of this part** (`media/2026-11-14/`, through the game's camera): `castle_notes_gate_wall.png` (no slit beside the gate), `castle_notes_cart.png`, `castle_notes_wide_stair.png`, `castle_notes_wall_stair.png`, `castle_notes_wall_stair_landing.png`, `castle_notes_wall_walk.png`, `castle_notes_keep_end_tower.png` (the tower's room, clear).

---

## Fifth part: the way down to the crypt, and a pixel-art test in the first zone
Two messages from the owner (Spanish), the second with a screenshot of the crypt stair's mouth:

> *"Prueba pixel art solo en la primera zona"*

> *"No se puede ir al sotano, arreglalo cuando puedas"*

Translation: try pixel art only in the first zone. / One cannot get to the basement; fix it when you can.

### The crypt could not be reached, and the checks said it could
- **What was wrong.** The stair's first flight went down under the Main Hall's back wall (X 10090). Where it passed, the wall's foot stood **1.3 m above the steps**: a player 1.8 m tall stops there. Version 1 called it "a low doorway, about 2 m"; nobody had measured it with a body.
- **Why two checks passed it.** The builder's walk check drew a line at chest height along each route and sampled the floor under it. The line ran under the wall's foot, and the floor was there. The tour teleports the player to each spot, so it never tried the doorway either. A check that could not fail on the fault it was written for (CLAUDE.md, tests, rule 3).
- **The new check: `body_walk`.** A capsule of the player's size (radius 38 cm, 180 cm tall, less the 45 cm he steps up) is swept from sample to sample, every 30 cm, on the floor found under it. It reports what stops the body at any height (a lintel, a kerb above a step) and any rise in the floor higher than a step. It runs on every walk now, in both directions on the crypt's (a twelfth walk: back up from the seal's hall to the Main Hall).
- **The fix.** The stair's mouth moved 4 m toward the middle of the hall (the four floor tiles left out are now X 9290 to 10090, were 9690 to 10490), so the stair is 3 m under the back wall's foot when it reaches it, and 3.6 m under the keep's outer wall behind it. The second flight has plain side walls and a ceiling where it leaves the keep, and a wall closes the space over the stair's mouth at the landing; the rock around the crypt starts further back.
- **Checks.** Builder: `walks: clear on 12 walks`; `windows: ... still open after that: 0`. Tour on the Editor build: `SMOKE tour block passed: 42 spots, 0 without floor`, no LogGB errors, 0 engine errors, 192 s. The landing's two dark slivers in the game's picture were looked at from six angles in unlit renders: floor and kerb, no hole.
- Pictures (the game's camera): `media/2026-11-14/castle_crypt_mouth_moved.png`, `castle_crypt_stair_flight1.png`, `castle_crypt_landing.png`.
- **Still not done by a person:** walking down it. The body sweep is the nearest thing to feet the builder has; it is not feet.

### Pixel-art test: the first zone, two variants side by side
The zone is the gatehouse and the lower ward (everything of the map up to the upper ward's edge: 727 pieces). The rule being tested is the GDD's (section 3): *pixel-art environments are 3D geometry with low-resolution pixel textures, nearest filtering*. The castle pack is the opposite: photographic stone on sculpted geometry. Two ways to get from one to the other were built, one on each side of the map's axis, so the owner sees both from one spot:

| | **A, drawn tiles** (left of the road, the King's Hall side) | **B, the pack's own pictures in large pixels** (right of the road) |
|---|---|---|
| What it is | Eight seamless 32 x 32 tiles drawn for the castle (wall, dressed stone, dark stone, cobble, wood, iron, earth, cloth), in the style guide's ramps plus a new stone ramp; each piece's material is replaced by the tile that fits it, laid on the world by `M_PixelSurface` (the material the test arena already uses) | Each of the pack's materials keeps its own colour picture, cut down to 64 px and read without filtering on the piece's own UVs; its occlusion picture darkens it (the pack's stone has its joints there, not in its colour); the brightness is cut to 7 steps |
| Made by | `Tools/Jam/make_castle_tiles.js` (seeded rules, no hand drawing, no AI image model) | A new master material, `M_GBPixelUV`, and one instance per pack material (23) |
| The floor | The modelled cobbles are hidden and a flat floor carries the cobble tile | The modelled cobbles, with their picture in pixels |

Applied by `Tools/Jam/castle_pixel_test.py` (settings in `Saved/castle_pixel.json`: `tiles`, `uv` or `both_sides`). It only overrides materials on placed pieces of that zone; building the map again takes the test away.

**What went wrong on the way**
- *B, first pass: speckles of yellow, purple and green.* Cutting each colour channel to steps on its own makes near-grey stone round three different ways. Now the brightness is cut to steps and the hue is kept.
- *B, second pass: a wall with no joints.* The pack's bricks are in its occlusion and normal pictures, not in its colour. The occlusion picture, in the same large pixels, now darkens the colour.
- *B, third pass: the material did not compile*, and the whole side rendered as the engine's grey checker. The pack's 4096 px pictures are virtual textures, which need their own kind of sampler; one material cannot serve both kinds. The fix is also the simpler design: a copy of each picture cut to 64 px, not virtual, no filtering, no mips (`small_copy`). The copy **is** the large-pixel picture; the UV-snapping maths went away.
- *A: a drawn cobble on a modelled cobble is two floors at once.* On A's side the modelled cobbles are hidden and a flat floor carries the tile.

**The architect's own reading of the pictures** (the game's camera; `castle_pixel_ward_A_left_B_right.png`, `castle_pixel_gate_A_left_B_right.png`, `castle_pixel_A_drawn_tiles_wall.png`, `castle_pixel_B_pack_pictures_wall.png`, unlit `castle_pixel_unlit_sheet.png`, the tiles `castle_pixel_tiles_preview.png`):
- **A's floor is the one thing that is plainly pixel art**: flat, drawn, crisp at every distance, and it sits well under the pixel HUD and the pixel hands.
- **A's walls read as pixel bricks from the middle of the ward and further.** Close to the gatehouse's rough stone they do not: the drawn joints and the modelled stones are two patterns that do not agree, and the low sun lights the modelled one.
- **B is not pixel art.** It reads as the same sculpted castle with a blurry texture swapped for a blocky one. The geometry and its shadows carry the image, the pixels only colour it. Unlit, B's walls are noise: a 64 px cut of a photograph of stone has no drawn shapes in it.
- **Neither variant touches the real obstacle: the pack's relief is in its geometry.** A material cannot flatten it. A wall that is truly pixel art here is a flat wall with a drawn tile, as A's floor is.

**Recommendation to the owner (his decision, not taken):** A, and where a surface is simple (floors, the curtain walls, the retaining wall, the wide stair) replace the sculpted piece with a flat one that carries the tile, as was done for the cobbles; keep the pack's shapes where the shape is the point (gatehouse, towers, keep) with A's tiles on them. B is dropped unless he sees something in it the architect does not.

**State of the map.** The test is left in the map and in today's package, both variants side by side, so the owner can walk it (`Glyphbound/Play-Castle.bat`). The rest of the castle is untouched.

**Checks of this part**
- Tour on the Editor build with the test applied: 42 spots, 0 without floor, 0 engine errors (the same run as above).
- Package: `BUILD SUCCESSFUL` (BuildCookRun 178 s), `TeamTBD_Glyphbound.zip` 2,628.8 MB. Still over the jam's 1 GB limit; the test adds 0.2 MB.
- Packaged tour: `Smoke-Jam.ps1 -Packaged -Map /Game/Glyphbound/Maps/PastCastle -Only tour -Offscreen`: `SMOKE tour block passed: 42 spots, 0 without floor`, the seal's map can be opened, no LogGB warnings or errors, 0 engine errors. Both variants are in the packaged pictures (the test's materials cooked).
- No C++ changed in this part. Not reviewed by Sol: the changes are Editor scripts (the builder's check and stair, the test's two tools), checked by their own output and by the pictures.

**Tools used in this part:** Unreal Editor 5.9 Python over MCP; Node.js (tile drawing, patches); no image model.
