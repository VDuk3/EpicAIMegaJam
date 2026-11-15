# Review: the castle's pixel-sprite enemies and the squad changes (read-only)

Unreal Engine 5.9 C++ game (Glyphbound). Report only real defects with file, line and a concrete failure scenario:
a crash or null dereference, a texture or actor leaked or freed too early, an index out of range (sprite sheet rows,
columns, frames, directions), a division by zero or NaN (clip rate, frame at time, view angle), state that can get stuck
(an attack clip never ended, a pincer wait never released, an enemy that never engages or never leaves a slot), a change
that alters the future's 3D squads although it should be opt-in, smoke checks that cannot fail or measure the wrong
thing. No style notes. If nothing, say so.

The change: new `Glyphbound/Source/Glyphbound/AI/GBSpriteAnimComponent.h/.cpp` (an 8-direction sprite sheet look:
PNG sheets from `Content/Data/Sprites/<Id>.png/.json` read at runtime into transient textures and cached by a
subsystem, a camera-facing quad, clips by speed and by the AI's windup), `Tests/GBSpriteAnimTests.cpp`; edits in
`AI/GBSkeletalEnemy.h/.cpp` (the look abstraction: Look* calls branch between the mesh anim instance and the sprite;
projectile shots), `AI/GBSquad.h/.cpp` and `AI/GBTactics.h/.cpp` (pincer attacks, taking turns, cornering by probing the
player's escape directions; all opt-in by stat), `AI/GBValkyrieEnemy.cpp` (`PlayActionClip`, `FireVolley` with the sprite
look), the "Castle" smoke block in `Core/GBDevCommands.cpp`, and the castle ids in `Content/Data/Enemies.json`.
The sheet format is in `Prompts/sprite_sheet_format.md`. See `git diff HEAD` for the edits and read the new files whole.
