# Review: castle wall torches (read-only)

You are reviewing a small change in an Unreal Engine 5.9 C++ project (Glyphbound, a game jam entry). Report only real
defects (crashes, wrong behaviour, leaks, things that will not work in a packaged game), each with file, line and a
concrete failure scenario. No style notes. If you find nothing, say so.

## The change
1. New actor `Glyphbound/Source/Glyphbound/World/GBTorch.h` and `.cpp`: a wall torch placed in a level by an Editor
   Python builder (`Tools/Jam/Build-PastCastle.py`, function `torch`). Root = static mesh holder (Static mobility, no
   collision), child `UGBPixelVfxComponent` (from the project's plugin `Glyphbound/Plugins/GBPixelVfx`, Runtime module,
   see `Source/GBPixelVfx/Public/GBPixelVfxComponent.h` and `Private/GBPixelVfxComponent.cpp`) that plays a baked
   flipbook clip with its own point light. `BeginPlay` sets SizeScale, LightIntensityScale, shadows, a random
   StartTimeOffset and calls `Play(ClipId)`. The builder sets `holder`'s mesh, `flame_size`, `light_scale` and the
   flame component's relative location on the placed instances, then saves the level.
2. The bake tool (Editor-only module `GBPixelVfxBaker`): a job JSON can now give `UserStaticMeshes`
   (Niagara user parameter name -> static mesh object path), read in `Private/GBPixelVfxBakeJob.cpp`
   (struct in `Public/GBPixelVfxBakeJob.h`) and applied with `UNiagaraComponent::SetVariableStaticMesh` in
   `Private/GBPixelVfxBakeGameMode_Bake.cpp`.

Questions worth checking: the flame component attached under a Static-mobility root (the clip's quad and light are
created at runtime by the component); whether `Play` in `BeginPlay` is safe for actors loaded with the level in a
packaged game; whether the instance values the builder writes survive save/load (they are UPROPERTY EditAnywhere);
anything in the JSON reading that can crash on a malformed job.

Use `git diff HEAD -- <path>` for the plugin files and read the two new torch files whole.
