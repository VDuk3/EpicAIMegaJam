# 2026-11-14, block: future base, the owner's fourth walk (six notes: fixes, a breakable wall, the deep wing, the energy gate and the Valkyrie's room)

Architect: Claude Code (Fable 5.2) on CENTRAL. Owner's notes in Spanish, translated here. Continues `2026-11-14_future_base_lab_boss_block.md`.

## What the owner sent
Six screenshots from the package and six notes:

> 1. que funcionen como escaleras. Ahora mismo funcionan como pequeños muros.
> 2. La caja de abajo se solapa con la de arriba y la tapa naranja de la caja de abajo se ve en el fondo de la de arriba. Solucionalo como sea más sencillo.
> 3. La sala nueva muy bien pero unas correciones. Las esquinas todas tienen gaps entre paredes.
> 4. hay gaps entre las cajas, todas tienen ese problema.
> 5. En ese panel oscuro, después de esas escaleras, nos gustaría que tal vez esa pared fuera rompible. [...] con unos 3 o 4 golpes. Si hay algún material para mostrar cómo se va quebrando mejor [...] Y que ya diera como a un lugar submarino. Aquí podríamos usar los assets del Deepwater Station para hacer unas escaleras hacia abajo. Ahí habría otro pasillo, pero este con falla de luz y un poco oscuro. Aquí vamos a usar los assets del pasillo que está como destruido, desgastado, que tú arreglaste [...] con una o dos luces nada más parpadeando, así como un poco tenebroso. Después de ese pasillo va a haber una pequeña sala que va a estar infestada, aunque bueno, tú no vas a poner los enemigos. Y después de esa sala, un pasillo completamente oscuro que más adelante vamos a poner lleno de humo. Y al final de ese pasillo, que va a ser mediano, una sala grande, también con barandas alrededor, pero no completo. Y si tienes por ahí de todos estos paquetes de assets de cosas que estén dañadas o malogradas, ahí va a haber el boss de los tentáculos. Eso va a ser un punto muerto [...]
> 6. En esta sala donde antes estaban todos los assets del laboratorio, después de subir la escalera que vas a arreglar, en esa pared hay un asset, quítalo. Y pon una puerta que encaje estéticamente con esa pared y esa sala. Esta puerta tiene que ser como de energía, holográfica, porque es una puerta que se va a desactivar poniendo dos objetos que van a dropear los enemigos en los dos objetos dispositivos horizontales que están ahí, que tienen la bola blanca brillante. Entonces, más adelante, en donde sí vas a utilizar la habitación con todos los assets de laboratorio, si puedes poner los materiales un poco como corroídos o destruidos, más como si hubiera habido una batalla que como si estuviera abandonado, ahí va a estar la Valquiria y se tiene que ver aunque el jugador no haya abierto la puerta [...]

Translation: (1) The steps must work as stairs; right now they work as small walls. (2) The lower crate overlaps the upper one and its orange lid shows in the bottom of the upper one; solve it the simplest way. (3) The new hall is very good; all its corners have gaps between walls. (4) There are gaps between the crates, all of them. (5) That dark panel after those stairs should be breakable, in three or four hits, with something to show it cracking if there is a material for it. It leads to a place that is already under water: stairs down, perhaps with the Deep Water Station assets; a corridor with failing light, a little dark, using the destroyed, worn corridor asset, worn, with one or two lights flickering, a bit scary; then a small room that will be infested (no enemies placed by the architect); then a completely dark corridor, of medium length, later full of smoke; and at its end a large hall with galleries around it but not all the way, with whatever damaged or broken assets there are: the tentacle boss's. A dead end. (6) In the lab room, up the steps, take the asset off the end wall and put a door that fits that wall and room: an energy, holographic door, which will be switched off by putting two objects that enemies drop into the two horizontal devices with the glowing white ball. Behind it, the room with all the lab assets, its materials somewhat corroded or destroyed, as after a battle rather than abandoned: the Valkyrie will be there and has to be seen even with the door shut, to wake the player's curiosity at the start.

## 1 to 4: the fixes, each with its cause
| Note | Cause, measured | Fix |
|---|---|---|
| Steps act as walls | The lab kit's step piece is one closed block: its riser is a single 50 cm face, higher than the player's step | An unseen slope over the three steps (`ramp()`), and the step meshes no longer collide. The builder's walk check now also compares the floor's height with the walk's, which is how the slope's direction was confirmed |
| The corridor's crates | In the `.blend` the upper crate is the lower crate's mesh scaled up 1.5 times, left open and sunk 1 cm into the lower one | The upper crate is left out of the export. One closed crate stays |
| Gaps in the hall's corners | The kit's wall module is set back 19 cm below its top band (read from its vertices), so two walls that meet leave an open slot down the corner. One of the architect's wall lamps was also hanging in a corner | The kit's own corner pillar (1.42 m) in every corner, two tiers; the lamp is gone |
| Gaps between crates | The crate is exactly 100 cm from its flat underside to its rim and 102 cm wide (read from its vertices). The architect had stacked them 105 cm apart, and 150 cm at the 1.38 size (which is 138 cm), and stood neighbours 110 cm apart | One helper, `crate()`: a crate on another is one height up, crates that touch are one width apart. Every crate of the map goes through it (pits, transfer bay, store, sub-boss hall) |

## 5a. The breakable wall
The "dark panel" is the kit's hangar shutter at the transfer bay's east end, a flat grate 10 x 5.6 m. It is now an `AGBBreakable` (new, `World/GBBreakable`): a neutral thing with attributes, like the ice wall, so shots stop on it and any damage reaches it. **It counts hits, not damage:** four hits break it, whatever each deals; hits less than 0.25 s apart count as one, so a shotgun blast or a flame is not four. With each hit dark cracks spread over it: an overlay material (`M_GB_Cracks`) shows what is brighter than a threshold in a crack mask whose lines start bright at five points and darken towards their tips, so the cracks grow outwards as the threshold drops. The mask is drawn by `Tools/Jam/Make-CrackTexture.ps1` (System.Drawing, fixed random seed; no source image). When it breaks it sets the progress flag `FutureBayShutterBroken` and stays broken.

## 5b. The deep wing (stage `deep`)
Behind the shutter, 6 m under the base's floor, heading east. All of it from the base's kit except where named.

| He asked for | Built |
|---|---|
| stairs down, to a place under water; "we could use the Deep Water Station assets" | A stairwell 16 x 12 m: a landing at the base's level, a 4 m wide stair down its middle (two flights of the catwalk's stair module side by side), the lower floor 6 m down. **The pack was installed** (below), but it has no stair, only ladders, which the player cannot climb. From it: six portholes with the glow of deep water behind them, four of its tube lamps, two racks of divers' gas tanks, shellfish and stones on the floor |
| a corridor with failing light, the worn corridor asset, one or two lights flickering | The Spaceship Corridor model a second time (40 m), with worn materials: `M_ShipCorridor_Worn` lays dirt and rust over the paint from two noise textures projected on the world (the model has no unwrapped UVs and its author's wear is procedural in Blender). Its light strip is dead; two lamps flicker (a light-function material, the same rhythm on the model's own small lamps); its windows give a faint deep-sea glow; its screens are off. Its far door opens like the first corridor's |
| a small room that will be infested | 12 x 12 m, 6 m high, red emergency light, a few crates and drums, grime on the floor, the first shellfish. No enemies |
| a completely dark corridor, medium | 25 m, 4 m wide, 3.5 m high, no lamp at all. The light at its far end is the hall's |
| a large hall, galleries around but not complete, damaged things, the tentacle boss, a dead end | 36 x 28 m, 12 m high, no exit. Galleries 4 m up along the south wall (stops half way, its last 6 m without railing), a stub in the west, and along the north and east walls joined; the deck tiles and railings that came off lie on the floor under the torn ends. In the middle, off centre, a heap of castle stone (the castle pack's rubble and a broken pillar top) under a cold shaft of light: the castle above has come through the roof. Crates thrown about, one pile still standing up to the south gallery, scorch and grime marks, one lamp steady and one failing |

**Exposure.** The first look in the game showed the corridor's windows and its "dead" strip blown out white: the base's exposure setting brightens whatever it finds in a dark place. The wing has its own post-process volume, two stops under the base's, and its lamps are about four times a base lamp to light the same. After that, three short tours on only the wing's spots (a filter in the builder, `TOUR_ONLY`) to set the lamps.

**Third-party asset installed:** Deep Water Station, by Kyrylo Sibiriakov, from Fab (listing id `ec2385d7-3e02-494b-a243-e18cac7f4a69`), Professional license, from the owner's library; installed through the Editor's Fab toolset (the first request showed nothing on disk for 14 minutes and was lost when the Editor was closed to compile; the second finished in about 5 minutes: 388 assets, 7.8 GB in `Content/DeepWaterStation`). Used: `SM_WinMetal02`, `SM_Lamp05`, `SM_GasCylinderA02`, `SM_GasCylinderA03`, `SM_Mollusk02`, `SM_RockSet05`. Also used for the first time: the castle pack's `SM_Rubble_02` and `SM_PillarTop_Broken`, and the impact pack's scorch and grime decals.

## 6. The energy gate and the Valkyrie's room
- **The asset on the end wall** was the cabinet the architect had put there in place of the specimen tube. It is gone, and so is the 7 m wall panel behind it: in their place the lab kit's own gate frame with a narrow panel each side.
- **The gate** (`AGBEnergyGate`, new): a sheet of energy across that frame. It blocks bodies and shots while any of its flags is missing, and what is behind it stays in sight. The hologram pack's materials turned out to be opaque clouds, so the sheet has its own (`M_GB_EnergyGate`): faint blue with slow brighter bands, 5 to 19% opaque. With both flags set it fades out in 1.2 s.
- **The two devices** with the glowing ball are the dais's two control panels. On each stands an `AGBKeySocket` (new): the player only has to come within 1.8 m carrying a **Power Cell** (new key item in `Items.json`; name, text and icon provisional); the cell is used up and the socket's flag is set. Without one, the HUD says once per approach that the socket needs a power cell. **Nothing drops power cells yet**: that is the enemies' side, to do with them.
- **The Valkyrie's room** (stage `valk`): the lab kit's room again, whole this time (figures, holograms, tubes), starting at its inner doorway 1.2 m behind the gate, on the same axis, so the big tube against its far wall is straight ahead of whoever looks through the gate. That tube is hers: the alien that floated in it is taken out and its glass put back; a cold light makes it the brightest thing in view. After a battle: the room's own glow is down to a fifth (an instance of each of its materials), twelve scorch marks and four patches of grime, every other chair and every third barrel knocked over, red emergency lamps, one failing. **The Valkyrie herself is not in the map**: she has no model yet.

## Checked in the game, with checks that can fail
The tour (`gb.Smoke tour`) gained three checks, named after the spot they run on:
- `breakwall`: deals the nearest breakable four hits 0.4 s apart and fails unless it is gone at the fourth;
- `gatetest` (two spots, one by each socket): hands the player one power cell and fails unless the socket took it;
- `gatecheck`: fails unless the energy gate is out after both.
Both leave nothing behind: the shutter's flag is taken off before it is broken, and the sockets' flags are cleared after the check.

The pure rules have tests (`Glyphbound.World.BreakableHits`: a weak hit counts like a strong one, a second pellet of the same blast does not, cracks never past full; `Glyphbound.World.EnergyGateFlags`: one of two flags is not enough, another flag does not stand in for a missing one, a gate without flags never goes out).

## Exception to the implementer rule
The architect wrote the C++ directly (two new actors and the tour checks, about 450 lines with tests). The map could only be built with those classes compiled, and the Editor was open for the rest of the work, so an implementer could not have compiled in parallel; both actors follow patterns already in the code (the door, the ice wall's health).

## Code review
Sol 6.1 (medium, read-only, through the logged wrapper; prompt `Prompts/review_breakable_gate_wing.md`), five findings, all fixed:
1. The breakable's health still went down with every hit: one enormous hit could kill its attributes and leave it unbreakable. Its health is topped up after every hit.
2. It destroyed itself inside the damage call that broke it, while the caller (a projectile) was still using it. Now it is hidden and stops blocking at once and is removed 0.05 s later (the ice wall already did it this way).
3. The tour's `breakwall` check passed with no breakable at all, or with one that broke at the first hit. It now needs one to be found, whole and with three hits counted before the fourth, and gone after it.
4. The `gatetest` check only saw that the item was gone. It now also needs the nearest socket filled and its flag set.
5. The sockets' flags were cleared only at the `gatecheck` spot. They are cleared when the tour ends too, so a filtered tour leaves no gate open.

## Checked
- Editor target builds; **52 tests green** (two new).
- **Tour: 71 spots pass** on the Editor build, with all four checks (ice wall, shutter, two sockets, gate), before and after the review's fixes.
- The architect's own looks at the screenshots, and what changed after each: the hologram pack's material hid the room behind the gate (own material, then thinner); the wing's windows and dead strip were white (own exposure, lower glow); the stairwell's landing was black (lamps four times brighter and one over the stair). Two things seen and left: the gas tank racks are large and flank the way to the worn corridor (3.5 m between them); the worn corridor's wear is dirt and rust blotches, not the chipped edges of its author's Blender material.
- **Package:** `TeamTBD_Glyphbound.zip` is **1,954.9 MB** (was 1,330.8 MB). The 624 MB more are the Deep Water Station pieces with their 4096 px textures and the second lab room's materials. It is almost twice the jam's 1 GB; the owner set the limit aside for now, and this step made it worse: before the delivery the kits' textures have to be capped (the 2048 px cap script is half done) or the pack's use cut. Packaged tour: 71 spots pass with the four checks; packaged full arena smoke clean; the runs leave no save file in the package's folder.
- Nobody has played it by hand yet.

## Media (all from the package)
- `media/2026-11-14/futurebase_deep_wing.png`: the shutter whole and broken, the stairwell from below, the worn corridor, the small room, the dark corridor, the tentacle boss's hall from the middle and from its far end.
- `media/2026-11-14/futurebase_energy_gate_valkyrie_room.png`: the lab's steps, the gate shut (her tube in sight through it), the gate out after both sockets, the Valkyrie's room and her tube.
- `media/2026-11-14/futurebase_corners_and_crates_fixed.png`: the sub-boss hall's corners and its crates, flush; the corridor's one crate.

## Left open
- What drops the Power Cells, and from which enemies (the gate cannot be opened in a normal game yet).
- The Valkyrie's model and fight; the tentacle boss (which boss it is) and its fight; the sub-boss; the enemies of the small room and of the wing; the smoke of the dark corridor.
- The package's size.
- The crack overlay is laid over the world's Y and Z axes: right for this shutter, which stands across X; a breakable wall facing another way needs the other projection.
