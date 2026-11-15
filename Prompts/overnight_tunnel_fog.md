# Overnight block E: the tunnel smoke reacts to the player and to shots (owner's point 4, game-code part)

Read first: `Prompts/implementer_rules.md` (hard rules apply except the build rule: see "Build"), `Design/ARCHITECTURE.md`.

## The owner's words (Spanish, translated)
"Smoke or fog in the tunnels to the tentacle boss's hall: a Niagara system made with **Niagara Fluids**; it moves
slowly and **reacts to physics: the player walking through it, the shots, the collision with the tunnel**. Quality
above all: no figures, silhouettes or cloud shapes, it must look like real smoke. Watch the cost on the RTX 4080 Laptop."

## The split
- The architect authors the Niagara system `/Game/Glyphbound/FX/NS_TunnelFog` (a Niagara Fluids **Grid 3D gas**
  simulation, from the plugin's `Grid3D_Gas_Smoke` template) in the Editor: its look, the grid, the slow drift, the
  tunnel walls as the grid's closed boundary. Its user parameters: `User.WorldSpaceSize` (Vector3f, cm),
  `User.ResolutionMaxAxis` (int), `User.Emit Position` (Vector3f), and more may be added (`User.FillDensity`...).
- **You write the game side:** an actor that places and sizes the system in a tunnel, and feeds the simulation what
  moves through it, so the smoke parts around the player and is torn by shots and blasts.

## How the gas hears the game: Niagara Data Channels (global)
The Niagara Fluids plugin ships two global data channels the gas emitters can read:
- `/NiagaraFluids/NDC_Grid3D_Gas` with variables `Position` (NiagaraPosition), `Velocity` (Vector3f), `Radius`,
  `PreviousRadius`, `Density`, `Temperature` (floats), `Color` (LinearColor), `Divergence` (float): **sources** (an entry
  adds density/velocity/temperature in a sphere).
- `/NiagaraFluids/DataChannels/NDC_FluidCollision` with `Position` (NiagaraPosition), `Velocity` (Vector), `Radius`:
  **moving collision spheres** (the gas is pushed out of them and takes their velocity).
Write to them from C++ every frame with the engine's data channel API (`NiagaraDataChannelAccessor.h`,
`NiagaraDataChannelPublic.h`, `UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel` or the newer writer API: read
the UE 5.9 headers in `C:\Program Files\Epic Games\UE_5.9\Engine\Plugins\FX\Niagara\Source\Niagara\Public` and find an
engine or plugin example that writes a global channel from C++). Entries must be visible to GPU simulations (the gas
runs on the GPU). Write only while the player is inside or near a fog volume (no cost elsewhere).

## What to build
1. `World/GBFluidFog.h/.cpp`: `AGBFluidFog`, an actor with a box (its `BoxExtent` is the tunnel's interior) and a
   `UNiagaraComponent` running `/Game/Glyphbound/FX/NS_TunnelFog` (path from data, `Game.json` `FluidFog.System`).
   On BeginPlay it sets `User.WorldSpaceSize` = box size, `User.ResolutionMaxAxis` (data `FluidFog.Resolution`,
   default 192), and positions the component at the box centre (check how the template places its grid: centred or
   cornered; read the template emitter's `Grid3D_Gas_InitializeEmitter` inputs if needed, or make it a parameter).
   **Activation by distance:** the simulation runs only while the player is within `FluidFog.ActiveRange` (default
   3000 cm) of the box; outside it the component is deactivated (no GPU cost), and when re-activated it warms up
   quickly (data `FluidFog.WarmupSeconds`, use the component's warmup if the system supports it, else accept a fade in).
2. Feeding the channels each tick while active:
   - **The player:** a collision sphere (`NDC_FluidCollision`) at the capsule's centre and one at the feet, radius
     from data (`FluidFog.PlayerRadius`, default 45), velocity = the character's velocity. Dashing through it must tear
     the smoke (that is just the velocity).
   - **Shots:** every gun shot (hitscan, `AGBGun::FireHitscan` in `Weapons/GBGun.cpp`) and every projectile that
     crosses a fog box. Add one static multicast event in `Combat/GBCombatLibrary` (e.g. `OnShotTraced(Start, End,
     Speed)`) broadcast from the gun's hitscan (one tiny hunk in `GBGun.cpp`), and the enemies' hitscan
     (`GBEnemy3D::FireHitscan` in `AI/GBEnemy3D.cpp`) — the fog actor subscribes. For a shot, write for 2 or 3 frames a
     row of collision spheres along the part of the segment inside the box (one every ~40 cm, radius
     `FluidFog.ShotRadius` 12) with velocity along the shot (`FluidFog.ShotPush` cm/s, default 1500), plus a small
     source of velocity at the impact point (a puff: radial velocity). Projectiles (`AGBProjectile`, spells such as
     the Fireball) inside the box: a moving collision sphere with their velocity while they fly, and at their
     explosion (if they explode inside the box) a ring of outward velocity sources (`FluidFog.BlastPush`).
   - **Enemies** walking in the fog (spiders, orcs): collision spheres too, cheaper (one per enemy, only within
     `FluidFog.EnemyRange` of the player).
   - Cap the entries per frame (`FluidFog.MaxEntries`, default 64).
3. A pure helper for "the points along a segment clipped to a box, every N cm" with a test
   `Glyphbound.FluidFog.Clip` (`Tests/GBFluidFogTests.cpp`): a segment that misses, one that crosses, one that starts
   inside; wrong clipping must fail it.
4. Smoke: a step `gb.Smoke Fog` (`Core/GBDevCommands.cpp`, a small new block): in the test arena spawn an
   `AGBFluidFog` around the player, check the Niagara component is active and has the system, walk the player through
   it, fire a few shots through it, log the entries written per frame (player, shots), take two screenshots (before and
   after the shots), check nothing errors. Also check it deactivates when the player is far.
5. The map: in `Tools/Jam/Build-FutureBase.py` `stage_deep` (read it: constants `WORN_X`, `DARK`, `DEEP_Z`, the dark
   corridor is 25 m x 4 m x 3.5 m, `DARK = (12254, 14762, -200, 200)`, floor `DEEP_Z = -600`), place one `AGBFluidFog`
   filling the dark corridor and one over the last 15 m of the worn corridor before the small room (its interior is
   about 3.2 m wide and 3 m high: read the collision boxes there). Use the builder's existing way of spawning a C++
   actor class (search how it places `GBDoor`, `GBLadder` or `GBBreakable`) and set the box extent. Do not run the
   builder (the Editor is the architect's); say in the report what you added.

## Files you own
New: `World/GBFluidFog.*`, `Tests/GBFluidFogTests.cpp`. Small hunks (Edit tool only, never rewrite whole files; other
agents edit some of these files at the same time): `Combat/GBCombatLibrary.h/.cpp` (the event), `Weapons/GBGun.cpp`
(one broadcast), `AI/GBEnemy3D.cpp` (one broadcast), `Core/GBDevCommands.cpp` (your block), `Content/Data/Game.json`
(the `FluidFog.*` keys), `Tools/Jam/Build-FutureBase.py` (`stage_deep` only).

## Build
You may build: take the machine lock (`Glyphbound/Saved/machine.lock`: one line, who, what, UTC time; if another agent
holds it and it is younger than 45 min, do other work and check again later; delete your line when done; an open
`UnrealEditor.exe` also blocks the build: never close it yourself). Iterate cheaply (owner's rule): Editor target only
(`powershell -File Tools/Jam/Build-Jam.ps1`), `Test-Jam.ps1 -Filter '^Glyphbound.FluidFog.'`,
`Smoke-Jam.ps1 -Only Fog -Offscreen`. No full smoke, no game build, no package, no commit. If the system asset does not
exist yet when you test, the actor must log a warning and do nothing (no crash). Final report as in
`implementer_rules.md`, plus the data channel API you used (with the header you read it in) and the smoke lines.
