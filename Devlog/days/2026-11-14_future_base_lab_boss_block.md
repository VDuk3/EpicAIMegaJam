# 2026-11-14, block: future base, the owner's third walk (lab room back, sub-boss hall, spells that always appear)

Architect: Claude Code (Fable 5.2) on CENTRAL. Owner's notes in Spanish, translated here. Continues `2026-11-13_future_base_slice1_block.md`.

## What the owner sent
Three screenshots from the package and three notes, then two more messages while the work was running:

> 1. Era mantener la sala pero quitar las figuras y hologramas y reemplazarlo con assets que dejen a la sala como una generica.
> 2. Despues de este pasillo has una sala para un sub jefe. Sala amplia, alta, con barandas donde pueda haber enemigos caminando y disparando, y una puerta del otro lado. Dentro de la sala, algunas cajas y algunas de ellas apiladas en altura. Siempre a altura de salto del jugador, y que saltando en algunas cajas, el jugador pueda llegar saltando a las barandas de los lados.
> 3. Arregla esto, la caja tiene una transparencia extraña.

Translation: (1) The idea was to keep the room but take out the figures and the holograms and replace them with assets that leave the room a generic one. (2) After this corridor make a room for a sub-boss: a wide, tall room, with railed galleries where enemies can walk and shoot, and a door on the other side. Inside, some crates, some of them stacked. Always within the player's jump, and by jumping on some crates the player can get onto the side galleries. (3) Fix this, the crate has a strange transparency.

> En el último pasillo, el hechizo de muro de hielo no aparece si acierto todos los QuickTime Events. [...] El muro tiene que aparecer sí o sí, independientemente de esas cosas.
> cualquier hechizo deberia suceder independiente de esas cosas

Translation: in the last corridor the ice wall does not appear when I get every quick-time event right; done half wrong it does, with a smaller size, so we assume the wall would be bigger than the corridor. The wall has to appear no matter what. Any spell should happen regardless of those things.

## 1. The room: the architect had misread him
On 2026-11-13 he said of the lab room "the room is fine, but do not use those assets, they are for the Valkyrie's fight". The architect took "those assets" to mean the whole lab kit, pulled every piece of it out of the map and built a muster hall from another kit in its place. He meant the figures. Worse, the muster hall's entry was walled up: one of its own dark backing boxes ran the full width of its end wall, across the doorway. The builder's walk check only looked for holes in the floor, and the tour teleports, so neither saw it. His first screenshot is that door.

**Now:** the lab kit's room is back where it was (through the far door of the air plant's gallery, 4 m up, 22 x 32 m), placed as its author built it, minus 24 pieces: the 12 alien figures, the 6 hologram projectors and the 6 specimen tubes (the tubes are the architect's extension of his words: an empty tube still says "specimen lab"). In their place, 20 pieces of what the room already has: cabinets, barrels and a work desk with its screens and chair along the long wall, a second control panel and a cabinet on the dais, and five soft lamps where the tubes used to light that wall. No fight in it for now (his earlier word).

**So it does not happen again:** the builder's walk check now also traces each walk at chest height and reports anything standing across it (doors that open are left out). The memory note about the lab kit was rewritten: only figures, holograms and tubes are reserved.

## 2. The sub-boss hall
Behind the long corridor's far door. That door was part of the corridor's body mesh; `corridor_blender_export.py` now writes its two leaves as meshes of their own and the map places each as an `AGBDoor` that slides sideways into the wall when the player is within 4.5 m.

The hall (`Build-FutureBase.py`, stage `boss`), every choice tied to his sentence:

| He asked for | Built |
|---|---|
| wide, tall | 28 x 36 m, 12 m high: two tiers of the base kit's 6 m wall modules, the freight branch's floor strips, the kit's ceiling with two of its lit panels |
| railed galleries where enemies can walk and shoot | a gallery 4 m up and 4 m wide along each side wall, joined by a bridge over the entry (the player comes in under it); the same grated deck, posts and mesh railing as the transfer bay's catwalk |
| a door on the other side | the kit's freight door in its surround, shut; it opens on the progress flag `FutureSubBossDefeated`, which nothing sets yet |
| some crates, some stacked | 13 cover crates in five groups, three of them with a crate on top; a stepped pile of one, two and three in the far corner; the middle of the hall clear for the fight |
| always within the player's jump | the jump rises 2.04 m (`Player.json`: 800 cm/s, gravity x1.6); every step is 1.5 m or less |
| jumping on crates gets you onto the galleries | no stair. At four places a two-crate stack (3 m) stands against a gallery (4 m) with a single crate (1.5 m) beside it, and the railing has a 2 m gap there |

The sub-boss itself does not exist yet: the hall has no encounter (provisional, marked TBD in the GDD).

## 3. The crates with "a strange transparency"
The Spaceship Corridor's two crates are shells one face thick, with some faces looking inwards. Blender draws both sides of a face, so the renders the owner approved looked whole; Unreal draws one side, and half of each crate was missing. The four crate materials now come from a two-sided master (`M_ShipCorridor_TwoSided`). Checked that no object of the model has a mirrored scale (none), so it is the model's own faces, not the export.

## 4. The ice wall that did not appear
`AGBSpell_Wall::StartSpell` looked for the ground by tracing down from **the wall's own height above the caster**. At full strength the wall is 3.2 m tall; the corridor is 3.3 m high outside and the player's middle is about 0.9 m up, so the search started above the corridor's roof, found the roof, and raised the wall on top of it, out of sight. A weaker cast makes a shorter wall and the search started inside. So the owner's guess (too big for the corridor) was right, by way of the ceiling rather than the width.

Fix, in the shared spell code, because the zone and trap spells searched the same way from 1 m above the aimed point:
- `AGBSpellActor::FindGroundUnder(Point, Lift, ...)`: the search may start up to `Lift` above the point, but it first traces up and never starts through a ceiling.
- The wall searches from the caster's own height at the wall's place, and if something solid is nearer than its distance it stands just in front of that, never on top of its caster.
- Every spell actor already spawned with "always spawn" (checked all 8 spawn calls in the game code), so nothing else could refuse a spell.

**A check that can fail:** the tour has a spot named `24c_long_castwall` in the corridor. There it casts the ice wall as if every quick-time step had been right (`UGBSpellbookComponent::DebugCastAtFullStrength`) and fails the run unless exactly one wall stands with its base at the player's feet.

**Exception to the implementer rule:** the architect wrote the C++ directly (about 60 lines in three spell files and the tour check). The cause was already found while reading the code and the fix is one helper.

## Proof that the diagnosis was right, and that the check can fail
The same tour was run on two builds of the Editor target, both with the new check:

| Wall code | What the tour said at the corridor spot |
|---|---|
| as it was (the three spell files put back from the last commit) | `the ice wall at full strength is not on this floor (1 walls, base 314 cm from the feet)`: the wall stood on the roof, 3.14 m above the floor |
| fixed | `the ice wall at full strength (320 cm tall) stands on the floor, -2 cm from the feet` |

A mistake on the way: after the first run the fixed files were copied back with their old dates, the build saw nothing new ("Build time: 3 s") and the second tour ran the old code again. The three files were touched and the build repeated (43 s).

Also found by this run: the tour's safety quit was 150 s after its start, and 44 spots take 154 s, so the last two spots and the block's verdict were cut off without a word. The safety quit is now at 400 s; the tour still quits by itself when it is done.

## What the walk in the game showed (the architect's own review)
- A review spot placed on one of the corridor's side benches dropped the player out of the map: the model is a shell seen from inside, and whoever gets inside a bench falls through its underside. An unseen floor now lies under the corridor's whole width.
- The lab room reads as a control room: desks, screens, cabinets, barrels; nothing alien in it. The gallery's door opens onto it.
- The sub-boss hall: the entry under the bridge, both galleries, the four piles, the exit door with its two red lamps. Nothing changed after the first look.

## Code review
Sol 6.1 (medium, read-only, through the logged wrapper; prompt `Prompts/review_wall_ground_boss_hall.md`), three findings:
1. **Real, fixed.** A caster with the face against a wall: the wall spell kept its distance from the caster, so it went into the obstacle, out of sight: the one thing the owner ruled out. Now, when there is no room in front, the wall stands where the caster is, in sight, and the two do not block each other.
2. **Left.** The wall does not stop in front of an earlier ice wall (those are moving-world objects, and the trace looks at the static world). Tracing the moving world too would let a passing shot or a pickup shorten the wall; two walls through each other are both in sight, which is what was asked.
3. **Fixed.** Two "castwall" spots in a row would count each other's walls: the check removes earlier walls before it casts.

## Checked
- Editor target builds; **50 tests green** (0.81 s of test time); no new test: the fix is about traces in a real room, which the tour's check covers in the game itself.
- **Tour: 44 spots pass** on the Editor build and on the package, the ice wall check among them. Packaged full arena smoke clean.
- `TeamTBD_Glyphbound.zip`: **1,330.8 MB** (was 1,226.2 MB: the lab kit's room is cooked again). Still over the jam's 1 GB; the owner set the limit aside for now, and it will have to be met before the delivery.
- Nobody has played it by hand yet. The crate routes up to the galleries are right by the numbers (1.5 m steps against a 2.04 m jump) but only a person jumping them can say they feel right.

## Media (all from the package)
- `media/2026-11-14/futurebase_subboss_hall.png`: the hall through the corridor's open door, from the entry, the middle, the floor by a west pile, the west gallery and the bridge.
- `media/2026-11-14/futurebase_lab_room_generic.png`: the gallery's door open onto the room, the room, its long wall, its dais.
- `media/2026-11-14/icewall_full_strength_before.png` and `..._after.png`: the same spot and the same cast; before, an empty corridor (the wall is on the roof); after, the wall across it.
- `media/2026-11-14/corridor_crates_two_sided.png`: the corridor's crates, whole.

## Left open
- The sub-boss, its encounter and what sets `FutureSubBossDefeated`.
- The first fight of the future (`Opening_Future`) is still switched off in the lab room.
- Enemies have a nav mesh volume over the hall, but nothing has walked the galleries yet.
- The package's size.
