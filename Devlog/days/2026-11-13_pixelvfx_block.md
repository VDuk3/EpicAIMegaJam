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

<!-- RESULTS: filled in below as the night goes on -->
