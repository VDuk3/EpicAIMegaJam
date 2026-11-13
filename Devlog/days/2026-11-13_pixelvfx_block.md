# Block: GBPixelVfx, a plugin that turns Niagara Fluids into pixel art (night of Nov 12 to 13, CENTRAL)

> Written by the architect chat of this block (Claude Code, Claude Fable 5.2). To be merged into the day entry by whoever closes the day. Times in UTC.

## The owner's idea, and the first answer
The owner's prompt (Spanish, 05:5x UTC): *"para esta Game Jam hagamos un plugin de conversión de Niágara y Niágara Fluids a pixel art [...] no debe quedar en simplemente una pixelización del efecto original, sino que quiero que los parámetros modernos de, por ejemplo, un Niagara Fluid se traspasen a una versión pixelada que se comporte exactamente igual pero que se vea pixel art y sea ligero"*.
Translation: "for this jam let's make a plugin that converts Niagara and Niagara Fluids to pixel art [...] it must not be a plain pixelation of the original effect; I want the modern parameters of, say, a Niagara Fluid carried over to a pixel version that behaves exactly the same but looks like pixel art and is light".

The architect's answer was a no to the general converter and a yes to a small one:
- "Behaves exactly the same and is light" contradicts itself: what costs in a Niagara Fluid is the simulation, not the drawing. Either the effect is baked (same look in motion, almost free, no longer reacts) or it is simulated on a smaller grid (still reacts, only somewhat lighter, not identical).
- There is no general parameter translation: every Niagara system is a different graph of modules.
- The strong half of the idea is the other one: fire, smoke and explosions are hard to draw by hand in pixel art, and the game's art rule says all magic is pixel art.

The owner's reply (06:1x UTC): *"hagamos la versión mínima que sugieres. Eres el arquitecto. Consulta el plan con Astra 6 medium por el CLI de Codex, haz revisiones de código cuando sea pertinente con Sol 6.1 medium, y usa a Muse Spark de implementador a través de OpenCode. [...] Me voy a dormir, no te detengas a pedirme permisos ni a hacerme preguntas. Este plugin es TUYO [...] lo que más me interesa es los niagara fluids y comportamientos de Ray tracing."*
Translation: "let's do the minimal version you suggest. You are the architect. Check the plan with Astra 6 medium through the Codex CLI, review code with Sol 6.1 medium when it makes sense, and use Muse Spark as the implementer through OpenCode. [...] I am going to sleep, do not stop to ask me for permission or questions. This plugin is YOURS [...] what interests me most is Niagara Fluids and ray tracing behaviors."

"Ray tracing behaviors" was not explained and the owner was asleep. The architect read it as: the pixel version must keep behaving like a light source in a ray-traced / Lumen scene (it lights the scene, its light casts shadows, it shows in reflections), and the bake must keep the volume's own shading. It is recorded here as an interpretation, to be confirmed by the owner.

## Decisions (architect) and why
| Decision | Why |
|---|---|
| The bake runs in an uncooked `-game` process without a window, with its own game mode (`?game=/Script/GBPixelVfxBaker.GBPixelVfxBakeGameMode`) | Niagara Fluids are GPU grid simulations: in a game process they simulate and render exactly as in the game, with no Editor preview scene to fake. Same pattern as our smoke test. |
| Our own capture (scene capture + CPU conversion) instead of the engine's Niagara Baker | The Baker's renderer lives in private headers of `NiagaraEditor` (`NiagaraBakerRenderer.h`); a plugin cannot link it. And the Baker stops at a smooth flipbook: the pixel-art conversion is the part that matters. |
| Output = `Content/Data/Vfx/<Id>.png` + `<Id>.json`, loaded at runtime into a transient nearest-filtered texture | The project's rule is "text in git, data as loose files": `Content/Data` is the only part of `Content/` in git and it is staged loose in the packaged game. A clip can be re-baked or hand-retouched without a cook. |
| Two modules: `GBPixelVfx` (Runtime, no Niagara dependency) and `GBPixelVfxBaker` (UncookedOnly) | The packaged game carries a quad, a texture and a light. Niagara Fluids is only enabled on the bake command line, never in the `.uproject`. |
| The plugin is off by default while it is written (`"EnabledByDefault": false`, UBT `-EnablePlugin=`, runtime `-EnablePlugins=`) | Another architect chat was building and packaging the shotgun on the same machine the same night: a half-written plugin must not break its build or enter its zip. |
| Colors are mapped to ramps of the master palette (`Design/VisualStyleGuide.md`), hard alpha, stepped 12 fps, hysteresis against flicker | That is what separates pixel art from a pixelated video: ramp shading, binary alpha, few colors, held frames. |
| A baked **light track** per clip drives a real point light at runtime | The baked sprite cannot emit light by itself in a way Lumen reliably picks up from a small moving quad; a real light gives shadows (Virtual Shadow Maps, or ray-traced with hardware RT), GI and reflections. Honest name: a stylized approximation of the volume's light, not its light transport. |
| Hardware ray tracing is NOT enabled in the project; the demo can run with it through a command-line ini override | Enabling it project-wide two days before the deadline is a risk for every other system (shader permutations, memory, players without RT hardware). |
| A machine lock file (`Glyphbound/Saved/machine.lock`) shared with the other chat | Project rule: one build or run at a time per machine. Both chats were working unattended; a file is checked by scripts, a chat message is not. |

## Who did what
1. **Architect (Claude Code, Fable 5.2):** read the engine (Niagara Baker headers, the Niagara Fluids template list, how our sprites and materials are built), wrote the plan, the four bake jobs, `Tools/Jam/PixelVfx-Jam.ps1` and the implementer brief. Coordinated with the shotgun chat by session message and the lock file.
2. **Astra (GPT-6-Astra, medium, Codex CLI, read-only):** plan review, prompt `Prompts/pixelvfx_plan_astra.md`, run `062728-codex-7ba49d`. Eight notes; the ones that changed the plan:
   - scene captures do not advance GPU simulations (`FSceneView::AllowGPUParticleUpdate`), so the main offscreen view must render and look at the effect;
   - orthographic capture of a ray-marched volume is unverified: use a long-lens perspective camera;
   - the fixed time step must come from the command line (`-UseFixedTimeStep -FPS=60`), BeginPlay is one frame late;
   - the scene capture's alpha is inverse opacity for translucent AND additive passes: keep one pass, choose the alpha rule per job;
   - "computed by Lumen" was wrong for direct shadows (the project uses Virtual Shadow Maps); call the light track a stylized approximation;
   - island removal and hole filling off by default (they erase sparks), hysteresis reset on fast-changing frames;
   - the exact override for a hardware-RT run: `-dx12 -ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=1,r.Lumen.HardwareRayTracing=1`.
   The architect's decision per note is in `Prompts/pixelvfx_astra_notes.md`.
3. **Muse Spark 1.3 Contributor (High, OpenCode):** implementation, brief `Prompts/pixelvfx_implement.md`.
4. **Sol 6.1 (medium, Codex CLI):** code review.

## What happened, in order
1. **Plan and brief (06:15 to 06:35).** Astra's eight notes changed the capture (perspective lens, the main view must render, fixed step from the command line) and the wording about lighting. Brief written, Muse Spark started (run `063311-opencode-e17e84`).
2. **First implementation, Muse Spark (06:33 to 08:20).** Both modules, the job loader, the offscreen bake, the CPU conversion, nine tests, the material script. The bake worked end to end for the Niagara Fluids templates: four clips in about 50 s. Then it spent its last half hour on the showcase stage (black screenshots) and one run died with `DXGI_ERROR_DEVICE_HUNG`. **The architect stopped the run** and moved the rest into a second brief.
3. **The architect looks at every sheet** (project rule: review the art yourself before showing it). Fixed through the job files, not code: the fire was mostly dark red (gamma and levels percentile), the "smoke" template is really a smoke column with fire at its base (split mapping onto two ramps, hot and cold), Epic's ColoredFire template rendered nothing in the capture (the arcane clip is now the fire simulation mapped to the arcane ramp), dark red blobs in the cooled explosion smoke (hot / cold thresholds).
4. **First review, Sol 6.1** (run `081821-codex-cc8c5a`, prompt `Prompts/pixelvfx_review_sol.md`): 18 findings, all accepted. The important ones: the pivot offset was applied in the wrong space (sprites shifted sideways instead of up), material and light state were only set when the quad was first created, a failed job skipped the next one, and six tests that wrong code would pass.
5. **Second run, Muse Spark** (08:24 to 11:10, run `082453-opencode-7eb1c6`, brief `Prompts/pixelvfx_fixes.md`). It fixed the pivot, the 18 findings, added the color cluster clean-up, the one-shot fade-out and six more tests, and then **stalled again on the showcase: 53 showcase runs in 2 h 45 min.** Its conclusion was that the engine's Plane mesh "does not rasterize in `-game`", so it replaced the quad with a cube one metre thick, and that "translating the stage camera breaks quad rendering", so it stopped moving the camera. The owner, back at 11:05, asked how it was going and offered Sonnet; the architect stopped the run.
6. **The real cause, found by the architect in six short runs** (11:10 to 11:22). Method: change one thing, log facts, look.
   - A 2 mm slab instead of the 1 m cube: the sprites vanished. So what the earlier screenshots showed was a **side face** of the cube, not the sprite's front.
   - A log line from the component: its rotation was right (the quad's normal pointed at the camera).
   - `GetLastRenderTimeOnScreen()` per quad: the fire was drawn for **one frame after each camera cut**, then never again. That is occlusion culling: something opaque was in front of it.
   - A dump of every primitive in the world with its bounds: **the floor, the wall and all the obstacles of the stage sat at the world origin.** They were spawned with `SpawnActor<AActor>(..., FTransform(Pos))`; a plain `AActor` has no root component when it spawns, so the spawn transform is dropped. The back wall stood exactly on the line of clips and hid them.
   - Fix: place each stage actor through its root component. The quad went back to the engine Plane, the stage got wider, close-ups frame each clip's own frame, and a last shot is taken from a moved camera.
   Lesson for the log: the implementer had a wrong theory after ten runs and spent forty more defending it. Three facts from the engine (render time, bounds, a list of primitives) settled it.
7. **Second review, Sol 6.1** (run `112310-codex-7cf33d`, prompt `Prompts/pixelvfx_review_sol_2.md`): 12 findings, 2 major (the quad inherits its actor's scale but the pivot math does not; the fade-out's last frame is not empty, a visible pop) and 10 minor, 7 of them tests that wrong code would pass. Packaging was checked against the engine source: the always-cook directory and the loose `Data` staging are enough, `pvfx.Spawn` is compiled out of Shipping. All accepted and given to **Claude Sonnet 5.5 (a Claude Code subagent)**: exception to the default implementer, allowed by the owner after Muse's second stall.

## Results
- **15 tests** of the conversion (pure logic), 15 s.
- **Bake:** four Niagara Fluids clips in 51 s on CENTRAL (RTX 4080 Laptop). Each clip is a PNG of a few kilobytes plus a JSON file; the four PNGs together are 56 KB.
- **Compare** (`-Action Compare -Id FluidFire`, 180 frames each, 1280 x 720, offscreen): 8 source simulations **50.1 ms per frame** (GPU 49.3), 8 baked clips with their lights **4.5 ms** (GPU 4.1), the empty stage 4.1 ms. The baked clip costs about a hundredth of the simulation.
- **Lighting:** each clip's point light follows the baked track, lights the wall and the obstacles, throws their shadows and shows in the floor's Lumen reflections. The same stage runs with **hardware ray tracing** (`-HardwareRT`: `r.RayTracing=1`, `r.Lumen.HardwareRayTracing=1` and, as the engine demands, `r.SkinCache.CompileShaders=1`, all three as a command-line ini override; the project's settings are untouched).
- Two things the script got wrong on the way: `$PSScriptRoot` is empty in a `param` default when the script has `[Parameter()]` attributes (resolved in the body now), and an `-ini:` override needs the `[Section]` prefix on **every** key, or only the first key is applied.
- One GPU "device removed" crash in a showcase run started right after a bake; the same run passed when repeated. Not reproduced.

## Media (`Devlog/media/2026-11-13/`)
- `pixelvfx_FluidFire.gif`, `pixelvfx_FluidSmoke.gif`, `pixelvfx_FluidExplosion.gif`, `pixelvfx_FluidArcaneFire.gif`: the four baked clips at their own frame rate, enlarged 6 times.
- `pixelvfx_<Id>_source.png` and `pixelvfx_<Id>_sheet.png`: what the capture saw (the realistic simulation) and the pixel-art frames made from it.
- `pixelvfx_showcase_wide.png`: the four clips on the test stage, each lighting the wall with its own color; obstacle shadows on the wall; reflections on the floor.
- `pixelvfx_showcase_FluidFire.png` and the other three close-ups; `pixelvfx_showcase_moved_camera.png`: the same stage from another place (the sprites turn to face the camera).
- `pixelvfx_compare_FluidFire.png`: the live simulation (left, inside its red debug bounds) and the baked clip (right) side by side.

## Third-party material
None enters the game. The bake reads Epic's **Niagara Fluids** template systems (`/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_Fire`, `_Smoke`, `_Explosion`), which ship with Unreal Engine 5.9; the game contains only the flipbooks baked from them.

## Tools used in this block
Claude Code (Claude Fable 5.2 as architect, Claude Sonnet 5.5 as a subagent), OpenCode with Muse Spark 1.3 Contributor, Codex CLI with GPT-6-Astra and Sol 6.1, Unreal Engine 5.9 (Niagara, Niagara Fluids, Lumen, hardware ray tracing for one demo run), ffmpeg (preview GIFs), Node.js and PowerShell (scripts).

## Open, for the owner
- Is the reading of "ray tracing behaviors" the right one?
- No spell uses a baked clip yet. Which spells should, and at what size and light strength?
- The plugin is still off by default in the game's `.uproject`.
