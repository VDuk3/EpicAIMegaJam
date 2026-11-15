# 2026-11-15 - The future base's warm corridors brought into its cool palette

The owner, with a screenshot of the corridor right after the spawn room (translated): "Right after that room there are all those warm lights. Look at how the cool lights of the rest of the map are, except the underground zone, and change the colours so it is all coherent." Done by the side session that made the katana (Claude Code, Opus 5.5), directly; the shared Editor was coordinated with the orchestrator (FutureBase opened only after the smoke session had saved, dated backups first, only FutureBase and the new material copies saved, EntityArena reopened).

## What the map's light looked like
Outside the underground wing the future base is lit in cool cyans and blue-whites (`BAE4FF` points, `65EBFF` and `8AE4FF` rects, `AAD7FF` spots). Two places broke it: the entry corridor and transfer bay after the spawn room (18 orange floor rects `FF6D37`, 18 warm ceiling rects `FFD7B9`, a few warm whites and two red points, and orange-glowing frame slats, ceiling strips and floor strips), and the damaged ship corridor past the gate room (10 red points, warm-white rects, warm-white and amber light strips).

## What changed
- **Lights:** orange became the map's cool accent (`65EBFF`), warm whites became cool white (`CDE8FF` rects, `BAE4FF` points): 42 lights in the entry corridor and bay, 15 in the ship corridor.
- **Glowing surfaces:** never the kits' own assets (the underground wing reuses them, and a shared edit was refused). Cool copies set only on the components of these two zones: `MI_ShipCorridor_LightCool` and `MI_ShipCorridor_LightAccentCool` for the ship corridor's strips; `MI_Emissive_01_Cool` for the frames' slats; `MI_Tile_Machine_01_Cool`, `MI_Sci_Fi_Electric_Cool` and `MI_NonSquare_Floor_Lighting_Cool` for the strips whose glow is an orange texture. For those, the project's own master `M_GBKit_Surface` gained an optional tint (`EmissiveTintAmount`, 0 by default so nothing else changes; at 1 the texture's brightness is shown in `EmissiveTint`).
- **Untouched:** the underground wing, the corridor smoke (FutureSmoke) and the tentacle boss.
- Reproducible with `Tools/Jam/future_corridor_cool.py` (`corridor_cool()` and `entry_corridor_cool()`). Before and after renders of both corridors were checked; in-game Lumen look still to be seen in PIE.
