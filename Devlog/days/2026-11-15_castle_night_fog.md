# 2026-11-15: the castle at night, under a full moon, in fog

The owner (Spanish): *"Haz que sea noche de luna llena en el mapa de castillo medieval y cubre de niebla impenetrable los alrededores."*
Translation: make it a full-moon night on the medieval castle map and cover the surroundings with impenetrable fog.

## The night (`Tools/Jam/Build-PastCastle.py`, stage `world`)
- The sun becomes the **moon**: a directional light of 0.9 lux, cold blue-white, 27 degrees up, placed so it hangs above the keep as seen from the road. It is the sky atmosphere's sun light, so the atmosphere draws its disc (made larger, 2.2 degrees, and brighter) and turns the sky a night blue.
- Sky light at 2.0 (a moonlit ambient, so shadows are not black); height fog thin and dark blue; auto exposure with a bias of -1 and a range of EV -0.5 to 1.5. Tried and dropped: a floor of EV 0.3 left the halls and passages black (picture sheet looked at, every spot).
- The torches (the castle's lights since Nov 14) now carry the night. The camp fire was cut from 260 to 60 cd; the spot next to it still reads as a strong pool of firelight, because the eye adapts to it (the exposure follows the fire whatever its strength).

## The fog
- **First try: the engine's Local Fog Volumes.** A ring of 30 around the site drew nothing. Tested down to one volume in front of the camera, uniform scale, bright red emissive, with and without the new `r.Fog.SeparateComposition`: nothing in the game, nothing in Editor captures. The volumes were in the scene and visible, and the project supports them (`r.SupportLocalFogVolumes 1`). Not solved; dropped in favour of something under our control.
- **The fog bank:** four layers of vertical curtains around the site (`FOG_LAYERS`: 4, 11, 18 and 26 m outside it), each a plane with `M_NightFog` (`Tools/Jam/make_night_fog.py`). The material is unlit and translucent. It has two layers of the engine's tiling noise in world space (banks of about 5 m and wisps of about 1 m, drifting in opposite directions), is thick at the ground, fades out between 25 and 55 m up, and blends softly where it meets the ground. The inner curtains are partly transparent; the outer one is opaque. The open ground inside it (`SITE`) starts 38 m in front of the gate passage (22 m behind the player's start on the road) and ends 30 m beyond the keep's back wall, and reaches 20 m beyond each side curtain wall (48 m from the castle's axis).
- **Impenetrable:** invisible walls stand 7 m outside the site, past the first curtain and before the second, so the player can step into the edge of the fog and no further.
- **Sol 6.1 review** (medium, read-only, Codex CLI through the logged wrapper; prompt `Prompts/review_castle_night_fog_sol.md`) found one real defect: the noise also thinned the outer curtain, which let up to 41% of what lies beyond show through. Fixed with a `NoiseOpacity` parameter, which is 0 on the outer curtain (fully opaque) and 1 on the inner ones.

## Checks
- Builder: `walks: clear on 12 walks` after every rebuild.
- Tour (Editor build), five rounds of pictures looked at close. The ward, wall walk and stair landing show the moon above the keep, a dark-blue sky, and a grey-blue fog bank hiding the horizon beyond the walls. 
- Full smoke (`Smoke-Jam.ps1 -Offscreen`): 0 engine errors and two LogGB errors outside the castle, both in `Enemies3D` in the test arena (`Drone_Future is 2 cm above the ground` and `two members stood within 200 cm of each other for 4.2 s`). Run again alone (`-Only Enemies3D`): 0 errors. These are flaky steps of the enemies; this change does not touch them. The drone one was already recorded on Nov 14.
- Implementer: the architect directly (map-builder work inside the castle architect's own tool chain).
- After the Sol fix: fog material regenerated, package `BUILD SUCCESSFUL`, zip **925.8 MB**; packaged castle tour: 42 spots, 0 without floor, the seal's map can be opened, 0 engine errors. The outdoor pictures of the packaged game were checked again: the bank still reads as mist (its colour keeps the noise; only the outer curtain's opacity is solid) and fades into the sky above. Pictures (packaged game): `media/2026-11-15/castle_night_outdoor_sheet.jpg` (gate, ward, camp fire, stair landing, wall walk), `media/2026-11-15/castle_night_03_ward.png`, `media/2026-11-15/castle_night_37_fog.png`.
- The fog material and its four instances (`/Game/Glyphbound/Castle/M_NightFog`, `MI_NightFog_0..3`) are Content, which is not in git; `Tools/Jam/make_night_fog.py` remakes them.
