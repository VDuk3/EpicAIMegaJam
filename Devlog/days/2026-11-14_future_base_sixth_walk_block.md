# 2026-11-14, block: future base, the owner's sixth walk (four corrections)

Architect: Claude Code (Fable 5.2) on CENTRAL. Owner's notes in Spanish, translated here. Continues `2026-11-14_future_base_fifth_walk_block.md`.

## What the owner sent
Four screenshots from the package and four notes:

> 1. Quita las figuras no holograficas.
> 2. Dale un retoque de mas de vidrio, que no se vea tan uniforme.
> 3. Esto lo tenias que dejar igual, el cambio era que el jugador pudiera montarse ahi.
> 4. Quita el primer tubo amarillo que interrumpe el paso.

Translation: (1) Take out the figures that are not holograms [the Valkyrie's room]. (2) Give it more of a glass look, not so uniform [the breakable wall]. (3) You had to leave this as it was; the change was that the player could get on it [a fallen deck tile, which the architect had laid flat on a crate]. (4) Take away the first yellow tube that is in the way [the ladder's platform at the landing].

## What was done
| Note | What it was | Change |
|---|---|---|
| 1. Figures | The room had the kit's five solid aliens in their tubes and six holograms of aliens on projectors | The solid ones are out (told apart by their material: the holograms use the kit's hologram materials). Holograms, projectors and tubes stay; the tubes are empty |
| 2. Glass | One flat colour and one gloss over 10 x 5.6 m | `M_GB_FrostedGlass` rebuilt: panes 2 m wide in dark seams with one seam across at half height; each pane clouded by a large noise (lighter and darker glass) and a fine one (how glossy it is, 0.1 to 0.5); a faint uneven glow as of light behind. A first version was too much like marble; its contrast was brought down |
| 3. Tiles | **The architect misread the fifth walk's note 6.** "It should work as a platform" meant: let the player get on it. He answered by changing how it looked (flat, on a crate) | Both tiles hang tilted where and how they were. Each has an unseen plate on its upper face: a deck tile's own collision is a grating's, full of holes. The crates under them are gone |
| 4. Yellow tube | The pack piece is a **walkway**: 4.7 m long, railed on both long sides, open at one end, with the ladder on one side near the far end. The architect had placed it sideways against the landing, so one of its own side rails ran across the way onto it | Turned a quarter: it runs straight out from the landing, its open end on the landing's edge, the ladder near its far end. The dock under it is 6 m deep now (it was 4.5) so the ladder's foot stands well inside it; the crates around the tanks and the dock were moved to keep clear of each other |

## Checked in the game
- No C++ changed in this step. **54 tests green.**
- **Tour: 82 spots pass** on the Editor build, with its checks: ice wall, glass wall (three hits, then broken), sockets and gate, and the ladder down (403 cm) and up (408 cm) from its new place.
- The architect's own look at the screenshots: the room with holograms only; the glass in panes; the walkway seen from the landing with nothing across its mouth; the ladder from the dock.
- **The tilted tiles.** The last hall is as dark as the owner asked and the review spots that look at the tiles show almost nothing, so the tour got a spot on the middle of each tile (`68a`, `68b`, placed by the builder with the same maths as the tile): the player stays standing on both, 10 and 7 cm from the mark, after 2.5 s. Whether getting onto them feels right is still for a person to say.
- After a full rebuild with those two spots: **tour 84 spots pass** on the Editor build.
- **Package:** `TeamTBD_Glyphbound.zip`, 1,906.5 MB; packaged tour 82 spots pass with every check; packaged full arena smoke clean. The package was made before the two tile spots were added: it differs from the map by those two review markers and nothing else, so it was not made again (another chat was waiting for the machine).
- Nobody has played it by hand yet.

## Media (from the package)
- `media/2026-11-14/futurebase_sixth_walk.png`: the Valkyrie's room with holograms only (two views), the glass wall in panes, the walkway from the landing, the view while climbing down, the ladder from the dock.

## Left open
- The package's size (almost twice the jam's 1 GB).
- Everything listed as open in the fourth and fifth walks' entries.
