# Submission sheet (Epic AI MegaJam 2026)

Prepared on 2026-11-15 from the dev log, the GDD (v0.68), the git log and the project files. Every line below cites where it comes from. Nothing here was checked by running the game. Items marked **check** need a person to confirm them before the form is sent.

Deadline: **2026-11-15 20:59 EST = 2026-11-16 01:59 UTC** (`Devlog/README.md`, rule 10).

---

## 1. Tools you used

Text to paste into the form's "Tools you used" field (one line):

> Unreal Engine 5.9.0 (C++, Niagara, Niagara Fluids, Lumen, Unreal Editor MCP server and toolsets, Fab plugin), Claude Code (Claude Opus 5.5, Claude Fable 5.2, Claude Sonnet 5.5 subagents), OpenCode (Muse Spark 1.3 Contributor), Codex CLI and Codex desktop/IDE (GPT Sol 6.1, GPT-6-Astra, GPT-6.1 Terra, GPT-6 Luna), Cursor (Claude Opus 5.5, Claude Sonnet 5.5, Grok 4.6 / 4.7, Composer 2.5, Gemini 3.7 / 3.8 Flash), Tripo (Studio canvas, CLI and API; models H3.1 and P2; image generation), Meshy (meshy-t2.5, meshy-7), ChatGPT (image generation), Nano Banana (reference images), fal (ElevenLabs Music, Lyria 2, MiniMax Music 2 trials), Blender 5.2 / 5.3, Pixelorama, ffmpeg, Node.js, Python, PowerShell, git, Fab.

### Engine and plugins
| Tool | What it was used for | Evidence |
|---|---|---|
| Unreal Engine 5.9.0 (CL 60351404) | The game: C++ only, one module, no Blueprint graphs | `CLAUDE.md`; `Design/GDD.md` §0.1; `Glyphbound/Glyphbound.uproject` |
| Unreal Editor MCP server (`ModelContextProtocol`) + 18 toolsets (EditorToolset, BlueprintToolsets, FileSystem, Console, ConfigSettings, AutomationTest, LiveCoding, Input, NavMesh, Niagara, AIModule, Plugin, Perf, SceneCapture, Physics, Cheats, GameplayTags, MetaSound), Editor only | Agents driving the Editor: imports, material fixes, plugin switches, Fab installs, renders, validation | `Glyphbound/Glyphbound.uproject`; `Devlog/days/2026-11-13_fab_assets_block.md` ("PluginToolset.SetPluginEnabled", "MaterialToolset", Fab toolset) |
| Fab plugin (enabled) | Installing the owner's Fab packs into the project | `Glyphbound.uproject`; `Devlog/days/2026-11-13_fab_assets_block.md` |
| Niagara Fluids (beta, enabled) | Fire, smoke and explosion simulations baked to pixel-art flipbooks; the castle torch flame | `Glyphbound.uproject`; `Devlog/days/2026-11-13_pixelvfx_block.md`; commit `f4dc179` |
| Niagara, Lumen, hardware ray tracing (one demo run of the bake only) | VFX, lighting | `Devlog/days/2026-11-13_pixelvfx_block.md`, "Tools used in this block" |
| GBPixelVfx (our own plugin, written during the jam) | Bakes a fluid simulation into a pixel-art flipbook with a point light | `Glyphbound.uproject`; `Glyphbound/Plugins/GBPixelVfx`; GDD §3 |
| Live Coding | `.cpp` iteration | `CLAUDE.md`; GDD §0.1 |
| Engine content: Roboto and Droid Sans Mono fonts (Apache 2.0), template mannequin animations (`Templates/TemplateResources`), basic shapes and materials | HUD fonts; enemy rifle, unarmed, hit and death clips (retargeted) | `Devlog/days/2026-11-13_hud_block.md`; `Devlog/days/2026-11-14_enemies_block.md`; `Devlog/days/2026-11-12.md` ("Engine content used") |

### AI coding agents
| Tool and models | What it was used for | Evidence |
|---|---|---|
| **Claude Code** with **Claude Opus 5.5** | Architect chats on CENTRAL (design, code, Editor work, Blender scripts, art review) | `Devlog/days/2026-11-12.md` "Tools used so far"; `Devlog/days/2026-11-14_enemies_block.md`; `Devlog/raw/CENTRAL/*/claude-*.jsonl` |
| Claude Code with **Claude Fable 5.2** | Architect chats (rifle rebuild, pixel VFX, future base, castle plan, enemy tactics) and the world-plan subagent | `Devlog/days/2026-11-13_fab_assets_block.md`; `2026-11-13_pixelvfx_block.md`; `2026-11-14_castle_plan_block.md`; `2026-11-14_enemy_tactics_block.md` |
| Claude Code subagents with **Claude Sonnet 5.5** | Implementer after Muse stalls and when the owner set it (site code, enemies, enemy AI, orc armor, vault) | `Devlog/days/2026-11-13_fab_assets_block.md`; `2026-11-14_enemies_block.md`; `2026-11-14_enemy_tactics_block.md`; `2026-11-12.md` (eleventh and twelfth blocks) |
| **OpenCode** with **Muse Spark 1.3 Contributor** (High) | Default implementer of game code | `Devlog/raw/CENTRAL/2026-11-12..14/agents.jsonl` (model `opencode-go/muse-spark-1.3-contributor#high`); `2026-11-12.md` |
| **Codex CLI** with **GPT Sol 6.1** (medium, read-only) | Code review of every implemented block | `Devlog/raw/CENTRAL/2026-11-12..15/agents.jsonl` (model `gpt-6.1-sol`); `2026-11-12.md` (seventh block review) |
| Codex CLI with **GPT-6-Astra** | Second opinions before compiling (balance, pixel VFX) | `Devlog/raw/CENTRAL/2026-11-12/agents.jsonl`, `2026-11-13/agents.jsonl` (model `gpt-6-astra`); GDD §19 "Second opinion (GPT-6-Astra)" |
| Codex desktop app and IDE extension with **GPT-6.1 Terra**, **GPT-6 Luna**, **GPT Sol 6.1** (ASSETS machine) | 3D asset work: Tripo / Meshy / Rodin MCP setup, Blender clean-up of the guns and enemies, provenance notes | `Devlog/raw/ASSETS/2026-11-12/`, `2026-11-13/codex-rollout-*.jsonl`; `2026-11-12.md` ninth block |
| **Cursor** (ASSETS machine) with Grok 4.7, Grok 4.6, Gemini 3.7 Flash | 3D asset work in Blender, music trials | `Devlog/raw/ASSETS/2026-11-1x/cursor-*.jsonl`; `Devlog/days/2026-11-13_DRAFT.md` |
| **Cursor** (LAPTOP, 2D art) with Claude Opus 5.5, Claude Sonnet 5.5, Grok 4.6 / 4.7, Composer 2.5, Gemini 3.8 Flash | Visual style guide, sprite measurement, hand plates | `Devlog/raw/LAPTOP/2026-11-1x/cursor-*.jsonl`; `2026-11-12.md` (twenty-fourth block, `Design/VisualStyleGuide.md`) |

### AI asset, image, 3D and audio generators
| Tool | What it was used for | In the shipped game? | Evidence |
|---|---|---|---|
| **Tripo** (Studio canvas, CLI 0.11.2, API; models H3.1 and P2; image generation) | Pistol, rifle, shotgun, the Entity, robot, orc and armor, Valkyrie, spider, sentinel, drone, enemy rifle, Valkyrie's sword, orc shield and orc pistol; the shotgun's reference image | Yes | `2026-11-12.md` "Assets created"; `2026-11-14_enemies_block.md`; `Glyphbound/SourceArt/*/README.md` and `provenance/` |
| **Meshy** (meshy-t2.5 geometry, meshy-7 textures) | Pistol and shotgun candidates (15 credits each) | Not found: the pistol keeps the Tripo base, and the shipped shotgun is a Tripo model (`SourceArt/Weapons/Shotgun/README.md`) | `2026-11-12.md` ninth block and "Assets created" |
| **ChatGPT** (image generation) | The pistol's reference image | Reference only | `2026-11-12.md` ninth block |
| **Nano Banana** | Reference images for the enemy models | Reference only | `2026-11-14_enemies_block.md` ("made with Tripo from Nano Banana reference images") |
| **fal**: ElevenLabs Music, Lyria 2 (failed with code 422), MiniMax Music 2 | Boss music trials on ASSETS | **Not found in the game**: the C++ source has no sound code | `Devlog/days/2026-11-13_DRAFT.md` (ASSETS, Cursor, 15:30 to 15:43 UTC) |
| **Hyper3D Rodin** | MCP installed and verified on ASSETS (`rodin_list_uploads` returned an empty list); **no generation was run**. The owner paused it ("Rodin pausado por decisión del usuario", in Spanish: Rodin paused by the user's decision) | **No** | `Devlog/days/2026-11-12_DRAFT.md` (14:47 and 14:52 UTC); `2026-11-12.md` ninth block ("Rodin was paused by the owner"); `Devlog/raw/ASSETS/2026-11-12/reports/` |

> **Partner prizes, flagged:**
> - **Hyper3D Rodin: installed, never used to make an asset.** No Rodin output is in the game. Listing it in "Tools you used" would claim more than the log shows. **Owner decision.**
> - **NVIDIA (DLSS, NvRTX, ACE): not found in the log.** No DLSS plugin is in `Glyphbound.uproject`. The engine is stock UE 5.9.0, not NvRTX. Hardware ray tracing was used once to bake a VFX demo on a development machine, and that is not an NvRTX feature in the game. The only DLSS entry is a plan line ("DLSS plugin check for the NVIDIA award", `2026-11-12.md`, plan for tomorrow), and GDD §17 leaves it open. **As things stand the game does not qualify for the NVIDIA RTX Award.**

### DCC and other software
| Tool | What it was used for | Evidence |
|---|---|---|
| Blender 5.2.2 / 5.2.3 / 5.3.0 (mostly headless scripts in `Tools/Blender/`) | Gun clean-up, baking, export; enemy rigs, splits and armor fitting; spaceship corridor; castle pixel twins | `2026-11-12.md`; `2026-11-14_enemies_block.md`; `2026-11-13_future_base_slice1_block.md` |
| Pixelorama | 9 hand-drawn pixel-art sprites (no AI) | `2026-11-12.md` twenty-fourth block; `Glyphbound/SourceArt/PixelArt/README.md` |
| ffmpeg | Preview GIFs of the baked VFX | `2026-11-13_pixelvfx_block.md` |
| Node.js, Python, PowerShell | Build, import, map and tile scripts (`Tools/Jam/`, `Tools/Blender/`, `Tools/Devlog/`) | `2026-11-13_pixelvfx_block.md`; `2026-11-14_castle_plan_block.md` |
| Microsoft Edge (headless) | Rendering the SVG floor plans to PNG | `2026-11-13_fab_assets_block.md` ("made with headless Edge from the SVG files") |
| git (remote on GitHub) | Version control and the dev log | `Devlog/README.md` rule 2; `CLAUDE.md` |
| butler | Installed on CENTRAL for uploads over 1 GB; no upload logged yet | GDD §0.1 "Size" |
| AssetSearch (`codepawl/asset-index` + EmbeddingGemma 2) | Prepared before the jam (declared). No use during the jam was found | `PRE_JAM_DECLARATION.md`; `PREJAM_PREP.md` "Tools evaluated" |
| Prepared but **no use found during the jam**: MetaHuman Animator, Modly (Hunyuan3D-mini, TripoSplat), Cascadeur | Tested or set up before the jam | `PRE_JAM_DECLARATION.md`; `PREJAM_PREP.md` (no mention in `days/`) |

### Marketplaces
| Marketplace | What it was used for | Evidence |
|---|---|---|
| **Fab** | Every third-party pack (section 2), installed through the Fab plugin and the Editor's Fab toolset | `2026-11-13_fab_assets_block.md`; `2026-11-13_future_base_slice1_block.md`; `2026-11-14_enemy_tactics_block.md`; `2026-11-14_future_base_fourth_walk_block.md` |

---

## 2. Third-party assets

Every Fab pack below is under the **Fab Standard License** (Professional for paid packs, Personal for free ones), and every listing has `aiTrainingForbidden` set. The packs' files were not fed to any generator (`2026-11-13_fab_assets_block.md`, "Licenses and AI").

| Asset (seller) | Source | License | Where used | Evidence |
|---|---|---|---|---|
| Hologram Shader (SERLO, free) | Fab, https://www.fab.com/listings/cfb64bbf-70e6-42db-9cab-7be76f796445 | Fab Standard, Personal | `M_Hologram` + 3 instances; the Entity's hologram tentacles (`M_EntityHologram`, a modified copy) | `2026-11-12.md` "Third-party assets"; `2026-11-13_fab_assets_block.md`; `SourceArt/Enemies/Entity/README.md` |
| Medieval Kingdom (Hivemind) | Fab, https://www.fab.com/listings/42d4a792-2b66-423d-9b20-84d6b2c578d8 | Fab Standard, Professional | The castle: pixel-art twins of the pack's meshes replace them in the map (GDD v0.68); two rubble pieces cooked for the future base map (84 MB) | `2026-11-13_fab_assets_block.md`; `2026-11-14_castle_plan_block.md`; commit `58af4a1` |
| Sci-Fi Creatures Research Lab (Tirgames Assets) | Fab, https://www.fab.com/listings/c2494db5-55d2-4e35-9c57-540d0f2cf455 | Fab Standard, Professional | The lab room and the Valkyrie's room in the future base (the listing says its screen images are AI-generated) | `2026-11-13_fab_assets_block.md`; `2026-11-14_future_base_lab_boss_block.md`; `2026-11-14_future_base_fourth_walk_block.md` |
| Sci-fi Impacts (SHOGUN GAMES) | Fab, https://www.fab.com/listings/2b206307-1e17-430c-9e43-39bb39e2d4b1 | Fab Standard, Professional | Enemy tracers and energy impacts | `2026-11-13_fab_assets_block.md`; `2026-11-14_enemies_block.md` |
| Muzzle Flash 3D (SHOGUN GAMES) | Fab, https://www.fab.com/listings/5cbcbcfa-8d17-4ed2-b0f1-9f9d96d95ba1 | Fab Standard, Professional | Enemy muzzle flashes | same |
| Realistic Gun VFX / GunFX (Hivemind) | Fab, https://www.fab.com/listings/21573c1a-e494-4194-9926-0c03f48a5563 | Fab Standard, Professional | Concrete impacts; four glass shard meshes for the breakable bulkhead | `2026-11-13_fab_assets_block.md`; `2026-11-14_enemies_block.md`; `2026-11-14_enemy_tactics_block.md` |
| Smoke Niagara Fluids (SHOGUN GAMES) | Fab, https://www.fab.com/listings/cdb1ca29-88fc-4818-9980-cbbed6abc59e | Fab Standard, Professional | In the project; **use in the game not found** (the pixel VFX bake uses Epic's own templates) | `2026-11-13_fab_assets_block.md`; `2026-11-13_pixelvfx_block.md` |
| Scifi Material Pack (JulioSillet, free) | Fab, https://www.fab.com/listings/5450f0ae-52b1-4194-890e-097d0427617a | Fab Standard, Personal | Used on the first site map, which was deleted; **use in the final build: check** | `2026-11-13_fab_assets_block.md` |
| High Tech Base (YaMaKundra, free) | Fab, https://www.fab.com/listings/9970b512-825c-4d6d-917a-1c0480de16fb | Fab Standard, Personal | The future base: control room, corridor bays, hangar shell; 85 instances re-parented to our own masters | `2026-11-13_fab_assets_block.md`; `2026-11-13_future_base_slice1_block.md` |
| Spaceship Corridor Damaged (ArmTm, free) | Fab, https://www.fab.com/listings/dcdef266-660e-477d-9f5a-03dc9579f3c6 | Fab Standard, Personal | Its meshes (not its materials, screen picture or planet backdrop) | `2026-11-13_future_base_slice1_block.md` |
| Modular SciFi Station (Martin Milz) | Fab, https://www.fab.com/listings/0667e321-31a7-40ed-b85c-6db5bbc4366b | Fab Standard, Professional | Future base: decks, posts, railings, stairs | `2026-11-13_future_base_slice1_block.md`; commit `a975d1c` (in `2026-11-14_DRAFT.md`) |
| Deep Water Station (Kyrylo Sibiriakov) | Fab, https://www.fab.com/listings/ec2385d7-3e02-494b-a243-e18cac7f4a69 | Fab Standard, Professional | Six meshes in the underwater wing (`SM_WinMetal02`, `SM_Lamp05`, two gas cylinders, `SM_Mollusk02`, `SM_RockSet05`) | `2026-11-14_future_base_fourth_walk_block.md` |
| Pistol and Rifle Locomotion Animations 1700 (Kingboars, free) | Fab, https://www.fab.com/listings/509f8c0a-3eb8-46cb-bda9-3cf1a48d6aa0 | Fab Standard License | 12 of its 1,688 clips, retargeted onto our enemies | `2026-11-14_enemy_tactics_block.md` |
| red scifi katana (M@xim, free) | Fab, https://www.fab.com/listings/ec6f2549-f8f9-4805-981b-534868d14aee | **CC-BY** (attribution required: "red scifi katana" by M@xim, Fab) | The Crimson Katana, the second sword (`SM_Katana`, first-person view model and floor pickup) | `2026-11-15_crimson_katana.md` |
| Modular Sci-Fi Treatment Station Environment (Tris Games) | Fab, https://www.fab.com/listings/d8ac4186-78f7-43c7-b21c-04b4dc0901a2 | **LICENSE MISSING: check** (offered by the owner, no install logged) | Not found in the project | `2026-11-13_future_base_slice1_block.md` |
| Concept art of a sci-fi weapon (purbek, ArtStation) | https://purbek.artstation.com/projects/EVnZY4 | **LICENSE MISSING: check** (reference only) | **Not in the game.** Visual guide for the pistol's energy bar; not traced, not a texture, not fed to a generator | `2026-11-12.md` "Third-party references" |
| Owner's reference images for the guns and enemies (ChatGPT, Tripo image, Nano Banana) | AI-generated during the jam | AI output, not a sourced asset; ASSETS' notes say the rights of the references were **not verified** | Reference only; marks copied by the generators were removed from the pistol | `2026-11-12.md` ninth block; `SourceArt/Enemies/Orc/provenance/HANDOFF_ORCOS_MODULARES.md` |

Not installed (looked at only): AdaptivePathfindingSystem (`2026-11-14_enemies_block.md`).

---

## 3. Prior work declared

`Devlog/PRE_JAM_DECLARATION.md` declares what was made before 2026-11-12 00:00 ET: the dev-log hooks and collectors, the agent wrapper, the Editor MCP and packaging scripts, the LAN share setup, AssetSearch (metadata only), the dev-log templates and `CLAUDE.md`. Its technical tests used throwaway projects that were deleted, and it states that no game content was made before the jam. The full log is in `Devlog/PREJAM_PREP.md`.

**Gap:** its section "AI tools we plan to use" still ends with "(to complete: ...)". Replace that line with a pointer to section 1 of this file before publishing.

---

## 4. itch.io page text

**Title:** `<Game title>`. The code name is **Glyphbound**; the GDD still lists the title as TBD (GDD §1).

**One-line pitch:** A fast first-person shooter-RPG where a pixel-art medieval mage jumps through portals between a magic castle in the past and a 3D cyberpunk base in the future, and wins by mixing spells and guns.

**Description:**

> A mage travels to a mysterious castle out of curiosity. At its seal he is trapped and thrown forward in time, to a military base built on the same ground. A woman's voice asks him for help.
>
> Glyphbound is one map in two times. The past is a pixel-art castle full of magic and orcs; the future is a 3D cyberpunk base full of guns, armored orcs, robots and drones. Fixed portals carry you between the eras, and everything you carry comes with you: a gun works in the past, and a spell works in the future.
>
> The game has one art rule that never breaks: anything medieval or magic is pixel art, and anything futuristic is 3D, whichever era it is in. The HUD follows the rule too. A staff in hand gives you a 1990s RPG interface of stone and gold; a gun gives you a modern cyberpunk one.
>
> Magic is performed, not triggered. Each spell is a short slowed-time sequence, a key pattern and then a mouse gesture, and how well you perform it sets the spell's power and reach. Enchant your guns with the elements, build up fire, ice, lightning and air on your enemies, and grow two skill trees, one for spells and one for weapons.
>
> Mix the eras, mix your weapons, and find out who is really asking for help.

(About 210 words. Built from GDD §1, §2, §3, §4 "Story", §7 and §13. **Check against the final build**: keep only what a player can reach in it.)

**How it shows the theme, DIRECTOR'S CUT:**
- **In the game's design** (GDD §14): an author's signature on every system. A hard art split that follows each object and never breaks (a pixel-art mage who wields 3D guns), a UI that changes style with what you hold, magic you perform instead of trigger, and combat that rewards mixing the two eras.
- **In how we made it:** a human director set the vision, and AI agents built it in roles: an architect chat (Claude Code) designing and deciding, an implementer (Muse Spark through OpenCode, or Claude Sonnet subagents when Muse stalled), and a separate reviewer (GPT Sol 6.1 through Codex) on every block. The director's calls are logged with their reasons, and so are the cuts: the first site maps, deleted whole because the kits were placed with no sense of place; the castle's sculpted pack replaced by plain pixel-art twins; the pixel hands on the guns, dropped after five attempts; Rodin paused. Each cut is in the dev log (`days/2026-11-13_site_maps_deleted_block.md`, GDD v0.67, v0.68 and v0.47).

**Controls** (GDD §9):

| Action | Keyboard & mouse | Gamepad |
|---|---|---|
| Move | W A S D | Left stick |
| Look | Mouse | Right stick |
| Primary attack (staff: magic attack; gun: shoot) | Left click | RT / R2 |
| Aim (gun in hand) | Right click | LT / L2 |
| Spell slot 1 (staff in hand) | Right click | LB / L1 |
| Spell slots 2 to 5 (staff in hand) | 1, 2, 3, 4 | Hold LT / L2 + A, B, X, Y |
| Reload (gun) | R | Standard shooter layout |
| Weapon enchant (gun in hand) | T | LB / L1 |
| Switch staff / gun / sword | Mouse wheel or Alt | D-pad up |
| Jump (climbs a reachable ledge) | Space | A / Cross |
| Dash | Ctrl | B / Circle |
| Quick-use item | F | RB / R1 |
| Cycle quick-use item | Shift | D-pad left / right |
| Interact | G | Y / Triangle |
| Block (ice shield, staff or sword in hand) | Q (hold) | LT / L2 (hold) |
| Spell sequence, part 1 | W A S D | Face buttons |
| Spell sequence, part 2 | Mouse gesture | Right stick |
| Inventory | Tab | View |
| Skill trees | K | From the inventory |
| Pause | Esc | Menu |

Menus also work with the mouse. GDD §9 marks the Block keys as provisional: **check in the final build.**

**System requirements:**
- Windows 10 or 11, 64-bit. Unzip and run the `.exe`. No extra software, account or server needed.
- Developed and tested on a laptop with an RTX 4080 Laptop GPU (GDD v0.10). Minimum specs were not measured.

**Credits by role** (placeholders, to fill in on the form):
| Role | Who |
|---|---|
| Director, game design, owner | `<Team member 1>` (Team Owner) |
| 2D pixel art (sprites, hand-drawn) | `<Team member 2>` |
| 3D assets (AI generation and clean-up), Valkyrie concept drawing | `<Team member 3>` |
| Other members | `<Team member 4>`, `<Team member 5>` |
| AI agents | Claude Code (architect, implementation), OpenCode / Muse Spark (implementation), Codex / GPT Sol 6.1 (code review), Cursor and Codex (asset work) |
| Third-party assets | See section 2 (Fab sellers) |

Teen-safe check for the page: no third-party brands are named, only Unreal Engine. The page should not name the games used as references (GDD §3, §13; `2026-11-13_future_base_slice1_block.md`).

---

## 5. Submission checklist status

| Item (`CLAUDE.md`, "Entrega") | Status | Evidence |
|---|---|---|
| Win64 zip packaged | **done** (last logged package) | commit `58af4a1`; `Tools/Jam/Package-Jam.ps1` |
| Zip **under 1 GB** | **BLOCKER: not met.** Last zip **2,024.9 MB**, about twice the limit. The castle pack is down to 84 MB cooked, so most of the size comes from elsewhere. The GDD calls 1 GB "a target, not a hard limit" (butler uploads), while `CLAUDE.md` and `PREJAM_PREP.md` list it as a requirement. **Owner decision, today** | commit `58af4a1`; `2026-11-14_enemy_tactics_block.md` ("The package is 2.0 GB against the jam's 1 GB"); `2026-11-14_future_base_lab_boss_block.md` (1,330.8 MB, "the owner set the limit aside for now"); GDD §0.1 |
| Named `<Team>_<Game>.zip` | **pending**: today's name is `TeamTBD_Glyphbound.zip`, and both the team name and the game title are TBD | `2026-11-12.md`; GDD §1 |
| No `.pdb` in the zip | **done** (the packaging script excludes them) | `Tools/Jam/Package-Jam.ps1` (line 9, line 56) |
| A package of the final commit | **pending**: commit `f4dc179` (castle torches) comes after the last package, and the working tree has uncommitted changes | `git log`; `git status` |
| Uploaded to itch.io and submitted to the jam | **unknown**: no upload is logged | none found |
| Gameplay video, 30 to 60 s, 1920x1080 or more, English, gameplay in the first 5 s | **pending**: no video is logged | none found |
| Dev log link in English, submitted with the game before the deadline | **pending**: final entries for Nov 13, 14 and 15 are not written (see section 6) | `Devlog/days/` |
| Team name, game name, full name and Epic email of each member, Team Owner | **pending** (kept out of the repo on purpose) | none |
| Third-party assets listed | **done in draft**: section 2; two lines say LICENSE MISSING | this file |
| Prior work declared | **done**, one line to complete | `PRE_JAM_DECLARATION.md` |
| "Tools you used" field | **done in draft**: section 1 | this file |
| NVIDIA RTX Award: NvRTX, DLSS or ACE in the game | **not met**: nothing found in the log or the plugin list | section 1 |
| Everything in English, Teen (13+), no third-party logos | **unknown**: no audit logged. Two things to check: the orc shield's source texture names carry a working title with a third-party character's name (renamed on arrival, and the model has no mark: `SourceArt/Enemies/OrcShield/README.md`), and the generated weapon marks were removed (`2026-11-12.md`) | listed files |
| No extra software needed to play | **done by design** (no local LLM or server) | GDD §0.1; `CLAUDE.md` |

---

## 6. Dev log publishing

`Devlog/README.md`, rule 9 (updated 2026-11-15): publish the final entries `days/<date>.md`, the block notes `days/*_block.md`, `media/`, this file, `PRE_JAM_DECLARATION.md` and `PREJAM_PREP.md`. Never publish `raw/` or the automatic `*_DRAFT.md` files (they quote raw prompts, partly in Spanish, and LAN details; they stay in the repo as evidence for Epic). The link goes in the submission before the deadline.

**Day entries that exist (`Devlog/days/`):**
- Final entries: `2026-11-12.md`, `2026-11-13.md`, `2026-11-14.md` (13 and 14 written 2026-11-15 from the drafts and blocks)
- Automatic drafts: `2026-11-12_DRAFT.md`, `2026-11-13_DRAFT.md`, `2026-11-14_DRAFT.md`
- Blocks for Nov 13 (merged into `2026-11-13.md`, kept for detail): `2026-11-13_enchant_choice_block.md`, `_fab_assets_block.md`, `_future_base_slice1_block.md`, `_general_perks_balance_block.md`, `_hand_plates_block.md`, `_hud_block.md`, `_ledge_climb_block.md`, `_menu_transition_confirm_block.md`, `_new_gun_perks_block.md`, `_perks_per_gun_block.md`, `_pixelvfx_block.md`, `_shotgun_poses_block.md`, `_site_maps_deleted_block.md`, `_skilltabs_era_menus_block.md`, `_weapon_hub_block.md`
- Blocks for Nov 14 (merged into `2026-11-14.md`, kept for detail): `2026-11-14_castle_plan_block.md`, `_chief_arms_block.md`, `_enemies_block.md`, `_enemy_tactics_block.md`, `_future_base_encounters_block.md`, `_future_base_fifth_walk_block.md`, `_future_base_fourth_walk_block.md`, `_future_base_lab_boss_block.md`, `_future_base_sixth_walk_block.md`, `_orc_look_block.md`
- Nov 15: no draft and no entry yet (run `Tools/Devlog/New-DevlogDay.ps1 -Date 2026-11-15`)
- Media: `media/2026-11-12/` (108 files), `media/2026-11-13/` (116), `media/2026-11-14/` (163)

**To do before publishing:**
- Write `2026-11-15.md` at the end of the day (`Tools/Devlog/New-DevlogDay.ps1 -Date 2026-11-15`, then the entry).
- The drafts are no longer published. `New-DevlogDay.ps1` now also redacts Windows user folders, invite codes, LAN IPs and host names.
- `PREJAM_PREP.md` names two machines after a team member's first name (lines 9 and 29): owner to decide whether to keep it (team members' names go in the submission anyway).
