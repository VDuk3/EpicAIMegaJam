Review the uncommitted diff of Glyphbound/Source/Glyphbound/UI/GBHUD.cpp and Glyphbound/Content/Data/Weapons.json
(git diff -- those two files) for correctness bugs only. UE 5.9 C++ jam game, first-person HUD drawn on a Canvas.
Goal: no first-person hands at all (staff, sword, empty-handed, casting, left hand, mantle/climb), only the weapon
sprites; the new hand-drawn staff sprites (22x90, twice the old 10x45) drawn with SpritePixelScale 0.5 so they keep the
old on-screen size; two new staffs (Staff_Frost: Ice, Staff_Storm: Lightning) cloned from Staff_Ember.
Look for: null derefs (Item, Staff texture), compile errors or warnings-as-errors from now-unused locals, a shield or
staff no longer drawn when it should be, weapon stats keys the code does not read (IceBuildUp, LightningBuildUp,
SpritePixelScale through AGBHeldItem::GetStat), ProjectileSprite ids that do not exist in Content/Data/Sprites.json.
Report each finding with file:line, a concrete failure scenario and a fix. Short; no style comments.
