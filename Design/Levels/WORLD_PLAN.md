# World plan: one site, two eras

> Written by the level planner (Claude Fable 5.2) on 2026-11-13 for the architect, who builds it in Unreal.
> Planning only: nothing was created or changed in the Editor. Sizes marked "measured" were read from the assets through the Editor's MCP server (asset registry, mesh bounds, vertex positions, simple collision).
> Brief: `Prompts/world_plan_fable.md`. Design source: `Design/GDD.md` sections 4, 6, 10, 11.

## Files in this folder

| File | What it is |
|---|---|
| `WORLD_PLAN.md` | this document |
| `plan_past.svg` | top-down plan of the castle |
| `plan_future.svg` | top-down plan of the base, same scale and origin |
| `plan_overlay.svg` | both eras on one sheet |
| `section_side.svg` | three side sections at true scale |
| `world_layout.json` | every number of the plan, for build scripts: rooms, doors, wall pieces, collision shell, structures, portals, encounters, spawns, pickups, chests, checkpoints |
| `make_plans.js` | the source of the numbers. `node Design/Levels/make_plans.js` checks the layout and rewrites the JSON and the four SVG files |
| `castle_keep_list.txt` | the 101 `CastleTown` packages to keep; everything else in that folder can go |

Change a number in `make_plans.js`, run it, and the JSON and the drawings follow. Its checks fail the run if a spawn point sits in a wall or on stairs, a portal is not clear in both eras, a structure blocks a doorway, two structures overlap or leave less than 2 m between them, a free-standing piece is closer than 3 m to a wall, or an arrival through a portal would start a fight at once.

## 1. Overview

**The site.** A square castle of nine blocks around a courtyard, and a military research base built on its walls centuries later. The base exists because of what is under the castle: the seal, and the Valkyrie frozen beside it. The castle's courtyard is the base's yard, the Great Hall is the Containment Hall, the two wings are the Generator Hall and the Server Hall, and the gatehouse is the checkpoint. Beyond the gate, where the past has nothing, the base has a damaged corridor and a hangar.

```
                         north (+X)
                        [ SEAL 16x16 ]
 [ NW 32x32      ] [ HALL 48x32          ] [ NE 32x32       ]
   Frozen Cistern    Great Hall              High Ward
   / Cryo Corridor   / Containment Hall      / Signal Corridor
 [ WEST 32x48    ] [ COURT 48x48         ] [ EAST 32x48     ]
   Stormworks        Courtyard               Forge
   / Server Hall     / Yard                  / Generator Hall
                        [ GATE 16x16 ]
                        [ BREACH 40.5 x 5.3 ]   future only
                        [ ANTE 4 x 36 ]         future only
                        [ HANGAR 48x48 ]        future only
```

**Critical path** (recommended order; the two wings can be done in either order, and so can Ice, Air and the Herald). "P" is the past, "F" the future.

| # | Era | Where | What happens |
|---|---|---|---|
| 1 | P | Gatehouse | Start. The portcullis is down behind the player |
| 2 | P | Courtyard | `Opening_Past`: a small group of pixel-art orcs. Clearing it drops the ward on the keep door |
| 3 | P | Great Hall, Seal Room | Walk past the Valkyrie in her rune cage (she cannot be freed yet). Interact with the seal: cutscene 1, thrown to the future on the spot |
| 4 | F | Seal Vault, Containment Hall | `Opening_Future` around the Valkyrie's cryo tube. The first armed orc drops the pistol; picking it up plays cutscene 2 and lights portal P1. The hall's other door is in lockdown, so P1 is the only way on |
| 5 | F to P | Seal Vault | Portal P1 back to the castle. The seal's discharge has broken the wards on both wings |
| 6 | P | Forge | `Zone_Fire`. Chest: fire knowledge |
| 7 | P to F | Forge | Portal P2 |
| 8 | F | Generator Hall | `Wave_Future_2`. Its door to the Signal Corridor is sealed by a lightning rune (cross-era unlock U1) |
| 9 | F to P | Yard | `Wave_Future_1`. Lights the hub portal P0 and opens the checkpoint |
| 10 | P | Stormworks | `Zone_Lightning`. Chest: lightning knowledge. This opens the sealed door of step 8 |
| 11 | P to F | Stormworks | Portal P3 |
| 12 | F | Server Hall | `Wave_Future_3`. Its door to the Cryo Corridor is already open, because the Forge was cleared (U2) |
| 13 | F | Checkpoint, Breach Corridor, Hangar | `Skirmish_Breach` (corridor fight), then the Herald, the tentacle machine (`MiniBoss_Herald`). Its death lifts the lockdown |
| 14 | F to P | Server Hall, Cryo Corridor | Back through the yard and the Server Hall to door D9f. Portal P4 |
| 15 | P | Frozen Cistern | `Zone_Ice`. Chest: ice knowledge. The barred door to the Great Hall opens |
| 16 | P to F to P | Courtyard, Generator Hall, Signal Corridor | Through D7 and the hub portal P0 to the yard, then the Generator Hall and door D8f. `Skirmish_Signal` (corridor fight), portal P5 |
| 17 | P | High Ward | `Zone_Air`. Chest: air knowledge (double jump). The barred door to the Great Hall opens |
| 18 | P | Great Hall | The four rune pylons are gone. Free the Valkyrie: `Valkyrie_Past` |
| 19 | P to F | Seal Room | Portal P1 |
| 20 | F | Containment Hall | Release the cryo tube: `Valkyrie_Future` |
| 21 | F | Seal Vault | Combine the two quest items: the final portal opens on the seal, to the Entity's map |

**Play time (estimate).** Fights 14.7 min (the sum of the lengths in GDD 19 for the encounters used, plus 20 s and 15 s for the two corridor fights and 75 s for the Herald). The walking legs drawn on the plans add up to 1,150 m, 2.5 min at full speed. With chests, the inventory, the skill tree and two cutscenes, a first run without deaths is about **22 to 25 minutes**; a player who knows the way, about 17. That is above the 10 to 15 minutes of the compact version. The cheapest cuts are listed in section 9.

## 2. Shared coordinate frame

- **Origin:** the centre of the courtyard floor. **+X = north** (towards the keep), **+Y = east**, **+Z = up**. On the plans north is up, so world Y runs left to right and world X bottom to top.
- **Units:** centimetres. A rectangle is written `X0..X1 x Y0..Y1`.
- **Z 0 is the main floor of both eras**, everywhere. All portals stand on it.

**Floor levels (cm)**

| Level | What | Era |
|---|---|---|
| 0 | main floor of every room, both eras | both |
| +100 | loading docks (HTB `SM_Hero_Floor_01` is 100 high) | future |
| +120 | the hearth of the Forge | past |
| +200 to +300 | cover: low walls 203, bookshelves 201, machine blocks 228, containers 260 (yard) and 300 (hangar), ice blocks 200 | both |
| +210 | galleries and the courtyard stage (`SM_Stairs_4M` rises 209) | both |
| +282 | hangar gantry (HTB `SM_Bridge_01` deck) | future |
| +410 | High Ward terrace and the courtyard perch (`SM_Stairs_8M` rises 409) | past |
| +610, +700, +860 | wind towers, the aerie and the spire on the terrace | past |
| 1500 | castle ceilings and wall tops | past |
| 1200 | base ceilings and wall tops; 700 in the Seal Vault and the two corridors; 600 in the checkpoint and antechamber; 333 in the Breach Corridor; 2400 in the hangar | future |

**Grids (from the measured module sizes)**

| Kit | Grid | Why |
|---|---|---|
| Castle shell | **1600** | `SM_Wall_16x15` and `SM_Wall_Door_16x15` are 1600 long and 1500 high. Every room is a multiple of 16 m |
| Castle detail | **400** | floors 400 and 1200, wall panels 400, stairs 820 wide, low walls 205 |
| Base (High_Tech_Base, MaterialsScifi) | **400** | HTB walls are 400 x 600; boxes with the panel materials take any size |
| Lab kit (SciFiWorld) | **350** | walls 350 and 700 long, 350 high; floors 350 x 350 and 700 x 350; a pilaster 100 wide. Used only as a lining inside the Seal Vault and the two corridors, where panels and pilasters add up to the room (700 + 100 + 700 = 1500; 4 x 700 + 3 x 100 = 3100) |
| Breach Corridor | one piece of 4050 | the imported model, or four segments of about 1012 |

**How the two eras line up**

- **Same wall lines.** Both eras use the same wall centre lines: X = -4000, -2400, 2400, 5600, 7200 and Y = +-800, +-2400, +-5600. The base adds X = 3100 (the corridors' north walls) and the hangar wing.
- **Same hub doors.** D1, D2 and D5 (gatehouse, keep, seal) are at the same place in both eras. The wings differ: the castle has two doors per wing (X = -1600 and X = 1600), the base has one wide door at X = 0.
- **Shared structures** (`era: "both"` in the JSON): the 8 pillars of the hall and the 6 galleries with their stairs (hall, Forge, Stormworks). One collision body, two looks, as `Design/ARCHITECTURE.md` asks.
- **The north-east and north-west blocks** are whole rooms in the past and only a 7 m strip in the future (X 2400..3100). The rest is sealed mass in the future.
- **Elsewhere the eras differ**: cover, the courtyard platforms and the yard docks are per era. Travel only happens at portals, and the script checks that every portal is clear of every structure of both eras by 1.5 m.

**Collision shell (recommended).** `world_layout.json` has a `shell` block: one floor slab under the whole site, and per era the walls as boxes with the door gaps and lintels already cut (past: 23 walls and 10 lintels; future: 35 and 11). The future boxes are 100 thick, centred on the wall lines, and are also the visible walls. The past boxes are 125 thick and sit exactly where the kit wall sits (8 cm to one side of the line, 117 to the other), so what blocks the player is what the player sees. Build the level from those boxes first and place the kits as looks without collision. Reasons:

- The slab goes in the persistent level, so the floor under a portal never changes during a swap.
- Three kit pieces cannot give their own collision as they are (measured): `SM_Wall_Door_16x15` and HTB `SM_Door_02_Side` have **one convex hull each, which closes the doorway**, and HTB `SM_Bridge_01` has one hull, so it is solid under the deck.
- The navmesh is built from a few boxes instead of 23,000-triangle walls.
- The same boxes with a plain material are the greybox for the first playable slice (section 9).

If a kit piece keeps its own collision instead, set the two door pieces to "use complex collision as simple".

**Reach rules** (from `Player.json`: jump velocity 800, gravity scale 1.6, ledge climb 160 above the hands, minimum 50; step height 45 is the engine default)

| Move | Limit | Design value used here |
|---|---|---|
| Walk up a step | 45 | docks and platforms always get a ramp or stairs for the enemies |
| One jump | 204 | |
| Jump + ledge climb | 364 | **320**: anything up to 3.2 m is a normal climb (cover, containers, the gantry) |
| Double jump + ledge climb | 568 | **500**: the courtyard podium (507) and the spire (450 above the terrace) are the rewards meant for the double jump. A dash jump from the stage or from a wind tower also reaches them: a trick, left in on purpose |
| Barrier | | **650 or more**, or a ceiling. Every room wall is 1200 or 1500 |

Gaps are never used as barriers: a running jump with a dash crosses about 14 m.

**Open-sky rooms** (courtyard, yard, High Ward): nothing the player can stand on is higher than the wall top minus 600 (900 in the castle, 600 in the base), so a double jump and a ledge climb from the highest perch still fall short of the wall top. The script checks it. The highest perch is the spire at +860: from it the hands reach 1,428, and the wall top is at 1,500.

**Rules for enemy paths** (GDD 10: enemies must never get stuck)

- Every opening an enemy may use is at least 288 wide and 303 high (measured: castle door 288 x 492, lab gate 392 x 303, HTB door 850 x 425). The tallest enemy capsule is 270.
- Every platform has stairs or a ramp, so the navmesh joins it. The ledge climb is only the player's faster way up.
- Cover is a plain rectangle, never a U or an L. Two obstacles either touch or leave at least 2 m between them (the widest enemy capsule is 1.2 m). Anything free-standing is at least 3 m from a wall; what is closer is built against the wall (galleries, the stage, docks, tubes). The script checks both.
- Spawn points are on the floor, at least 1 m from walls and structures, never on stairs (checked by the script).

## 3. Zone by zone

Every table below is generated from `world_layout.json`. Footprints are `X0..X1 x Y0..Y1`, heights are bottom to top. "BOX" is the engine cube scaled, with a kit material: it costs nothing in the pixel-art conversion, because it takes the same pixel tile as the walls. Wave sizes are the ones already in `Encounters.json` and are provisional.

### 3.0 The castle shell

Every castle wall is one of two meshes, 1600 long, on the wall lines below. `world_layout.json` lists the 47 pieces with origin and yaw (`castleWallPieces`). A run along Y (line "X = ...") has yaw 0, starts at its west end, and its 125 cm of thickness lies to the south of the line. A run along X (line "Y = ...") has yaw -90, starts at its south end, and its thickness lies to the east. A door piece has its doorway 799 from its origin.

| Wall line | Runs | Pieces | Door pieces (span of the piece) |
|---|---|---|---|
| X = -4000 | Y -800..800 | 1 | D0 (-800..800) |
| X = -2400 | Y -5600..5600 | 7 | D1 (-800..800) |
| X = 2400 | Y -5600..5600 | 7 | D2 (-800..800) |
| X = 5600 | Y -5600..5600 | 7 | D5 (-800..800) |
| X = 7200 | Y -800..800 | 1 | none |
| Y = -5600 | X -2400..5600 | 5 | none |
| Y = -2400 | X -2400..5600 | 5 | D4a (-2400..-800), D4b (800..2400), D7 (2400..4000) |
| Y = 2400 | X -2400..5600 | 5 | D3a (-2400..-800), D3b (800..2400), D6 (2400..4000) |
| Y = 5600 | X -2400..5600 | 5 | none |
| Y = -800 | X -4000..-2400 and X 5600..7200 | 2 | none |
| Y = 800 | X -4000..-2400 and X 5600..7200 | 2 | none |
| **Total** | | **47** | **10** door pieces, 37 plain |

The base uses the same lines as boxes (the collision shell made visible, with a panel material): 1200 high, 2400 around the hangar, 600 around the antechamber. Its doors are in the door table of section 5.

### 3.1 GATE: Gatehouse (past) / Checkpoint (future)

- **Bounds:** X -4000..-2400, Y -800..800 (16 x 16 m). Floor 0. Ceiling 1500 past, 600 future.
- **Purpose, past:** the start. A quiet room to try the controls. The outer gate (D0) is shut for good by the portcullis, which is why the south is never reachable in the past.
- **Purpose, future:** the guard post between the yard and the Breach Corridor. No fight. A weapon crate (shotgun, provisional) and ammo before the corridor.
- **Entrances:** past D1 to the courtyard, always open. Future D1f to the yard, opens when the yard wave is cleared; G0 to the corridor, opens when both wing waves are cleared (two locks in the same doorway).
- **Lighting:** past, two torches and the daylight through D1; warm and dim. Future, cold white strips, one red lamp over G0 while it is locked.

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `gate_keg_w` | prop | -3860..-3740 x -660..-540 | 0 to 127 | SM_Large_Keg | dressing |
| `gate_keg_e` | prop | -3860..-3740 x 540..660 | 0 to 127 | SM_Large_Keg | dressing |

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `gate_console_w` | cover | -4000..-3820 x -750..-350 | 0 to 240 | HTB SM_Prop_03 | guard console |
| `gate_console_e` | cover | -4000..-3820 x 350..750 | 0 to 240 | HTB SM_Prop_03 | guard console |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| chest `crate_yard` | future | (-3200, 600, 0) | Shotgun (provisional) + 1 HealthInjection |
| pickup `pk_gate_ammo` | future | (-3000, -500, 0) | Ammo_Magazine x2 |
| checkpoint `cp_start` | past | (-3300, 0, 0) | half size (300, 300, 200) |
| checkpoint `cp_checkpoint` | future | (-3300, 0, 0) | half size (300, 300, 200) |
| Player start | past | (-3300, 0, 0) | facing north (yaw 0) |

**Modules.** Past: `SM_Portcullis` 1 behind D0, 4 floor tiles `SM_CastleTower_Floor_A_05`, 2 `SM_Large_Keg`, 2 `SM_Barrel_01`, 1 `SM_WeaponStand`, 2 `SM_Torch_A`. Future: 2 `SM_Prop_03`, lab gate at G0 (frame + 2 leaves), HTB door frame and leaf at D1f.

### 3.2 COURT: Courtyard (past) / Yard (future)

- **Bounds:** X -2400..2400, Y -2400..2400 (48 x 48 m). Floor 0. Open sky in both eras; walls 1500 past, 1200 future.
- **Purpose, past:** the opening fight and the hub of the castle. Four low walls to fight around, a stage (+210) on the east wall and a perch (+410) on the west wall, each between the two doors of its wing, and four podiums 5 m high as full cover. The top of the north-east podium holds a bonus meant for the double jump.
- **Purpose, future:** the first large wave arena. Two loading docks 26 m long and 1 m high split it into three lanes (the centre lane is 24 m wide); containers at the north and south ends of the centre lane; machine blocks on the docks; a floodlight mast in each corner. The landing pad in the middle holds the hub portal P0.
- **Entrances, past:** D1 south, D2 north (ward until `Opening_Past` is cleared), D3a and D3b east, D4a and D4b west (wards until the seal is used).
- **Entrances, future:** D1f south, D2f north (lockdown until the Herald dies), D3f east and D4f west (each opens from its wing when that wing's wave is cleared).
- **Lighting, past:** overcast late afternoon, low sun from the west (the style guide's light from the top left when looking north), torches at the six doors. Future: night, four floodlight masts in the corners, cold light, red beacons on the lockdown door.

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `court_stage` | block | -800..800 x 1800..2400 | 0 to 210 | BOX + 4x8 SM_CastleTower_Floor_A_01 on top | east stage between the two Forge doors |
| `court_stage_stairs` | stairs (climbs +Y) | -405..405 x 1500..1800 | 0 to 210 | SM_Stairs_4M |  |
| `court_perch` | block | -800..800 x -2400..-1800 | 0 to 410 | BOX + SM_CastleTower_Floor_A_01 on top | west perch between the two Stormworks doors |
| `court_perch_stairs` | stairs (climbs -Y) | -405..405 x -1800..-1200 | 0 to 410 | SM_Stairs_8M |  |
| `court_cover_a` | cover | -1235..-1165 x -910..-290 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `court_cover_b` | cover | -1235..-1165 x 290..910 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `court_cover_c` | cover | 690..1310 x -935..-865 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `court_cover_d` | cover | 690..1310 x 865..935 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `court_podium_ne` | pillar | 1790..2010 x 1790..2010 | 0 to 507 | SM_Podium | full-height cover; its top is meant for the double jump (bonus pickup) |
| `court_podium_nw` | pillar | 1790..2010 x -2010..-1790 | 0 to 507 | SM_Podium | full-height cover |
| `court_podium_se` | pillar | -2010..-1790 x 1790..2010 | 0 to 507 | SM_Podium | full-height cover |
| `court_podium_sw` | pillar | -2010..-1790 x -2010..-1790 | 0 to 507 | SM_Podium | full-height cover |

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `yard_dock_w` | block | -1300..1300 x -1800..-1200 | 0 to 100 | HTB SM_Hero_Floor_01 | loading dock, 1 m high |
| `yard_dock_w_ramp_n` | stairs (climbs -X) | 1300..1600 x -1800..-1200 | 0 to 100 | BOX wedge | ramp so the navmesh joins the dock |
| `yard_dock_w_ramp_s` | stairs (climbs +X) | -1600..-1300 x -1800..-1200 | 0 to 100 | BOX wedge |  |
| `yard_dock_e` | block | -1300..1300 x 1200..1800 | 0 to 100 | HTB SM_Hero_Floor_01 | loading dock, 1 m high |
| `yard_dock_e_ramp_n` | stairs (climbs -X) | 1300..1600 x 1200..1800 | 0 to 100 | BOX wedge |  |
| `yard_dock_e_ramp_s` | stairs (climbs +X) | -1600..-1300 x 1200..1800 | 0 to 100 | BOX wedge |  |
| `yard_container_nw` | cover | 1575..1825 x -800..-200 | 0 to 260 | BOX 250x600x260, MI_Scifi_Panels + HTB SM_Wall_Cargo face | container |
| `yard_container_ne` | cover | 1575..1825 x 200..800 | 0 to 260 | BOX container | container |
| `yard_container_sw` | cover | -1825..-1575 x -800..-200 | 0 to 260 | BOX container | container |
| `yard_container_se` | cover | -1825..-1575 x 200..800 | 0 to 260 | BOX container | container |
| `yard_cube_w1` | cover | -810..-590 x -1610..-1390 | 100 to 320 | HTB SM_Prop_HS | on the dock |
| `yard_cube_w2` | cover | 590..810 x -1610..-1390 | 100 to 320 | HTB SM_Prop_HS | on the dock |
| `yard_cube_e1` | cover | -810..-590 x 1390..1610 | 100 to 320 | HTB SM_Prop_HS | on the dock |
| `yard_cube_e2` | cover | 590..810 x 1390..1610 | 100 to 320 | HTB SM_Prop_HS | on the dock |
| `yard_mast_ne` | pillar | 2225..2375 x 2225..2375 | 0 to 1200 | 2 x HTB SM_Wall_03 stacked + flood light | floodlight mast |
| `yard_mast_nw` | pillar | 2225..2375 x -2375..-2225 | 0 to 1200 | 2 x HTB SM_Wall_03 stacked + flood light | floodlight mast |
| `yard_mast_se` | pillar | -2375..-2225 x 2225..2375 | 0 to 1200 | 2 x HTB SM_Wall_03 stacked + flood light | floodlight mast |
| `yard_mast_sw` | pillar | -2375..-2225 x -2375..-2225 | 0 to 1200 | 2 x HTB SM_Wall_03 stacked + flood light | floodlight mast |

**Encounters**

| Encounter | Era | Starts | Trigger box (centre, half size) | Spawn points | Size (provisional) | Length |
|---|---|---|---|---|---|---|
| `Opening_Past` | past | on entering the box | (0, 0, 300), (1600, 1900, 400) | (1700, -1300, 0) (1700, 1300, 0) (2000, -600, 0) (2000, 600, 0) (1200, -1900, 0) (1200, 1900, 0) (600, -1500, 0) (600, 1500, 0) | 2 sub-waves, 8 enemies (imps and orcs) | 20 s |
| `Wave_Future_1` | future | on entering the box | (0, 0, 300), (1900, 1900, 600) | (2100, -1700, 0) (2100, 1700, 0) (-2100, -1700, 0) (-2100, 1700, 0) (600, -2000, 0) (600, 2000, 0) (-600, -2000, 0) (-600, 2000, 0) | 5 sub-waves, 35-40 enemies | 60 s |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| pickup `pk_court_stage` | past | (0, 2100, 210) | HealthPotion |
| pickup `pk_court_perch` | past | (0, -2100, 410) | ManaPotion |
| pickup `pk_court_podium` | past | (1900, 1900, 507) | ManaPotion x2 (meant for the double jump) |
| pickup `pk_yard_dock_w` | future | (0, -1500, 100) | Ammo_Magazine |
| pickup `pk_yard_dock_e` | future | (0, 1500, 100) | Ammo_Magazine |
| checkpoint `cp_forge_door` | past | (1600, 2150, 0) | half size (250, 200, 200) |
| checkpoint `cp_storm_door` | past | (1600, -2150, 0) | half size (250, 200, 200) |

**Modules.** Past: 16 floor tiles, `SM_Short_Walls_Wall_2M` 12, `SM_Short_Walls_Post` 8, `SM_Podium` 4, `SM_Stairs_4M` 1, `SM_Stairs_8M` 1, 2 BOX, `SM_Torch_A` 8. Future: HTB `SM_Hero_Floor_01` 2, BOX wedges 4, BOX containers 4 with `SM_Wall_Cargo` faces, `SM_Prop_HS` 4, `SM_Wall_03` 8 (four masts of two), door frames 4.

### 3.3 HALL: Great Hall (past) / Containment Hall (future)

- **Bounds:** X 2400..5600, Y -2400..2400 (32 m deep, 48 m wide). Floor 0. Ceiling 1500 past, 1200 future.
- **Purpose:** the heart of the site in both eras, and **the Valkyrie's arena in both**. She stands at the same spot: inside a cage of four rune pylons in the past, inside a cryo tube in the future. The player sees her on the way to the seal, long before the fight.
- **Past:** eight pillars 14 m high in two rows form a nave 24 m wide. Each pylon belongs to one element and disappears when that zone is cleared, so the hall itself shows the progress. With the four gone, interacting with the cage starts `Valkyrie_Past` and the centre of the hall is clear for the fight.
- **Future:** the same eight positions hold steel columns. `Opening_Future` is fought here right after the arrival. The tube stays as central cover in both future fights. `Valkyrie_Future` starts at the cryo console and needs both `Valkyrie_Past` and the Herald cleared.
- **Shared:** the pillars and the two side galleries (+210, in the north half, with stairs) are one collision body in both eras.
- **Entrances, past:** D2 south, D5 north to the seal, D6 east and D7 west (barred from the other side until the Air and Ice zones are cleared; then they are shortcuts).
- **Entrances, future:** D2f south (lockdown), D5f north to the vault. Two ways in once the lockdown lifts.
- **Lighting, past:** dark and tall. Eight chandeliers, a violet glow from the cage, a shaft of light on the throne gallery. Future: white clinical light around the tube, cyan from the tube itself, the rest dim blue; red rotating lamps during the lockdown.

**Structures, shared by both eras**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `hall_pillar_2800_w` | pillar | 2725..2875 x -1275..-1125 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_pillar_3600_w` | pillar | 3525..3675 x -1275..-1125 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_pillar_4400_w` | pillar | 4325..4475 x -1275..-1125 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_pillar_5200_w` | pillar | 5125..5275 x -1275..-1125 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_pillar_2800_e` | pillar | 2725..2875 x 1125..1275 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_pillar_3600_e` | pillar | 3525..3675 x 1125..1275 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_pillar_4400_e` | pillar | 4325..4475 x 1125..1275 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_pillar_5200_e` | pillar | 5125..5275 x 1125..1275 | 0 to 1400 | past: SM_PillarStem_01 + SM_PillarTop_01; future: 2 x HTB SM_Wall_03 stacked | same spot in both eras |
| `hall_gallery_w` | block | 4000..5600 x -2400..-1800 | 0 to 210 | past: BOX + floor tiles; future: BOX, MI_Scifi_Panels | west gallery |
| `hall_gallery_w_stairs` | stairs (climbs -Y) | 4395..5205 x -1800..-1500 | 0 to 210 | past: SM_Stairs_4M; future: BOX wedge |  |
| `hall_gallery_e` | block | 4000..5600 x 1800..2400 | 0 to 210 | past: BOX + floor tiles; future: BOX, MI_Scifi_Panels | east gallery |
| `hall_gallery_e_stairs` | stairs (climbs +Y) | 4395..5205 x 1500..1800 | 0 to 210 | past: SM_Stairs_4M; future: BOX wedge |  |

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `hall_pylon_air` | feature | 4490..4710 x 490..710 | 0 to 507 | SM_Podium + rune glow | Air pylon (north-east). Removed by `Encounter.Zone_Air.Cleared` |
| `hall_pylon_ice` | feature | 4490..4710 x -710..-490 | 0 to 507 | SM_Podium + rune glow | Ice pylon (north-west). Removed by `Encounter.Zone_Ice.Cleared` |
| `hall_pylon_fire` | feature | 3290..3510 x 490..710 | 0 to 507 | SM_Podium + rune glow | Fire pylon (south-east). Removed by `Encounter.Zone_Fire.Cleared` |
| `hall_pylon_lightning` | feature | 3290..3510 x -710..-490 | 0 to 507 | SM_Podium + rune glow | Lightning pylon (south-west). Removed by `Encounter.Zone_Lightning.Cleared` |
| `hall_table_w` | cover | 2750..3050 x -1870..-1730 | 0 to 99 | SM_HallTable_01 | low cover |
| `hall_table_e` | cover | 2750..3050 x 1730..1870 | 0 to 99 | SM_HallTable_01 | low cover |
| `hall_throne` | prop | 4740..4860 x -2310..-2190 | 210 to 495 | SM_Throne | on the west gallery, facing east |

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `hall_cryo_tube` | feature | 3790..4210 x -210..210 | 0 to 455 | SciFi SM_LabCapsule11_1_Capsule + _Glass, scale 1.3 | the Valkyrie's cryogenic tube; stays as central cover |
| `hall_cube_w` | cover | 3090..3310 x -1810..-1590 | 0 to 228 | HTB SM_Prop_HS | cover |
| `hall_cube_e` | cover | 3090..3310 x 1590..1810 | 0 to 228 | HTB SM_Prop_HS | cover |
| `hall_side_tube_w2050` | prop | 2435..2685 x -2175..-1925 | 0 to 328 | SciFi SM_LabCapsule05_Capsule + _Glass | against the south wall |
| `hall_side_tube_w1600` | prop | 2435..2685 x -1725..-1475 | 0 to 328 | SciFi SM_LabCapsule05_Capsule + _Glass | against the south wall |
| `hall_side_tube_e1600` | prop | 2435..2685 x 1475..1725 | 0 to 328 | SciFi SM_LabCapsule05_Capsule + _Glass | against the south wall |
| `hall_side_tube_e2050` | prop | 2435..2685 x 1925..2175 | 0 to 328 | SciFi SM_LabCapsule05_Capsule + _Glass | against the south wall |
| `hall_console_n1` | prop | 4660..4740 x -390..-310 | 0 to 130 | SciFi SM_ControlPanel01 | cryo release console (interaction that starts Valkyrie_Future) |

**Encounters**

| Encounter | Era | Starts | Trigger box (centre, half size) | Spawn points | Size (provisional) | Length |
|---|---|---|---|---|---|---|
| `Opening_Future` | future | on entering the box | (3900, 0, 300), (1400, 2300, 600) | (2900, -400, 0) (2900, 400, 0) (3050, -2000, 0) (3050, 2000, 0) (3600, -2100, 0) (3600, 2100, 0) | 2 sub-waves, 8 enemies; the first one spawned (first spawn point) carries the pistol | 25 s |
| `Valkyrie_Past` | past | by interaction, needs `Encounter.Zone_Fire.Cleared`, `Encounter.Zone_Lightning.Cleared`, `Encounter.Zone_Ice.Cleared`, `Encounter.Zone_Air.Cleared` | none | (4000, 0, 0) | boss | 120 s |
| `Valkyrie_Future` | future | by interaction, needs `Encounter.Valkyrie_Past.Cleared`, `Encounter.MiniBoss_Herald.Cleared` | none | (3650, 0, 0) | boss | 120 s |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| pickup `pk_hall_gallery_w` | past | (5200, -2100, 210) | ManaPotion |
| pickup `pk_hall_gallery_e` | past | (5200, 2100, 210) | HealthPotion |
| pickup `pk_hall_f_w` | future | (5200, -2100, 210) | Ammo_Magazine |
| pickup `pk_hall_f_e` | future | (5200, 2100, 210) | Ammo_Magazine |
| checkpoint `cp_hall` | past | (2750, 0, 0) | half size (200, 300, 200) |
| Rune cage (Valkyrie) | past | (4000, 0, 0) | interact when the four pylons are gone: starts Valkyrie_Past |
| Cryo tube console | future | (4700, -350, 0) | interact: starts Valkyrie_Future; needs Valkyrie_Past and MiniBoss_Herald cleared |

**Modules.** Past: 12 floor tiles, `SM_PillarStem_01` 8, `SM_PillarTop_01` 8, `SM_Podium` 4 (the pylons), `SM_Chain_XtraLong` 4 from the pylons to the cage, `SM_Stairs_4M` 2, 2 BOX galleries, `SM_HallTable_01` 2, `SM_Throne` 1, `SM_IronChandelier` 8, `SM_Castle_Door` 4 (D6 and D7). Future: HTB `SM_Wall_03` 16 (eight columns of two), `SM_LabCapsule11_1` capsule and glass at scale 1.3, `SM_LabCapsule05` capsule and glass 4, `SM_Prop_HS` 2, `SM_ControlPanel01` 1, HTB `SM_Lighting_Ceiling_01` 4.

### 3.4 SEAL: Seal Room (past) / Seal Vault (future)

- **Bounds:** X 5600..7200, Y -800..800 (16 x 16 m). Floor 0. Ceiling 1500 past, 700 future (the portal frame is 400 high).
- **Purpose:** the seal, the first portal, and at the very end the final portal. A safe room: no enemy ever spawns here.
- **The seal** lies on the floor at (6850, 0). In the past it is the interaction that sends the player to the future, in place. In the future the same spot is where the player arrives, and later where the **final portal** opens.
- **Portal P1** stands at (6150, 0), 7 m south of the seal. It lights when the pistol is picked up.
- **Entrances:** D5 / D5f south, always open.
- **Lighting, past:** four torches and the violet pulse of the seal. Future: a white lab cell, the seal under glass lit from below.

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `seal_shelf_w` | prop | 6520..6690 x -760..-700 | 0 to 201 | SM_Bookshelf_A_01 | dressing |
| `seal_shelf_e` | prop | 6520..6690 x 700..760 | 0 to 201 | SM_Bookshelf_A_01 | dressing |

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `seal_console_w` | prop | 6460..6540 x -640..-560 | 0 to 130 | SciFi SM_ControlPanel01 | dressing |
| `seal_console_e` | prop | 6460..6540 x 560..640 | 0 to 130 | SciFi SM_ControlPanel01 | dressing |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| checkpoint `cp_seal` | past | (5900, 0, 0) | half size (200, 300, 200) |
| The seal | past | (6850, 0, 0) | interact: cutscene 1, sets SealUsed, travel to the future in place |
| Final portal | future | (6850, 0, 0) | opens on the seal when the two quest items are combined; leads to the Entity map |

**Modules.** Past: 4 floor tiles, `SM_Bookshelf_A_01` 2, `SM_Rug` 1, `SM_Torch_A` 4, `SM_Book_Stack` 4. Future, the lab kit as a lining on the inner faces of the shell (the room is 1500 x 1500 between them, and 700 + 100 + 700 = 1500): on the west, east and north sides 2 `SM_MWall03-700x350-1` with 1 `SM_MPillar02_1` between them; on the south side the gate frame of D5f between 2 `SM_MWall03-350x350-1` and 2 pilasters. Totals: 6 long panels, 2 short, 5 pilasters, 1 gate, `SM_ControlPanel01` 2, `SM_LabHoloProjector01_Base` 1. The lining is 350 high; above it the shell shows up to a box ceiling at 700 (no lab ceiling: the portal frame is 400 high). The floor is a box with a panel material.

### 3.5 EAST: Forge, the Fire zone (past) / Generator Hall (future)

- **Bounds:** X -2400..2400, Y 2400..5600 (48 m long, 32 m deep). Floor 0. Ceiling 1500 past, 1200 future.
- **Past:** the fire challenge. The great hearth (a block 6 x 6 m, 1.2 m high, with a brazier) stands in the middle; four low walls; a gallery on the north wall and one on the south wall, each with stairs. The chest and portal P2 are at the east end between two podiums.
- **Future:** a wave arena. The generator core rises from the hearth's footprint to the ceiling and hides the portal from the rest of the hall, so an arrival is never in a line of fire. Machine blocks stand where the low walls were. The trigger box stops 7 m short of the portal.
- **Shared:** both galleries and their stairs.
- **Entrances, past:** D3a and D3b from the courtyard. Future: D3f to the yard (opens when the wave is cleared), D8f in the north wall to the Signal Corridor (two locks: the lightning rune plug, cross-era unlock U1, and the wave).
- **Lighting, past:** the hearth is the main light, orange from the centre, deep shadows behind the walls (pixel flame clips with their own point lights). Future: amber core glow, hazard strips on the floor.

**Structures, shared by both eras**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `east_gallery_n` | block | 1800..2400 x 2800..4400 | 0 to 210 | past: BOX + floor tiles; future: BOX, MI_Scifi_Panels | north gallery |
| `east_gallery_n_stairs` | stairs (climbs +X) | 1500..1800 x 3195..4005 | 0 to 210 | past: SM_Stairs_4M; future: BOX wedge |  |
| `east_gallery_s` | block | -2400..-1800 x 2800..4400 | 0 to 210 | past: BOX + floor tiles; future: BOX, MI_Scifi_Panels | south gallery |
| `east_gallery_s_stairs` | stairs (climbs -X) | -1800..-1500 x 3195..4005 | 0 to 210 | past: SM_Stairs_4M; future: BOX wedge |  |

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `east_hearth` | block | -300..300 x 3700..4300 | 0 to 120 | BOX + SM_ConicalCauldron_02 + SM_FirePit_01 on top | the great hearth |
| `east_cover_1` | cover | -1210..-590 x 3165..3235 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `east_cover_2` | cover | 590..1210 x 3165..3235 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `east_cover_3` | cover | -1035..-965 x 4390..5010 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `east_cover_4` | cover | 965..1035 x 4390..5010 | 0 to 203 | 3 x SM_Short_Walls_Wall_2M + 2 x SM_Short_Walls_Post | low wall |
| `east_apse_n` | pillar | 390..610 x 5380..5600 | 0 to 507 | SM_Podium | frames the portal, against the end wall |
| `east_apse_s` | pillar | -610..-390 x 5380..5600 | 0 to 507 | SM_Podium | frames the portal, against the end wall |
| `east_anvil_1` | prop | -540..-460 x 3460..3540 | 0 to 40 | SM_Anvil | dressing |
| `east_anvil_2` | prop | 460..540 x 3460..3540 | 0 to 40 | SM_Anvil | dressing |

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `east_core` | block | -300..300 x 3700..4300 | 0 to 1200 | BOX, emissive MI_Scifi_Panels | generator core (hides the portal from the hall) |
| `east_cube_1` | cover | -1010..-790 x 3090..3310 | 0 to 228 | HTB SM_Prop_HS | machine block |
| `east_cube_2` | cover | 790..1010 x 3090..3310 | 0 to 228 | HTB SM_Prop_HS | machine block |
| `east_cube_3` | cover | -1110..-890 x 4590..4810 | 0 to 228 | HTB SM_Prop_HS | machine block |
| `east_cube_4` | cover | 890..1110 x 4590..4810 | 0 to 228 | HTB SM_Prop_HS | machine block |

**Encounters**

| Encounter | Era | Starts | Trigger box (centre, half size) | Spawn points | Size (provisional) | Length |
|---|---|---|---|---|---|---|
| `Zone_Fire` | past | on entering the box | (0, 4000, 300), (2000, 1300, 600) | (-1500, 5200, 0) (1500, 5200, 0) (-2000, 4750, 0) (2000, 4750, 0) (-600, 4650, 0) (600, 4650, 0) | 5 sub-waves, 35-45 enemies | 72 s |
| `Wave_Future_2` | future | on entering the box | (0, 3550, 300), (2000, 1050, 600) | (-1400, 2750, 0) (1400, 2750, 0) (-700, 2700, 0) (700, 2700, 0) (-1200, 3800, 0) (1200, 3800, 0) | 5 sub-waves, 35-40 enemies | 67 s |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| chest `chest_fire` | past | (0, 4900, 0) | Fire knowledge (fire wall + flamethrower); Ember staff (provisional). Appears on `Encounter.Zone_Fire.Cleared` |
| pickup `pk_east_gallery` | past | (2100, 3000, 210) | ManaPotion |
| pickup `pk_east_f_gallery` | future | (-2100, 3600, 210) | Ammo_Magazine |
| pickup `pk_east_f_gallery2` | future | (2100, 3600, 210) | HealthInjection |

**Modules.** Past: 12 floor tiles, `SM_Short_Walls_Wall_2M` 12, `SM_Short_Walls_Post` 8, `SM_Podium` 2, `SM_Stairs_4M` 2, 3 BOX, `SM_ConicalCauldron_02` 1, `SM_FirePit_01` 1, `SM_Anvil` 2, `SM_WeaponStand` 2, `SM_Large_Keg` 2, `SM_Torch_A` 6. Future: 1 BOX core, `SM_Prop_HS` 4, 2 BOX galleries and 2 wedges (looks of the shared bodies), HTB `SM_Pipe_03` 4, `SM_Top_Cable_System` 2, `SM_Lighting_Ceiling_01` 2, lab gate at D8f, HTB door at D3f.

### 3.6 WEST: Stormworks, the Lightning zone (past) / Server Hall (future)

- **Bounds:** X -2400..2400, Y -5600..-2400. The mirror of EAST across the X axis.
- **Past:** the lightning challenge, in the castle's library and storm workshop. Four rows of bookshelves as cover, the storm rod (a podium with sparks) in the middle, the same two galleries. Chest and portal P3 at the west end.
- **Future:** a wave arena. **Server racks stand exactly where the bookshelves stood**, and the server core where the storm rod was.
- **Entrances, past:** D4a and D4b. Future: D4f to the yard, D9f to the Cryo Corridor (two locks: the fire rune plug, cross-era unlock U2, and the wave).
- **Lighting, past:** cold blue-white flashes from the rod, candles on the shelves. Future: rack LEDs in rows, dark ceiling.

**Structures, shared by both eras**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `west_gallery_n` | block | 1800..2400 x -4400..-2800 | 0 to 210 | past: BOX + floor tiles; future: BOX, MI_Scifi_Panels | north gallery |
| `west_gallery_n_stairs` | stairs (climbs +X) | 1500..1800 x -4005..-3195 | 0 to 210 | past: SM_Stairs_4M; future: BOX wedge |  |
| `west_gallery_s` | block | -2400..-1800 x -4400..-2800 | 0 to 210 | past: BOX + floor tiles; future: BOX, MI_Scifi_Panels | south gallery |
| `west_gallery_s_stairs` | stairs (climbs -X) | -1800..-1500 x -4005..-3195 | 0 to 210 | past: SM_Stairs_4M; future: BOX wedge |  |

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `west_orrery` | pillar | -110..110 x -4110..-3890 | 0 to 507 | SM_Podium + spark sprite | the storm rod |
| `west_cover_1` | cover | -1155..-645 x -3260..-3140 | 0 to 201 | 3 x SM_Bookshelf_A_01 back to back with 3 more | bookshelf row |
| `west_cover_2` | cover | 645..1155 x -3260..-3140 | 0 to 201 | 3 x SM_Bookshelf_A_01 back to back with 3 more | bookshelf row |
| `west_cover_3` | cover | -1060..-940 x -4955..-4445 | 0 to 201 | 3 x SM_Bookshelf_A_01 back to back with 3 more | bookshelf row |
| `west_cover_4` | cover | 940..1060 x -4955..-4445 | 0 to 201 | 3 x SM_Bookshelf_A_01 back to back with 3 more | bookshelf row |
| `west_apse_n` | pillar | 390..610 x -5600..-5380 | 0 to 507 | SM_Podium | frames the portal, against the end wall |
| `west_apse_s` | pillar | -610..-390 x -5600..-5380 | 0 to 507 | SM_Podium | frames the portal, against the end wall |

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `west_core` | block | -300..300 x -4300..-3700 | 0 to 1200 | BOX, emissive MI_Scifi_Panels | server core |
| `west_rack_1` | cover | -1155..-645 x -3260..-3140 | 0 to 260 | BOX 510x120x260, emissive MI_Scifi_Panels | server rack row (where the bookshelves were) |
| `west_rack_2` | cover | 645..1155 x -3260..-3140 | 0 to 260 | BOX 510x120x260, emissive MI_Scifi_Panels | server rack row (where the bookshelves were) |
| `west_rack_3` | cover | -1060..-940 x -4955..-4445 | 0 to 260 | BOX 510x120x260, emissive MI_Scifi_Panels | server rack row (where the bookshelves were) |
| `west_rack_4` | cover | 940..1060 x -4955..-4445 | 0 to 260 | BOX 510x120x260, emissive MI_Scifi_Panels | server rack row (where the bookshelves were) |

**Encounters**

| Encounter | Era | Starts | Trigger box (centre, half size) | Spawn points | Size (provisional) | Length |
|---|---|---|---|---|---|---|
| `Zone_Lightning` | past | on entering the box | (0, -4000, 300), (2000, 1300, 600) | (-1500, -5200, 0) (1500, -5200, 0) (-2000, -4750, 0) (2000, -4750, 0) (-600, -4650, 0) (600, -4650, 0) | 5 sub-waves, 35-45 enemies | 72 s |
| `Wave_Future_3` | future | on entering the box | (0, -3550, 300), (2000, 1050, 600) | (-1400, -2750, 0) (1400, -2750, 0) (-700, -2700, 0) (700, -2700, 0) (-1200, -3800, 0) (1200, -3800, 0) | 5 sub-waves, 35-40 enemies | 74 s |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| chest `chest_lightning` | past | (0, -4900, 0) | Lightning knowledge (rune trap + static ball). Appears on `Encounter.Zone_Lightning.Cleared` |
| pickup `pk_west_gallery` | past | (2100, -3000, 210) | ManaPotion |
| pickup `pk_west_f_gallery` | future | (-2100, -3600, 210) | Ammo_Magazine |
| pickup `pk_west_f_gallery2` | future | (2100, -3600, 210) | HealthInjection |

**Modules.** Past: 12 floor tiles, `SM_Bookshelf_A_01` 24 (four rows of three, back to back), `SM_Podium` 3, `SM_Stairs_4M` 2, 2 BOX, `SM_Book_Stack` 6, `SM_Torch_A` 6. Future: 4 BOX rack rows, 1 BOX core, gallery looks, `SM_Top_Cable_System` 2, `SM_Lighting_Ceiling_01` 2, lab gate at D9f, HTB door at D4f.

### 3.7 NE: High Ward, the Air zone (past) / Signal Corridor (future)

- **Bounds, past:** X 2400..5600, Y 2400..5600 (32 x 32 m), open sky. **Future:** only the strip X 2400..3100 (7 m between wall lines, 6 m clear, 32 m long), ceiling 700.
- **Past:** the air challenge, and the one zone built on height. A ground strip 12 m wide along the south wall, where portal P5 and the barred door D6 are, and a terrace 4.1 m up over the rest. Ways up: the wide stairs (the enemies' way), or a 2 m step and a ledge climb at the east end. On the terrace: two wind towers 2 m high, the aerie 2.9 m high with the chest, and a spire 4.5 m high meant for the double jump, as the first use of what the chest teaches. Parapets on the terrace edge give cover both ways.
- **Future:** a corridor fight (one of the few). The player comes in through D8f in the middle, the portal is 16 m to the west, a supply stash at the east end.
- **Entrances, past:** portal P5 only, until the zone is cleared; then D6 into the Great Hall. Future: D8f.
- **Lighting, past:** the brightest place of the castle: open sky, wind, banners. Future: flickering strip lights, the mast's red beacon through a skylight.

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `ne_terrace` | block | 3600..5600 x 2400..5600 | 0 to 410 | BOX; south face 8 x SM_CastleTower_Wall_A_01; top 5x8 SM_CastleTower_Floor_A_01 (or one tiled box) | the terrace, 4.1 m up |
| `ne_stairs` | stairs (climbs +X) | 3000..3600 x 3595..4405 | 0 to 410 | SM_Stairs_8M | main way up (navmesh) |
| `ne_step` | block | 3400..3600 x 5100..5300 | 0 to 200 | BOX | second way up for the player: hop on, then ledge climb |
| `ne_crenel_1` | cover | 3607.5..3692.5 x 2492..2908 | 410 to 641 | SM_Crenel_A_01 | parapet on the terrace edge |
| `ne_crenel_2` | cover | 3607.5..3692.5 x 2908..3324 | 410 to 641 | SM_Crenel_A_01 | parapet |
| `ne_crenel_3` | cover | 3607.5..3692.5 x 4442..4858 | 410 to 641 | SM_Crenel_A_01 | parapet |
| `ne_tower_1` | block | 4000..4400 x 2800..3200 | 410 to 610 | BOX | wind tower, 2 m above the terrace |
| `ne_tower_2` | block | 4800..5200 x 3800..4200 | 410 to 610 | BOX | wind tower, 2 m above the terrace |
| `ne_aerie` | block | 5200..5600 x 4800..5600 | 410 to 700 | BOX | the aerie, 2.9 m above the terrace: the Air chest |
| `ne_spire` | pillar | 4190..4410 x 4890..5110 | 410 to 860 | SM_Podium, sunk 57 cm into the terrace | 4.5 m above the terrace: meant for the double jump (bonus) |

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `ne_crate` | cover | 2920..3040 x 3840..3960 | 0 to 112 | SciFi SM_LabBarrels01_1 x3 | low cover against the north wall of the corridor |

**Encounters**

| Encounter | Era | Starts | Trigger box (centre, half size) | Spawn points | Size (provisional) | Length |
|---|---|---|---|---|---|---|
| `Zone_Air` | past | on entering the box | (4600, 4000, 710), (1000, 1600, 400) | (5300, 2800, 410) (5300, 3400, 410) (5000, 4500, 410) (4500, 5300, 410) (4000, 3600, 410) (4700, 2700, 410) | 5 sub-waves, 35-45 enemies (imps and shamans first) | 72 s |
| `Skirmish_Signal` (new) | future | on entering the box | (2750, 4800, 200), (350, 300, 200) | (2750, 4100, 0) (2600, 3600, 0) (2900, 3600, 0) (2750, 4400, 0) | NEW, corridor fight: 3 Drone_Future + 2 Orc_Future, one wave | 15 s |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| chest `chest_air` | past | (5400, 5200, 700) | Air knowledge (double jump, improved dash, speed). Appears on `Encounter.Zone_Air.Cleared` |
| pickup `pk_ne_tower` | past | (4200, 3000, 610) | ManaPotion |
| pickup `pk_ne_spire` | past | (4300, 5000, 860) | HealthPotion x2 (meant for the double jump) |
| pickup `pk_signal_end` | future | (2750, 5400, 0) | Ammo_Magazine + HealthInjection |

**Modules.** Past: 3 floor tiles on the strip, 1 BOX terrace (2000 x 3200 x 410) with `SM_CastleTower_Wall_A_01` 8 on its south face, `SM_Stairs_8M` 1, `SM_Crenel_A_01` 3, 4 BOX, `SM_Podium` 1, `SM_Rubble_02` 3 and `SM_PillarTop_Broken` 1 as dressing, `SM_Castle_Door` 2 at D6 (counted in HALL). Future, the lab kit as a lining on the two long faces of the shell (the corridor is 600 wide and 3100 long between them, and 4 x 700 + 3 x 100 = 3100): per side and per row 4 `SM_MWall03-700x350-1` and 3 `SM_MPillar02_1`, two rows high (to 700). Totals: 16 panels, 12 pilasters, `SM_MFloor02-700x350-1` 9, `SM_LabBarrels01_1` 3, lab gate at D8f (counted in EAST; it replaces part of one panel).

### 3.8 NW: Frozen Cistern, the Ice zone (past) / Cryo Corridor (future)

- **Bounds, past:** X 2400..5600, Y -5600..-2400 (32 x 32 m), ceiling 1500. **Future:** only the strip X 2400..3100, ceiling 700.
- **Past:** the ice challenge, flat and wide. Six frozen pillars in two rows, two ice blocks, and a gallery along the whole north wall with the chest. The arrival pocket with portal P4 is the south strip, 9 m from the trigger box.
- **Future:** a quiet corridor (no fight): frost on the pipes, a stash at the west end. It is the cold end of the base, next to where the Valkyrie is kept.
- **Entrances, past:** portal P4 only, until the zone is cleared; then D7 into the Great Hall. Future: D9f.
- **Lighting, past:** blue light from the ice, no warm source at all, breath-like fog near the floor. Future: white-blue, cold fog cards.

**Structures, past**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `nw_pillar_3700_5000` | pillar | 3625..3775 x -5075..-4925 | 0 to 1400 | SM_PillarStem_01 + SM_PillarTop_01 | frozen pillar |
| `nw_pillar_3700_4000` | pillar | 3625..3775 x -4075..-3925 | 0 to 1400 | SM_PillarStem_01 + SM_PillarTop_01 | frozen pillar |
| `nw_pillar_3700_3000` | pillar | 3625..3775 x -3075..-2925 | 0 to 1400 | SM_PillarStem_01 + SM_PillarTop_01 | frozen pillar |
| `nw_pillar_4400_5000` | pillar | 4325..4475 x -5075..-4925 | 0 to 1400 | SM_PillarStem_01 + SM_PillarTop_01 | frozen pillar |
| `nw_pillar_4400_4000` | pillar | 4325..4475 x -4075..-3925 | 0 to 1400 | SM_PillarStem_01 + SM_PillarTop_01 | frozen pillar |
| `nw_pillar_4400_3000` | pillar | 4325..4475 x -3075..-2925 | 0 to 1400 | SM_PillarStem_01 + SM_PillarTop_01 | frozen pillar |
| `nw_gallery` | block | 5000..5600 x -5600..-2400 | 0 to 210 | BOX + floor tiles | north gallery: the Ice chest |
| `nw_gallery_stairs` | stairs (climbs +X) | 4700..5000 x -4405..-3595 | 0 to 210 | SM_Stairs_4M |  |
| `nw_ice_1` | cover | 3950..4150 x -4700..-4300 | 0 to 200 | BOX, ice tile | ice block |
| `nw_ice_2` | cover | 3950..4150 x -3700..-3300 | 0 to 200 | BOX, ice tile | ice block |

**Encounters**

| Encounter | Era | Starts | Trigger box (centre, half size) | Spawn points | Size (provisional) | Length |
|---|---|---|---|---|---|---|
| `Zone_Ice` | past | on entering the box | (4500, -4000, 300), (900, 1600, 600) | (4550, -5300, 0) (4550, -2700, 0) (4000, -5300, 0) (4000, -2700, 0) (4500, -4600, 0) (4500, -3400, 0) | 5 sub-waves, 35-45 enemies | 72 s |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| chest `chest_ice` | past | (5300, -4000, 210) | Ice knowledge (ice shield + ice floor). Appears on `Encounter.Zone_Ice.Cleared` |
| pickup `pk_nw_gallery` | past | (5300, -2700, 210) | HealthPotion |
| pickup `pk_cryo_end` | future | (2750, -5400, 0) | Ammo_Magazine + Grenade (provisional) |

**Modules.** Past: 9 floor tiles, `SM_PillarStem_01` 6, `SM_PillarTop_01` 6, `SM_Stairs_4M` 1, 3 BOX (gallery and two ice blocks), `SM_Castle_Door` 2 at D7 (counted in HALL). Future: the same lining as the Signal Corridor: 16 panels, 12 pilasters, `SM_MFloor02-700x350-1` 9, lab gate at D9f (counted in WEST).

### 3.9 BREACH, ANTE, HANGAR (future only)

- **Breach Corridor:** X -8050..-4000, Y -265..265, height 333. The imported Spaceship Corridor Damaged, whole or in four segments of about 10 m. Until the model is ready, a box corridor of the same size. `Skirmish_Breach`, the second corridor fight: the trigger is 7 m in, the enemies stand 30 m away at the far end, so the fight is a push down the corridor under flickering light.
- **Antechamber:** X -8450..-8050, Y -1800..1800 (4 x 36 m), ceiling 600. It turns the one corridor into **two doors into the hangar** (H1, H2), so the arena has two ways in and out. Ammo and an injection before the fight, and a checkpoint.
- **Hangar:** X -13250..-8450, Y -2400..2400 (48 x 48 m), **ceiling 2400**. The Herald is about 9.4 m tall and floats, banks and strafes; with the tentacle strikes to come it needs the height and a clear centre. A lane 23 m wide stays clear down the whole length of the room. Four containers 12 m long and 3 m high stand along its two sides as cover that can be climbed; a dock 26 m long (+100) runs along each side wall; a gantry (+282) runs along the south wall, with a ramp at each end, for shots at eye level with the weak point.
- **Entrances:** G0 from the checkpoint, B1 the open end of the corridor, H1 and H2.
- **Lighting:** the corridor has only its broken cables and sparks (`SciFi_Impacts`). The hangar is lit by eight spot lights from the roof in pools, with the Herald's own green glow; haze near the roof.

**Structures, future**

| Id | Kind | Footprint | Bottom to top | Built from | Note |
|---|---|---|---|---|---|
| `hangar_dock_w` | block | -12150..-9550 x -2400..-1800 | 0 to 100 | HTB SM_Hero_Floor_01 | dock, 1 m high |
| `hangar_dock_w_ramp_n` | stairs (climbs -X) | -9550..-9250 x -2400..-1800 | 0 to 100 | BOX wedge |  |
| `hangar_dock_w_ramp_s` | stairs (climbs +X) | -12450..-12150 x -2400..-1800 | 0 to 100 | BOX wedge |  |
| `hangar_dock_e` | block | -12150..-9550 x 1800..2400 | 0 to 100 | HTB SM_Hero_Floor_01 | dock, 1 m high |
| `hangar_dock_e_ramp_n` | stairs (climbs -X) | -9550..-9250 x 1800..2400 | 0 to 100 | BOX wedge |  |
| `hangar_dock_e_ramp_s` | stairs (climbs +X) | -12450..-12150 x 1800..2400 | 0 to 100 | BOX wedge |  |
| `hangar_container_nw` | cover | -10300..-9100 x -1450..-1150 | 0 to 300 | BOX 1200x300x300 container | big cover |
| `hangar_container_ne` | cover | -10300..-9100 x 1150..1450 | 0 to 300 | BOX container | big cover |
| `hangar_container_sw` | cover | -12600..-11400 x -1450..-1150 | 0 to 300 | BOX container | big cover |
| `hangar_container_se` | cover | -12600..-11400 x 1150..1450 | 0 to 300 | BOX container | big cover |
| `hangar_catwalk` | block | -13250..-13050 x -2090..2090 | 262 to 282 | 10 x HTB SM_Bridge_01 (deck at +282) + 4 x SM_Railing_02 on the wall side | gantry along the south wall: a deck box in the shell (the kit piece keeps no collision; its own hull is solid underneath) |
| `hangar_catwalk_ramp_w` | stairs (climbs -X) | -13050..-12450 x -2090..-1890 | 0 to 282 | BOX wedge | ramp to the gantry |
| `hangar_catwalk_ramp_e` | stairs (climbs -X) | -13050..-12450 x 1890..2090 | 0 to 282 | BOX wedge | ramp to the gantry |
| `hangar_cube_w_s` | cover | -11410..-11190 x -2210..-1990 | 100 to 328 | HTB SM_Prop_HS | on the dock |
| `hangar_cube_w_n` | cover | -10510..-10290 x -2210..-1990 | 100 to 328 | HTB SM_Prop_HS | on the dock |
| `hangar_cube_e_s` | cover | -11410..-11190 x 1990..2210 | 100 to 328 | HTB SM_Prop_HS | on the dock |
| `hangar_cube_e_n` | cover | -10510..-10290 x 1990..2210 | 100 to 328 | HTB SM_Prop_HS | on the dock |

**Encounters**

| Encounter | Era | Starts | Trigger box (centre, half size) | Spawn points | Size (provisional) | Length |
|---|---|---|---|---|---|---|
| `Skirmish_Breach` (new) | future | on entering the box | (-4700, 0, 160), (300, 265, 170) | (-7000, 0, 0) (-7200, -120, 0) (-7200, 120, 0) (-7600, 0, 0) (-7700, -130, 0) (-7700, 130, 0) | NEW, corridor fight: 4 Drone_Future + 2 Orc_Future, one wave | 20 s |
| `MiniBoss_Herald` (new) | future | on entering the box | (-10850, 0, 500), (2000, 2000, 1000) | (-10850, 0, 0) | NEW, mini-boss: the tentacle machine (actor class GBEntity), one wave | 75 s |

**Chests, pickups, checkpoints and special points** (all provisional)

| What | Era | Position | Contents or note |
|---|---|---|---|
| chest `crate_herald` | future | (-12800, 0, 0) | Rifle (provisional) + 2 Ammo_Magazine. Appears on `Encounter.MiniBoss_Herald.Cleared` |
| pickup `pk_ante` | future | (-8250, -1500, 0) | Ammo_Magazine x2 + HealthInjection (before the Herald) |
| pickup `pk_hangar_dock_w` | future | (-10850, -2100, 100) | Ammo_Magazine |
| pickup `pk_hangar_dock_e` | future | (-10850, 2100, 100) | Ammo_Magazine |
| pickup `pk_hangar_catwalk` | future | (-13150, 0, 282) | HealthInjection |
| checkpoint `cp_ante` | future | (-8250, 0, 0) | half size (150, 300, 200) |

**Modules.** Breach: the corridor model. Antechamber: boxes only. Hangar: HTB `SM_Hero_Floor_01` 2, BOX wedges 6, BOX containers 4, `SM_Bridge_01` 10, `SM_Railing_02` 4, `SM_Prop_HS` 4, HTB door frames 2 (H1, H2, no leaves), `SM_Wall_01` 16 as accent bays, `SM_Pipe_03` 6, `SM_Top_Cable_System` 2.

## 4. Portals

All six are `AGBPortal` actors in the persistent level, on the shared floor slab at Z 0, with `bTeleport` off: the player changes era on the spot. A portal with a required flag gets a `UGBEraComponent` (Presence Both, RequiredFlag), so it only exists once the flag is set. The frame is 320 wide and 400 high and has no collision.

| Portal | Position | Past side | Future side | Exists when | Note |
|---|---|---|---|---|---|
| **P1** Seal portal | (6150, 0, 0) | Seal Room | Seal Vault | `GotFirstGun` | the way back after the first gun |
| **P0** Hub portal | (0, 0, 0) | Courtyard | Yard | `Encounter.Wave_Future_1.Cleared` | Courtyard <-> Yard, lit once the Yard wave is cleared |
| **P2** Forge portal | (0, 5300, 0) | Forge (Fire zone) | Generator Hall | `Encounter.Zone_Fire.Cleared` | Forge <-> Generator Hall |
| **P3** Stormworks portal | (0, -5300, 0) | Stormworks (Lightning zone) | Server Hall | `Encounter.Zone_Lightning.Cleared` | Stormworks <-> Server Hall |
| **P4** Cryo portal | (2750, -3200, 0) | Frozen Cistern (Ice zone) | Cryo Corridor | always | Cryo Corridor <-> Frozen Cistern; only reachable from the future first |
| **P5** Signal portal | (2750, 3200, 0) | High Ward (Air zone) | Signal Corridor | always | Signal Corridor <-> High Ward; only reachable from the future first |

The floor is walkable and clear at each of these spots in both eras: the script checks 1.5 m around each against every structure of both eras, and that each is at least 2 m inside the walkable part of its room.

**The seal** (6850, 0, 0) is not a portal: it is a one-way trip to the future, once. **The final portal** opens at that same spot in the future when the flag `FinalPortalOpen` is set (the recipe that combines the two quest items sets it, as `Items.json` already allows).

## 5. Era-gated zones, cross-era unlocks, and the final portal

**Era-gated zones** (reachable in one era and not in the other)

| # | Zone | Era | Why it cannot be reached in the other era |
|---|---|---|---|
| G1 | Breach Corridor, Antechamber, Hangar | future only | in the past the outer gate is shut by the portcullis and nothing is built beyond it |
| G2 | High Ward: terrace, wind towers, aerie | past only | in the future only the 7 m corridor exists; the rest is sealed mass |
| G3 | Frozen Cistern: the pillar hall and its gallery | past only | same |
| G4 | The way into the Air and Ice zones | from the future only | in the past D6 and D7 are barred from the other side; the only way in is through the future corridors and portals P5 and P4 |

**Cross-era unlocks** (cause in the past, effect in the future). Each one is a barrier actor in the future sub-level with `RemovedByFlag`; no new code.

| # | Cause, in the past | Effect, in the future | What the player sees |
|---|---|---|---|
| U1 | The Lightning zone is cleared (`Encounter.Zone_Lightning.Cleared`) | The plug in D8f is gone: Generator Hall to Signal Corridor, and so to the Air zone | A player who did the Forge first arrives in the Generator Hall and finds D8f sealed by a block marked with the lightning rune. After the Stormworks, it is open |
| U2 | The Fire zone is cleared (`Encounter.Zone_Fire.Cleared`) | The plug in D9f is gone: Server Hall to Cryo Corridor, and so to the Ice zone | The same with the fire rune, for a player who did the Stormworks first |
| U3 | The Valkyrie is defeated in her runes (`Encounter.Valkyrie_Past.Cleared`) | The cryo tube can be released | The tube is seen sealed in step 4, with the same rune marks as the cage |

The two wing unlocks are **crossed on purpose**: each wing opens the other wing's door. If the Forge opened its own future door, the player would always arrive after the cause and never see the door shut. Storm for air and fire for ice also read well.

**Where the final portal stands:** on the seal, in the Seal Vault, at (6850, 0, 0), one room north of where the second Valkyrie falls. The Entity's prison door is the seal that trapped the player in the first minute.

**Flags used** (all read by `UGBEraComponent` on a barrier, portal, chest or pylon)

| Flag | Set by | What it changes |
|---|---|---|
| `Encounter.Opening_Past.Cleared` | clearing `Opening_Past` (set by the encounter itself) | opens D2 (rune ward) |
| `SealUsed` | the seal, when used (new) | opens D3a (rune ward); opens D3b (rune ward); opens D4a (rune ward); opens D4b (rune ward) |
| `GotFirstGun` | picking up the pistol (the flag name is already used by the test arena) | lights portal P1 |
| `Encounter.Zone_Fire.Cleared` | clearing `Zone_Fire` (set by the encounter itself) | opens D9f (rune-sealed plug); lights portal P2; shows `chest_fire`; removes `hall_pylon_fire`; needed to start `Valkyrie_Past` |
| `Encounter.Zone_Lightning.Cleared` | clearing `Zone_Lightning` (set by the encounter itself) | opens D8f (rune-sealed plug); lights portal P3; shows `chest_lightning`; removes `hall_pylon_lightning`; needed to start `Valkyrie_Past` |
| `Encounter.Zone_Ice.Cleared` | clearing `Zone_Ice` (set by the encounter itself) | opens D7 (barred door (2 x SM_Castle_Door)); shows `chest_ice`; removes `hall_pylon_ice`; needed to start `Valkyrie_Past` |
| `Encounter.Zone_Air.Cleared` | clearing `Zone_Air` (set by the encounter itself) | opens D6 (barred door (2 x SM_Castle_Door)); shows `chest_air`; removes `hall_pylon_air`; needed to start `Valkyrie_Past` |
| `Encounter.Wave_Future_2.Cleared` | clearing `Wave_Future_2` (set by the encounter itself) | opens G0 (blast gate); opens D3f (door leaf); opens D8f (door leaf) |
| `Encounter.Wave_Future_3.Cleared` | clearing `Wave_Future_3` (set by the encounter itself) | opens G0 (blast gate (second lock)); opens D4f (door leaf); opens D9f (door leaf) |
| `Encounter.Wave_Future_1.Cleared` | clearing `Wave_Future_1` (set by the encounter itself) | opens D1f (door leaf); lights portal P0 |
| `Encounter.MiniBoss_Herald.Cleared` | clearing `MiniBoss_Herald` (set by the encounter itself) | opens D2f (lockdown shutter); shows `crate_herald`; needed to start `Valkyrie_Future` |
| `Encounter.Valkyrie_Past.Cleared` | clearing `Valkyrie_Past` (set by the encounter itself) | needed to start `Valkyrie_Future` |
| `FinalPortalOpen` | the recipe that combines the two quest items | shows the final portal |

**Doors and their locks**

| Door | Era | Position | Opening (w x h) | Piece | Connects | Locks (the door opens when every lock is gone) |
|---|---|---|---|---|---|---|
| **D0** | past | (-4000, 0), wall X = -4000 | 288 x 492 | SM_Wall_Door_16x15 | Outer gate (shut for good: portcullis) | SM_Portcullis: never removed |
| **D1** | past | (-2400, 0), wall X = -2400 | 288 x 492 | SM_Wall_Door_16x15 | Gatehouse -> Courtyard | none, always open |
| **D2** | past | (2400, 0), wall X = 2400 | 288 x 492 | SM_Wall_Door_16x15 | Courtyard -> Great Hall | rune ward: removed by `Encounter.Opening_Past.Cleared` |
| **D3a** | past | (-1600, 2400), wall Y = 2400 | 288 x 492 | SM_Wall_Door_16x15 | Courtyard -> Forge (south door) | rune ward: removed by `SealUsed` |
| **D3b** | past | (1600, 2400), wall Y = 2400 | 288 x 492 | SM_Wall_Door_16x15 | Courtyard -> Forge (north door) | rune ward: removed by `SealUsed` |
| **D4a** | past | (-1600, -2400), wall Y = -2400 | 288 x 492 | SM_Wall_Door_16x15 | Courtyard -> Stormworks (south door) | rune ward: removed by `SealUsed` |
| **D4b** | past | (1600, -2400), wall Y = -2400 | 288 x 492 | SM_Wall_Door_16x15 | Courtyard -> Stormworks (north door) | rune ward: removed by `SealUsed` |
| **D5** | past | (5600, 0), wall X = 5600 | 288 x 492 | SM_Wall_Door_16x15 | Great Hall -> Seal Room | none, always open |
| **D6** | past | (3200, 2400), wall Y = 2400 | 288 x 492 | SM_Wall_Door_16x15 | Great Hall <- High Ward (barred from the Air side) | barred door (2 x SM_Castle_Door): removed by `Encounter.Zone_Air.Cleared` |
| **D7** | past | (3200, -2400), wall Y = -2400 | 288 x 492 | SM_Wall_Door_16x15 | Great Hall <- Frozen Cistern (barred from the Ice side) | barred door (2 x SM_Castle_Door): removed by `Encounter.Zone_Ice.Cleared` |
| **G0** | future | (-4000, 0), wall X = -4000 | 392 x 303 | SciFi SM_MGate01 frame + GateL + GateR | Checkpoint -> Breach Corridor | blast gate: removed by `Encounter.Wave_Future_2.Cleared`; blast gate (second lock): removed by `Encounter.Wave_Future_3.Cleared` |
| **D1f** | future | (-2400, 0), wall X = -2400 | 850 x 425 | HTB SM_Door_02_Side (frame) + SM_Door_02 (leaf) | Yard -> Checkpoint | door leaf: removed by `Encounter.Wave_Future_1.Cleared` |
| **D2f** | future | (2400, 0), wall X = 2400 | 850 x 425 | HTB SM_Door_02_Side (frame) + SM_Door_02 (leaf) | Yard <-> Containment Hall (LOCKDOWN) | lockdown shutter: removed by `Encounter.MiniBoss_Herald.Cleared` |
| **D3f** | future | (0, 2400), wall Y = 2400 | 850 x 425 | HTB SM_Door_02_Side (frame) + SM_Door_02 (leaf) | Generator Hall -> Yard | door leaf: removed by `Encounter.Wave_Future_2.Cleared` |
| **D4f** | future | (0, -2400), wall Y = -2400 | 850 x 425 | HTB SM_Door_02_Side (frame) + SM_Door_02 (leaf) | Server Hall -> Yard | door leaf: removed by `Encounter.Wave_Future_3.Cleared` |
| **D5f** | future | (5600, 0), wall X = 5600 | 392 x 303 | SciFi SM_MGate01 frame + GateL + GateR | Containment Hall <-> Seal Vault | none, always open |
| **D8f** | future | (2400, 4800), wall X = 2400 | 392 x 303 | SciFi SM_MGate01 frame + GateL + GateR | Generator Hall -> Signal Corridor (cross-era unlock U1) | rune-sealed plug: removed by `Encounter.Zone_Lightning.Cleared`; door leaf: removed by `Encounter.Wave_Future_2.Cleared` |
| **D9f** | future | (2400, -4800), wall X = 2400 | 392 x 303 | SciFi SM_MGate01 frame + GateL + GateR | Server Hall -> Cryo Corridor (cross-era unlock U2) | rune-sealed plug: removed by `Encounter.Zone_Fire.Cleared`; door leaf: removed by `Encounter.Wave_Future_3.Cleared` |
| **B1** | future | (-8050, 0), wall X = -8050 | 530 x 333 | open end of the corridor model | Breach Corridor -> Antechamber | none, always open |
| **H1** | future | (-8450, -1200), wall X = -8450 | 850 x 425 | HTB SM_Door_02_Side (frame) + SM_Door_02 (leaf) | Antechamber -> Hangar (west door) | none, always open |
| **H2** | future | (-8450, 1200), wall X = -8450 | 850 x 425 | HTB SM_Door_02_Side (frame) + SM_Door_02 (leaf) | Antechamber -> Hangar (east door) | none, always open |

## 6. Module catalog

Sizes are local bounds in cm, "measured" through the Editor. The pivot is where the bounds say (0,0,0) is. All paths start at `/Game/`.

### 6.1 Castle (`CastleTown`): the short list for the pixel-art conversion, 31 meshes

Two kits of the pack are used: the Hall kit for the shell and the Courtyard kit for stairs and cover, plus two pillars from the Candle interior set.

| # | Asset (under `CastleTown/`) | Bounds min to max | Triangles | Use |
|---|---|---|---|---|
| 1 | `Static_Mesh/Castle_Kit/Buildings/Hall_1/SM_Wall_16x15` | (-117, 0, 0) to (8, 1600, 1500) | 23,294 | every wall of the castle: 37 pieces. Both faces are modelled, so one piece serves the rooms on both sides |
| 2 | `.../Hall_1/SM_Wall_Door_16x15` | same | 22,914 | the same wall with a pointed doorway: opening Y 655..943 (288 wide), 492 high. 10 pieces. **Its simple collision is one hull that closes the doorway** |
| 3 | `.../A/SM_CastleTower_Floor_A_05` | (-600, -600, 0) to (600, 600, 50) | 2,293 | floor tile 12 x 12 m, pivot in the centre; the walking surface is its top, so it goes at Z -50. About 72 pieces |
| 4 | `.../A/SM_CastleTower_Floor_A_01` | (0, 0, 0) to (400, 400, 50) | 298 | 4 x 4 m tile, for platform tops and patches |
| 5 | `.../A/SM_CastleTower_Wall_A_01` | (0, -59, 0) to (400, 9, 400) | 12,455 | 4 x 4 m wall panel: the south face of the terrace (8) |
| 6 | `.../Castle_Gate/SM_Portcullis` | (-11, -10, 0) to (414, 12, 599) | 6,140 | shuts the outer gate (1) |
| 7 | `.../Courtyard/SM_Stairs_4M` | (-811, -1, 4) to (9, 297, 213) | 4,834 | stairs 8.2 m wide, run 3 m, rise 2.09 m, climbing towards +Y. One hull: it behaves as a 35 degree ramp. 8 pieces |
| 8 | `.../Courtyard/SM_Stairs_8M` | (-811, -1, 4) to (9, 605, 413) | 7,806 | the same, run 6 m, rise 4.09 m. 2 pieces |
| 9 | `.../WindowsandDoors/SM_Castle_Door` | (-5, -12, 0) to (26, 147, 525) | 31,241 | door leaf 1.59 x 5.25 m; two close a doorway. 4 pieces (D6, D7) |
| 10 | `.../Courtyard/SM_Short_Walls_Wall_2M` | (-21, -202, 0) to (47, 4, 203) | 1,824 | low wall 2.05 m long, 2.03 m high: cover in runs of three. 24 pieces |
| 11 | `.../Courtyard/SM_Short_Walls_Post` | (-9, -63, 0) to (63, 9, 222) | 1,580 | ends of the low walls. 16 pieces |
| 12 | `.../Courtyard/SM_Podium` | (-13, -8, 0) to (212, 207, 507) | 10,211 | stone block 2.2 m square, 5.07 m high, pivot at a corner: full cover, the rune pylons, the storm rod, the spire (sunk 57 cm). 14 pieces |
| 13 | `.../A/SM_Crenel_A_01` | (-8, -71, 0) to (408, 14, 231) | 9,684 | battlement 4.16 m long: the terrace parapets (3) |
| 14 | `Candle/StaticMeshes/Architecture/SM_PillarStem_01` | (-59, -59, 18) to (59, 59, 983) | 3,665 | pillar shaft. 14 pieces |
| 15 | `Candle/StaticMeshes/Architecture/SM_PillarTop_01` | (-117, -227, 0) to (227, 117, 417) | 1,336 | pillar capital, pivot off its centre (its centre is near (55, -55)). Shaft plus capital is 14.0 m. 14 pieces |
| 16 | `Candle/StaticMeshes/Rubble/SM_Rubble_02` | 89 x 347 x 117 | 2,426 | rubble at the blocked ends |
| 17 | `Candle/StaticMeshes/Rubble/SM_PillarTop_Broken` | 345 x 345 x 413 | 2,125 | fallen capital, dressing |
| 18 | `Static_Mesh/Props/Castle/SM_Throne` | (-6, -115, -2) to (90, 3, 285) | 1,916 | the hall's west gallery |
| 19 | `Static_Mesh/Props/Furniture/SM_Bookshelf_A_01` | (-84, -27, 0) to (84, 32, 201) | 2,614 | the Stormworks' cover rows (24), the Seal Room (2) |
| 20 | `Static_Mesh/Props/Furniture/SM_HallTable_01` | (-149, -69, 0) to (149, 67, 99) | 1,784 | low cover in the hall (2) |
| 21 | `Static_Mesh/Props/SM_Large_Keg` | 113 x 133 x 127 | 3,644 | dressing |
| 22 | `Static_Mesh/Props/SM_Barrel_01` | 54 x 54 x 73 | 5,208 | dressing |
| 23 | `Static_Mesh/Props/SM_Anvil` | 44 x 80 x 40 | 1,678 | Forge |
| 24 | `Static_Mesh/Props/SM_ConicalCauldron_02` | 115 x 103 x 144 | 1,460 | the brazier on the hearth |
| 25 | `Static_Mesh/Props/SM_FirePit_01` | 85 x 89 x 22 | 5,881 | Forge |
| 26 | `Static_Mesh/Props/SM_IronChandelier` | 182 x 175 x 81 | 8,555 | the hall (8) |
| 27 | `Static_Mesh/Props/Chains/SM_Chain_XtraLong` | 579 x 6 x 73 | 8,180 | the rune cage (4) |
| 28 | `Static_Mesh/Props/SM_Torch_A` | 18 x 13 x 66 | 2,330 | wall torches, about 36 |
| 29 | `Static_Mesh/Props/SM_WeaponStand` | 76 x 112 x 128 | 830 | dressing |
| 30 | `Static_Mesh/Props/SM_Rug` | 630 x 443 x 1 | 252 | Seal Room, hall |
| 31 | `Static_Mesh/Props/Castle/SM_Book_Stack` | 29 x 64 x 31 | 544 | dressing |

Looked at and left out, with the reason:

- `CurtainWall/SM_Curtain_Wall_01` and the others of that set: 150,000 to 290,000 triangles each, not Nanite. The Hall walls do the same job with 23,000.
- `Cobblestone_Road/SM_Cobbles_8M`: 311,000 triangles for one 8 m tile.
- `A/DoorWays/SM_CastleTower_Wall_Door_A_01`: its doorway is about 1.4 m wide and 2.4 m high, too small for the large enemies.
- Candle `SM_Arch_01` (8 m arch): the opening is 3.78 m wide but only 2.36 m high at the top.
- `Courtyard/SM_Stairs_5M`: 1 m wide and 45 degrees.
- `Castle_Gate/SM_Front_Arch` (opening 3.93 x 5.55 m): a good main gate, but nobody ever sees the gate from outside; one mesh less to convert.
- Candle walls (`SM_CorridorWall_01` 800 x 750, `SM_WallSideHall_01` 800 x 1400): low-poly and a fine fallback if the Hall walls convert badly to pixel art; the layout would need 94 pieces instead of 47.

### 6.2 Base, `High_Tech_Base` (15 meshes; each pulls 20 to 110 MB of textures, largely shared, see section 8)

| Asset (under `High_Tech_Base/Models/`) | Bounds min to max | Use |
|---|---|---|
| `SM_Wall_01` | (0, -16, 0) to (400, 38, 600) | wall panel 4 x 6 m: accent bays on the box walls |
| `SM_Wall_03` | (-141, -101, -2) to (1, 41, 600) | column 1.42 m square, 6 m: the hall's columns (two stacked), the yard masts |
| `SM_Door_02_Side` | (0, -49, 0) to (1200, 0, 600) | frame of the wide doors, 12 x 6 m. **One collision hull: place it without collision** |
| `SM_Door_02` | (0, 0, 0) to (850, 49, 425) | door leaf 8.5 x 4.25 m: the wide doors' opening is this size |
| `SM_Hero_Floor_01` | (0, -600, -100) to (2600, 0, 0) | dock 26 x 6 m, 1 m high. The pivot is on its top: place it at Z 100 |
| `SM_Bridge_01` | (-200, -18, -282) to (0, 418, 1) | gantry module 2 x 4.18 m. The pivot is on the deck: place it at Z 282. Solid underneath (one hull) |
| `SM_Railing_02` | (-1200, 0, 0) to (0, 24, 133) | railing 12 m |
| `SM_Prop_HS` | (-107, -110, -124) to (107, 105, 104) | machine block 2.2 m: cover. The pivot is in the middle: place it at Z 124 |
| `SM_Prop_03` | (-5, -392, -3) to (174, 4, 235) | console 1.8 x 3.9 x 2.4 m |
| `SM_Wall_Cargo` | (-600, 0, 0) to (0, 0, 500) | a flat plane 6 x 5 m: the face of the box containers |
| `SM_Lighting_Ceiling_01` | (0, 0, 0) to (1150, 1900, 75) | ceiling light panel |
| `SM_Pipe_03` | (-25, -1750, -25) to (25, 0, 25) | pipe 17.5 m |
| `SM_Top_Cable_System` | (-2, -1, -90) to (2403, 600, 94) | cable trays under the ceiling |
| `SM_Structure_01` | (-629, -733, -3) to (6, 5, 334) | canopy, optional dressing |
| `SM_Reinforcement_01` | (-1, -8, -15) to (120, 1007, 550) | brace around a 10 m opening, optional |

Left out on purpose: `SM_Base_Hero_Prop_01` (the red container: 105 MB of textures of its own, one 8192 map), `SM_Floor_04`, `SM_Top_01`, `SM_Ventilation`, `SM_Concrete_Wall_02` (26 to 44 MB of their own each). Floors, ceilings, walls and containers are boxes with the `MaterialsScifi` panels instead.

### 6.3 Base, lab kit `SciFiWorld` (19 meshes)

| Asset | Bounds min to max | Use |
|---|---|---|
| `SciFiWorld/Modules/Meshes/SM_MWall03-700x350-1` | (-1, -350, 0) to (5, 350, 350) | lab wall 7 m, a thin panel with one good face |
| `.../SM_MWall03-350x350-1` | (-1, -175, 0) to (5, 175, 350) | lab wall 3.5 m |
| `.../SM_MWall07-350x350-1` | (-91, -175, 0) to (146, 175, 350) | wall with a built-in screen console, 2.4 m deep: optional, in place of a short panel |
| `.../SM_MFloor02-700x350-1` | (-175, -350, 0) to (175, 350, 1) | lab floor |
| `.../SM_MFloor02-350x350-1` | (-175, -175, 0) to (175, 175, 1) | lab floor |
| `.../SM_MCeiling03-2-350x700-1` | (-350, -175, 0) to (350, 175, 4) | lab ceiling (not over a portal: the frame is 400 high) |
| `.../SM_MGate01-500x350-1_GateFrame` | (-29, -250, 0) to (29, 250, 350) | gate frame; opening 392 wide, 303 high |
| `.../SM_MGate01-500x350-1_GateL`, `_GateR` | 31 x 280 x 308 each | the two leaves: the locked state |
| `.../SM_MPillar02_1` | (0, -25, 0) to (100, 25, 350) | pilaster between wall panels |
| `SciFiWorld/Meshes/SM_LabCapsule11_1_Capsule`, `_Glass` | (0, -159, 0) to (325, 159, 349) | **the Valkyrie's cryo tube**, at scale 1.3 (4.2 x 4.1 x 4.5 m). The pivot is at the back edge |
| `SciFiWorld/Meshes/SM_LabCapsule05_Capsule`, `_Glass` | (-111, -126, 0) to (142, 126, 328) | smaller tubes along the hall's south wall |
| `SciFiWorld/Meshes/SM_ControlPanel01` | (-35, -37, 0) to (29, 37, 130) | consoles; the cryo release |
| `SciFiWorld/Meshes/SM_LabDesk03_2` | (0, -120, 0) to (126, 120, 90) | dressing |
| `SciFiWorld/Meshes/SM_LabHoloProjector01_Base`, `_Volume` | 144 x 144 x 30 | hologram over the seal (with `HologramShader`) |
| `SciFiWorld/Meshes/SM_LabBarrels01_1` | 39 x 39 x 112 | low cover in the corridor |

The pack's alien creatures in capsules are not used (the Valkyrie is the only one kept on ice, and it avoids any doubt about the Teen rating).

### 6.4 Materials and effects

- `MaterialsScifi/Materials/Instances/MI_Scifi_Panels_06` to `_14`: nine panel materials with 4096 textures. Pick three or four by eye for floor, wall, ceiling and one accent; I did not see them rendered. Three of them cost 75 MB on disk before any cap.
- Dressing effects, a few of each at most: `SciFi_Impacts` (sparks and plasma burn decals in the Breach Corridor), `GunFX` and `MuzzleFlash3D` (already the guns'), `HologramShader` (the seal, the tube's read-outs), `Smoke_NiagaraFluids` (see the warning in section 8).

## 7. Delete list (`CastleTown`)

**Result of the check:** the 31 meshes above depend on 70 more packages (25 materials and material functions, 45 textures). Together that is **101 packages of 1,797**, listed in `castle_keep_list.txt`. Nothing outside `CastleTown` refers to any package that would be deleted (checked through the asset registry's referencers, hard and soft), and none of the kept packages lives in a folder marked for deletion below.

**Delete whole** (no kept package inside):

- `CastleTown/Blueprints`, `Cinematics`, `Demo`, `Levels`, `Scanned_Foliage`, `SkySphere`, `VFX`, `Water`
- `CastleTown/Candle/Materials`, `Candle/StaticMeshes/Props`, `Candle/Textures`, `Candle/VFX`
- `CastleTown/Materials/Decals`, `Materials/LandscapeLayerInfo`, `Materials/MaterialFunctions`, `Materials/MaterialLayer`, `Materials/Instances/Foliage`, `Materials/Instances/Fruit`
- `CastleTown/Static_Mesh/`: `Bloking`, `Brick_Kit`, `Building_Kit`, `Cobblestone_Road`, `DockWalls`, `Flags`, `Foliage`, `Gangways`, `Hills`, `Mountains`, `Rock_Kit`, `Stone_Fence`, `Village`, `Castle_Kit/CurtainWall`, `Castle_Kit/Buildings/Arch_Walkway`, `Castle_Kit/Buildings/Internal_Floors`, `Castle_Kit/Buildings/Roofs`, `Castle_Kit/Buildings/A/DoorWays`, `Castle_Kit/Buildings/A/Windows`, `Castle_Kit/Buildings/WindowsandDoors/Boolean_Tools`, `Props/Candles`, `Props/Crates`, `Props/Fruit`, `Props/Lanterns`, `Props/Market`, `Props/Ropes`, `Props/carts`
- `CastleTown/Textures/`: `Cobbles`, `Decals`, `Flags`, `Flat`, `Foliage`, `Fruit`, `Landscape`, `Masks`, `Moss`, `RVT`, `Rock`, `Roof_Tiles/T_Tiles_Moss`, `Sandstone`, `Slabs`, `Stone_Brick`, `Temp`, `Tile/Land`, `Trim`, `Unique`, `WallBrick`
- The two loose meshes `Static_Mesh/Castle_Kit/Buildings/SM_Spiral_Stairs` and `SM_Spiral_Stairs_Cap`

**Delete all but the kept ones** (the keep list names them): `Candle/StaticMeshes/Architecture` (keep 2 of 43), `Candle/StaticMeshes/Rubble` (2 of 16), `Materials/Functions` (4 of 15), `Materials/Instances` (11 of 105), `Materials/Instances/BrickKit` (3 of 7), `Materials/Masters` (6 of 27), `Castle_Kit/Buildings/A` (4 of 32), `Castle_Gate` (1 of 7), `Courtyard` (5 of 8), `Hall_1` (2 of 6), `WindowsandDoors` (1 of 12), `Static_Mesh/Props` (9 of 42), `Props/Castle` (2 of 7), `Props/Chains` (1 of 7), `Props/Furniture` (2 of 25), `Textures/Fabric` (3 of 4), `Textures/Other` (9 of 47), `Textures/Props` (3 of 15), `Textures/Roof_Tiles` (3 of 6), `Textures/StoneBricksPacked` (8 of 14), `Textures/Stone_Fence` (3 of 6), `Textures/Tile` (6 of 23), `Textures/Utility` (7 of 8), `Textures/Walls` (3 of 9). `Materials/Instances/Floor` stays whole (1 of 1).

**Outside `CastleTown`, the pack's leftovers** (the day's asset block already calls them orphans): `Content/__ExternalActors__/CastleTown` (6,277 files, 325 MB), `__ExternalActors__/Medieval_Docks` (2,277 files, 72 MB), `__ExternalActors__/Maps` (308 files, 66 MB), `__ExternalActors__/FirstPerson`, `__ExternalActors__/ThirdPerson`, `__ExternalObjects__/CastleTown`, `__ExternalObjects__/Maps`.

**Order:** back up first (`Tools/Jam/Backup-Content.ps1`), delete the maps (`Levels`, `Demo`, `Scanned_Foliage`) and the external actor folders, then the rest, then fix redirectors and validate the 31 meshes. The kept part is about 0.7 GB on disk, against 7.9 GB today.

**After it:** the Water and ChaosCloth plugins were enabled only for this pack's water, its two cloth meshes and its building level instances, all on the delete list. They can go back off; measure the Editor boot before and after, as the project rule says.

## 8. Budget

**Pieces placed** (estimate; "BOX" pieces are engine cubes)

| | Past | Future |
|---|---|---|
| Shell walls | 47 kit pieces | 46 boxes (the collision shell made visible) |
| Floors and ceilings | 72 floor tiles + 6 ceiling boxes | 11 floor boxes + 9 ceiling boxes |
| Pillars, columns | 28 (14 pillars of two) | 24 |
| Cover | 40 low wall pieces, 24 bookshelves, 14 podiums, 2 tables, 2 ice boxes | 14 machine blocks, 8 container boxes, 4 rack boxes, 2 consoles |
| Platforms and stairs | 15 boxes, 10 stairs, 8 wall panels, 3 parapets | 4 docks, 16 wedges, 6 gallery boxes, 2 cores, 10 gantry modules |
| Doors and locks | 5 (portcullis, 4 leaves) + 5 wards | 10 frames, 14 leaves and plugs |
| Lab kit | | 40 wall panels, 29 pilasters, 18 floor tiles, tubes 10 |
| Dressing | about 100 | about 60 |
| **Total actors** | **about 380** | **about 320** |

- **Draw calls:** about 60 distinct mesh and material pairs per era, so instancing folds most of the 350 actors together. Expect 150 to 300 mesh draws in view in the largest rooms, before shadows. Not a risk on the RTX 4080 Laptop.
- **Triangles:** the castle shell is the heavy part: 47 x 23,000 = 1.1 million, all Nanite. The base is under 100,000 without the corridor model (324,000 vertices as delivered by its author; decimate it on import).
- **Lights:** about 50 point lights per era, at most two that cast shadows in any one room (hearth, cage, seal, tube); the rest without shadows. Lumen is on, so emissive panels do much of the base's lighting for free. Sun, sky light and fog belong to each era's sub-level. The hangar: 8 spot lights, 2 with shadows.

**Warnings against a package near 1 GB** (it is 299.7 MB today)

1. **`High_Tech_Base` textures.** The 22 meshes I first measured pulled 209 textures, 647 MB on disk, mostly 2048 and 4096; the 15 kept share most of them. Uncapped, they could add 0.5 GB or more to the package. **Cap the size of world textures at 1024 for the cook** (the `TEXTUREGROUP_World` maximum size in the device profile; a config change, no asset is edited). Our own pixel-art textures are far below that and are not affected.
2. **Castle textures.** The kept closure has 45 textures, 624 MB on disk, the largest 4096 (`T_stoneSurface_ORMH` alone is 96 MB). The same cap handles it until the pixel-art conversion replaces them.
3. **`MaterialsScifi`.** 25 MB per material at 4096. Use three or four, not nine.
4. **`SM_Castle_Door`** has 31,241 triangles for a door leaf; fine with Nanite, worth a note for the conversion.
5. **`Smoke_NiagaraFluids`** is a GPU grid simulation; its seller warns about the cost. Do not place it live in a fight room. Bake it to a flipbook with `GBPixelVfx`, as GDD 3 already says, or use the pack's fog cards idea with a plain translucent card.
6. **`GunFX`** is 833 MB on disk; only the systems a map or a weapon refers to are cooked. Reference a handful, never a folder.
7. **Video memory in the Editor.** One of the HTB thumbnails was saved with "Video memory has been exhausted" printed on it: the texture pool was already full on the machine that made it. The cap above helps here too.
8. **The corridor model** has no UVs and procedural materials (asset block of Nov 13): its baked textures are new weight. One 2048 set for the whole corridor is enough.
9. **Substrate.** `High_Tech_Base` materials were authored for Substrate and the project runs with it off. Check one wall and one emissive panel in the level before building on them; the fallback is the `MaterialsScifi` panels on the same meshes.

Measure with a real package after the first dressed slice (build order, step 4), not at the end.

## 9. Build order

The aim is a playable slice on the first evening and a second one the next morning, with the dressing after.

1. **Maps and shell (about 1 hour).** One persistent map with the floor slab, the navmesh bounds, the player start, the portals and the encounters; sub-levels `..._Past` and `..._Future` (the era system already swaps levels with those endings). Generate the collision shell and the structures of both eras from `world_layout.json` as plain boxes and wedges. This greybox is already the whole site.
2. **Slice A, the opening in both eras.** Gatehouse, Courtyard, Great Hall, Seal Room; Seal Vault, Containment Hall. Needs the seal (code need 1) and the gun carrier (code need 3). `Opening_Past` and `Opening_Future` exist. Human play-test.
3. **Slice B, one arena per era and the first loop.** Forge with `Zone_Fire`, portal P2, Generator Hall with `Wave_Future_2`, the Yard with `Wave_Future_1`, portal P0. Human play-test, and measure the navmesh after an era swap (code need 4).
4. **Dress slices A and B.** Castle: the 47 shell pieces, pillars, low walls, stairs, the hearth. Base: panel materials on the boxes, columns, docks, the tube. Cap the textures, package, read the size. Renders to the owner before going on (team rule).
5. **West wing.** It is EAST mirrored (`Y` to `-Y`), with bookshelves and racks. Add the two rune plugs (U1, U2).
6. **North-west and north-east.** The two past zones, the two future corridors, portals P4 and P5, the barred doors D6 and D7.
7. **The Valkyries and the end.** The pylons, the cage and the console (code need 2), the final portal (code need 6).
8. **The south.** Checkpoint, Breach Corridor (a box until the model is imported), Antechamber, Hangar, the Herald (code need 3).
9. **Last pass.** Lights per era, pickups, checkpoints, dressing effects. Remove `Air_DoubleJump` from the starting spells. Full smoke, full build, package.

**If time runs short, cut in this order:** the two corridor fights (keep the corridors empty); the hangar's dressing; the lab kit in the two north corridors (plain boxes); the Breach Corridor model (keep the box); and last, move the Herald into the Yard as the last sub-wave of `Wave_Future_1` and drop the whole south wing. The critical path survives every one of these cuts, because no flag depends on the dressing.

## 10. Code needs

What the plan needs that the code does not do today (read from `World/`, `AI/GBEncounter.*` and the data files).

1. **The seal.** No class exists. An interactable that plays cutscene 1, sets `SealUsed` and calls `RequestTravel(Future)` without teleport.
2. **Starting an encounter by interaction, with prerequisites.** `AGBEncounter` starts from its trigger box or from `StartEncounter()` in code. The two Valkyrie fights need an interactable (the cage, the console) that calls it, and a list of required flags: four zones for the past, `Valkyrie_Past` and the Herald for the future. A `RequiredFlags` array on the encounter and a small starter actor cover both.
3. **Data.** New encounters `Skirmish_Signal`, `Skirmish_Breach` and `MiniBoss_Herald`; a gun carrier enemy for `Opening_Future` (an armed orc whose drop is the pistol at 100%, first in the first wave, spawning at the first spawn point) whose pickup sets `GotFirstGun` (if a dropped pickup cannot carry a flag, that part is code); a Herald entry using the `GBEntity` actor class with mini-boss numbers. If spawn points are not used in list order, the gun carrier needs that guarantee in code.
4. **Navigation across an era swap.** The navmesh is dynamic and rebuilds when collision changes. With the shell and the shared structures in the persistent level, only each era's cover and platforms change. Measure the rebuild after a swap in the largest room: the fade lasts 1.05 s and the first enemies must have paths when the player walks in. If it is too slow there is a fallback without code: place the future sub-level at a fixed offset and let each portal teleport by that offset (`bTeleport` and `Destination` exist), so both navmeshes are always there.
5. **Era lighting in a real map.** The test arena changes the sun, sky and fog in code. In the map, each era's sub-level carries its own; check that a hidden streaming level's directional light, sky light and fog really stop contributing, else reuse the arena's `ApplyEraLighting`.
6. **The final portal.** An interactable present when `FinalPortalOpen` is set, which opens the Entity map with the player's snapshot.
7. **The real map as the game's map.** The game mode builds the test arena when a map has no player start; the site map needs a player start, to be the default map, and its own smoke steps.
8. **Flag on the encounter's start** (optional). `Encounter.<Id>.Started` would let the cryo tube open and the arena doors shut when a fight begins. Without it the tube stays shut as cover and the doors stay as they are.
9. **Kit collision** (asset settings, not code): only if the kits carry collision instead of the shell, see section 2.

Already covered, no code: era-gated barriers, wards, rune plugs, pylons, the chests that appear after a challenge and the portals that light up later are all an actor plus a `UGBEraComponent`; a door with two conditions is two barriers in the same doorway.

## 11. Provisional

Every choice below is mine and waits for the owner's word. None blocks the build: each is a flag name, a position or a data entry.

- **The Valkyrie's arena is the Great Hall / Containment Hall**, and the future intro fight happens in that same hall. The brief lists a Valkyrie arena as its own zone; merging it saves a room in each era and puts her at the centre of the site in both. A separate arena would go north of the Seal Room.
- **The keep's future name and role** ("Containment Hall", a research block around the seal and the frozen Valkyrie) instead of the owner's example of a command block.
- **Order and locks:** the wings' wards fall when the seal is used; the Air and Ice zones are entered from the future; the Valkyrie's cage needs all four zones; her tube needs her past defeat and the Herald's; the Herald's death lifts the lockdown; the breach gate needs both wing waves.
- **The crossed unlocks** (lightning opens the way to air, fire opens the way to ice).
- **The final portal on the seal.**
- **The hangar to the south, beyond the gate**, and the antechamber with two doors.
- **"The Herald"** as the name of the tentacle machine as a mini-boss, and that it shares its asset with the Entity's first phase (GDD 11): a story question for the owner. Its fight length (75 s) and numbers.
- **All zone names** (Forge, Stormworks, Frozen Cistern, High Ward, Generator Hall, Server Hall, Signal Corridor, Cryo Corridor, Breach Corridor, Checkpoint, Yard).
- **Rewards:** the rifle from the Herald's crate, the shotgun in the checkpoint crate, the ember staff in the fire chest, every pickup and its place.
- **Encounters:** which existing encounter goes in which room, the two corridor skirmishes and their sizes, every spawn point and trigger box.
- **The hub portal P0** in the middle of the courtyard and yard.
- **The ice floor of the cistern is only a look** (no sliding).
- **Lighting moods** per zone and the time of day of each era (late afternoon, night).
- **The length:** 22 to 25 minutes for a first run.
- **Not using the pack's alien creatures.**

## 12. What was not verified

- Nothing was placed in a level: piece orientations (the yaw in `castleWallPieces`) follow from the measured bounds and Unreal's rotation rule, and the first piece of each kind should be checked in the viewport before the loop runs.
- The door opening of HTB `SM_Door_02_Side` is taken from its leaf (`SM_Door_02`, 850 x 425); the frame is too low-poly for the vertex method that measured the other doors.
- The stairs' walkability is inferred from their bounds, their vertex profile (a steady rise) and their single collision hull; nobody walked them.
- The interior width of the corridor model is unknown until it is imported; 530 is its outer width.
- Package sizes in section 8 are estimates from sizes on disk.
