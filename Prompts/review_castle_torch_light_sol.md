# Review: torch light offset (read-only)

Unreal Engine 5.9 C++ (Glyphbound). Report only real defects (crashes, wrong behaviour, things that break in a
packaged game) with file, line and a concrete failure scenario. No style notes. If nothing, say so.

The change (see `git diff HEAD -- Glyphbound/Plugins/GBPixelVfx Glyphbound/Source/Glyphbound/World/GBTorch.h Glyphbound/Source/Glyphbound/World/GBTorch.cpp`):
- `UGBPixelVfxComponent` (`Glyphbound/Plugins/GBPixelVfx/Source/GBPixelVfx/Public/GBPixelVfxComponent.h`,
  `Private/GBPixelVfxComponent.cpp`) gets `FVector LightOffset` (component frame, cm); `UpdateQuadTransform` places the
  clip's point light at the clip's light anchor plus `GetComponentQuat().RotateVector(LightOffset)`. Default zero, so
  existing users (the Fireball's flight and impact clips) must behave exactly as before: check that.
- `AGBTorch` (`Glyphbound/Source/Glyphbound/World/GBTorch.h/.cpp`) has `LightOffset` (default (0, 150, 0)) and copies
  it to its flame component in `BeginPlay` before `Play`. The torch's holder is its root; the flame component is
  attached to it, so its rotation is the torch's (+Y leans out of the wall).
