# Review: castle night and fog bank (read-only)

Editor-side build scripts for the castle map of an Unreal Engine 5.9 game (Glyphbound). Report only real defects
(a script error, wrong geometry or placement, a curtain or wall that leaves a gap the player can walk or see through, a
material graph that cannot compile or whose math does not do what the comments say) with file, line and a concrete
failure scenario. No style notes. If nothing, say so.

The change (see `git diff HEAD -- Tools/Jam/Build-PastCastle.py` and the new file `Tools/Jam/make_night_fog.py`):
- `Build-PastCastle.py`, `stage_world`: the sun becomes a dim cold moon (directional light, `MOON`), the sky light,
  height fog and auto exposure are set for night (`NIGHT_EXPOSURE`). Around the site (`SITE`, x0 x1 y0 y1) four
  layers of vertical fog curtains (`FOG_LAYERS`, distances outside SITE; `/Engine/BasicShapes/Plane` turned upright
  with roll 90 and yaw 0 or 90, scaled to the side's length and to `FOG_TOP - FOG_BOTTOM`), and four invisible
  collision walls `FOG_WALL_IN` outside SITE. Check the plane's orientation and scale really give a vertical curtain
  along each side that covers its corners, and that the walls close the rectangle.
- `stage_camp`: the camp fire's light is lowered.
- `make_night_fog.py`: builds `M_NightFog` (unlit, translucent, two-sided) with MaterialEditingLibrary: world-space
  noise (x + y along a curtain, z up) through two panners, a vertical fade between `FogFadeStart` and `FogTop`, an
  `Opacity` parameter, a depth fade; and `MI_NightFog_0..3`. Check the node wiring (input names, the mask nodes,
  the divide by a constant) and that rerunning the script on an existing material does not break it.
