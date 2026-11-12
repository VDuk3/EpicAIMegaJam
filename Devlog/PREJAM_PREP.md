# Pre-jam preparation log

A record of everything prepared, tested and decided before the Epic AI MegaJam 2026 started. It backs `PRE_JAM_DECLARATION.md` and the preparation section of the dev log (Glass Box). It is updated as we go; newer entries go at the end of each section.

- **Jam:** Nov 12 00:00 ET to **Nov 15 20:59 ET** (Nov 16 01:59 UTC). The theme is announced on Nov 11.
- **Submission:** packaged Win64 build, zip under 1 GB named `TeamName_GameName`, a 30–60 s gameplay video and the pre-jam declaration.
- **Target prize:** Glass Box (best dev log).
- **Machines:**
  - **PC Romelt** (log id `JAM_MACHINE=ASSETS`): **heavy 3D and animation** (owner, 2026-11-10). Jam root at `D:\MegaJam`.
    - **One heavy task at a time** on this PC: Editor, Modly, Cascadeur and mocap solves run one after another, never together.
    - Example flow: MetaHuman mocap is recorded, solved and cleaned up here (Cascadeur or Control Rig), exported to the game skeleton, then sent to CENTRAL with Migrate through `\\CENTRAL\Incoming\ASSETS\`.
    - Hardware (checked 2026-11-10): AMD Ryzen 5 9600X (6 cores), AMD Radeon RX 7700 XT (12 GB), 16 GB RAM (15.1 GB usable).
    - **Local 3D generation: Modly.**
      - App `D:\Modly`, API on `127.0.0.1:8765` (start it with `D:\Modly\start_api.bat`), MCP `modly` (user scope).
      - Models: Hunyuan3D-mini (default, about 4.5 GB of VRAM) and TripoSplat (about 10 GB of VRAM). TripoSplat cannot run with the Unreal Editor open, and its end-to-end generation was never verified on this PC.
      - Output: `.glb` files in `D:\Modly\workspace\<collection>\`. Run `POST /model/unload-all` after generating.
      - Generated meshes are not guaranteed to be closed: check them in Blender before importing.
    - **Blender 5.2.2 LTS** (Steam, `D:\SteamLibrary\steamapps\common\Blender\blender.exe`, Python 3.13).
      - Preferred route: headless scripts with `--background --factory-startup --python-exit-code 1`. Without the last flag, a crashing script exits 0.
      - Export settings verified for Unreal: `apply_unit_scale=True, global_scale=1.0, axis_forward='-Y', axis_up='Z'` (1 m → 100 cm). Start from `read_factory_settings(use_empty=True)`, or the default cube gets exported too.
      - MCP `blender` (community `uvx blender-mcp`, user scope, version not pinned) is configured. It runs arbitrary Python over an unauthenticated local TCP port, so use it only for concrete Blender tasks.
    - **Cascadeur 2026.2.3** (basic license, `C:\Program Files\Cascadeur\cascadeur.exe`).
      - MCP `cascadeur` (`cascadeur-script-runner`, one tool `run_script`) runs **inside the Cascadeur app** at `http://127.0.0.1:8765/mcp`. The owner starts it from Cascadeur, **before** opening the agent chat.
      - **Port clash:** Modly and the Cascadeur MCP both use port 8765, so only one can run at a time.
      - With the Unreal Editor open, Cascadeur ran out of RAM once (it dropped its undo history).
    - **Also:** EmbeddingGemma 2 on `127.0.0.1:8766` (for AssetSearch, about 1.5 GB of VRAM; stop it when done), and UE 5.9.0 with the MetaHuman Markerless Motion Capture plugin.
    - **Main limit: 16 GB of RAM.** In the mocap test, with the Editor open and a body solve running, under 3 GB were free. Do not run the Editor, Modly and Cascadeur at the same time.
    - Source: Panthelia's `State_DeveloperTools.md` §65 (Modly, EmbeddingGemma), §67 (Blender) and §99 (Cascadeur), checked on 2026-11-10.
  - **Laptop Romelt** (log id `JAM_MACHINE=LAPTOP`): **2D art, sound and light 3D** (owner, 2026-11-10). AMD Ryzen 5 5600H, NVIDIA RTX 3050 Ti Laptop (4 GB), 16 GB RAM. On the same LAN as CENTRAL (assumed); sends its assets through `\\CENTRAL\Incoming\LAPTOP\`. With 4 GB of VRAM, Modly/TripoSplat and heavy Editor scenes do not fit: heavy 3D goes to PC Romelt.
  - **Remote machine:** low-powered, outside the LAN. Pure code (built and tested by CENTRAL), commit reviews, game ideas, concepts and English text. Specs still pending.
  - **CENTRAL:** builds and packages the game. Set up with `SETUP_MAQUINA_CENTRAL.md`.

## Timeline
| Date | What we did | Result |
|---|---|---|
| 2026-11-09 | Evaluated the jam and UE 5.9.0 (CL 60351404) | We use 5.9.0 |
| 2026-11-09 | Tested the 5.9 Editor MCP server in an empty project (`MCPProbe59`) | 5/5 steps OK; registered in Claude Code as `unreal-mcp-59` (port 8059) |
| 2026-11-09 | Built an empty C++ module | `Result: Succeeded` |
| 2026-11-09 | Test packaging of the ThirdPersonBP template | 386 MB zip, boots. Recipe in `Package-Jam.ps1` |
| 2026-11-09 | Disk cleanup after the tests | Test projects deleted; notes kept in `D:\MegaJam\_probe_notes` |
| 2026-11-09 | Central machine setup document + `JamKit.zip` | `SETUP_MAQUINA_CENTRAL.md` |
| 2026-11-09 | Dev log system (Claude Code hooks, agent wrapper, Editor log collector, daily draft) | Installed and tested on ASSETS |
| 2026-11-09 | Evaluated 8 links + the "video → Rokoko" animation workflow from a tweet | See "Tools evaluated" |
| 2026-11-09 | AssetSearch (asset-index + EmbeddingGemma 2) | Works: 37,668 assets, ~0.8 s per query |
| 2026-11-09 | Reviewed the MetaHuman mocap tools in 5.9 | Body and face from single-camera video, no account or payment. Real test pending |
| 2026-11-10 | Real MetaHuman Animator test with a team member's video (project `D:\MegaJam\MHProbe59`, all over MCP) | Ingest and face OK (faster than real time on the RX 7700 XT). Body blocked until the free Fab plugin "MetaHuman Animator Markerless Motion Capture" was installed |
| 2026-11-10 | Plugin installed; full-video body solve + export to the UE5 mannequin, all over MCP | **Works.** 1,598/1,598 frames, automatic retarget to Manny, but it takes 34 min for 27 s of video |
| 2026-11-10 | Cleanup after the MetaHuman test | Deleted the `MHProbe59` project (including its copy of the test video) and the 870 MB of extracted frames in `%LOCALAPPDATA%\CaptureManager`. Kept only the evidence in `Devlog\prejam_evidencia` |
| 2026-11-10 | Read the itch.io page and the full Official Rules | See "Rules read". Everything public goes in English from now on |
| 2026-11-10 | Wrote down what our previous project taught us about automated tests, verification time and agent tooling | See "Lessons from our previous project: tests and verification time". No code is carried over, only the rules |

## Tested technical recipe
- **Packaging:** UAT `BuildCookRun -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -archive`.
  - **Without `-nocompileeditor`:** with non-default plugins the cook asks for the project's Editor target.
  - `bUseZenStore=False` in `[/Script/UnrealEd.ProjectPackagingSettings]`: with Zen the oplog failed on this machine (cause not verified). It does not change the packaged game.
  - `.pdb` files (341 MB) are left out of the zip.
- **Editor MCP:** flags `-ModelContextProtocolStartServer -ModelContextProtocolPort=N`, path `/mcp`; progressive discovery `list_toolsets` → `describe_toolsets` → `call_tool` (~128 toolsets).
  - Right after launch it answers "slow task" for a while: retry.
  - Close it with `QUIT_EDITOR` over MCP (`Stop-EditorMcp.ps1`); `CloseMainWindow` hung.
- **Compiler:** per the 5.9 `Windows_SDK.json`, MSVC 14.50.35717+ or 14.44.35207+ and SDK 10.0.26100.0. UBT here uses 14.50.35735.

## Tools evaluated
| Tool | Verdict | Reason |
|---|---|---|
| **AssetSearch** (`codepawl/asset-index` + EmbeddingGemma 2) | **Adopted** (ASSETS machine) | 37,668 licensed assets (22,696 Megascans under the Fab Standard license, the rest CC0). Works with Spanish or English queries. Produces the third-party list |
| **MetaHuman Animator (UE 5.9)** | **Main choice for animation** | See the next section. Free, local, no account |
| Video → **Rokoko Vision** → IK Retargeter (tweet) | Fallback | Needs an account; free plan 30 s/month, FBX only; Basic USD 12/month for 600 s. Commercial terms of the free plan not confirmed. Not tested |
| **Hyper3D Rodin** | Owner decision pending | Text/image → 3D with PBR, UE/Blender plugin, API and MCP. Uses credits; read the commercial terms first |
| **Patch.gg** | Possible | ~50,000 open assets with license info, CLI and MCP. Windows only, pricing not published |
| **NVIDIA Lyra 2.0** | Secondary | Image → 3D world (splats or mesh). License unclear and heavy on GPU. At most for distant backdrops |
| WarforgeTech gist (Sand3D → Roblox) | Pattern only | Targets Roblox; the idea of a skill that rebuilds props in Blender while keeping their license can be adapted |
| TailorRig (TRIG) | No | Research; code and license unclear |
| 67.js | No | Web JavaScript engine, unrelated to Unreal |
| Atomic Agent | No | Very early generic agent |
| `fab-megascans-claimer.js` (DevTools script, reviewed 2026-11-10) | Safe code, owner's call | Clicks "Add to My Library" on the free Quixel Megascans listings of fab.com, inside an iframe on the same site. It makes no requests to other sites and reads no cookies or tokens. Risks: automated claiming may break Fab's terms (account risk), and it depends on an obfuscated CSS class, so it can break. It only adds licenses; it downloads nothing. We were not asked to run it. **Context (CG Channel, 2026-11-06):** Quixel made the whole Megascans library (~23,000 assets) free on Fab on Nov 5, 2026, with no announced end date; listings switch over in stages until **Nov 12**, so the script would miss whatever is still priced. Standard Epic license unchanged; listings now say "Allows usage with AI: Yes" |

## Team workflow (draft, 2026-11-10)
- 4 machines: CENTRAL, ASSETS and a third one on the same LAN; a fourth, low-powered, remote machine.
- Owner decisions: **git for code and text only** (`Source/`, `Config/`, `.uproject`, `Devlog/`, `Tools/`, docs, CSV/JSON data). **Assets travel over the LAN.** Gameplay logic goes in C++; Blueprints only configure values.
- Proposal: CENTRAL owns the game's `Content/`. Other machines produce assets in their own work projects, hand them over with UE's **Migrate** into a shared folder on CENTRAL (`\\CENTRAL\Incoming\<MACHINE>\`), and the CENTRAL agent imports them. `Content/` is backed up daily because it has no git history.
- Remote machine: pure code tasks (built and tested by CENTRAL before merging), reviewing the other machines' commits, game ideas, concepts and English text.
- GitHub accounts: **one per person, not per machine** (GitHub's terms allow one free personal account per person). Machines used by the same person share that account, but each one signs in separately (`gh auth login` or its own SSH key), so one machine's access can be revoked without touching the others. Each other person uses their own account, invited as a collaborator to the private repo. Which machine made a commit comes from `JAM_MACHINE` (dev log) and, optionally, a commit trailer.

## MetaHuman Animator in UE 5.9 (checked in the installed engine source)
Plugins that ship with the engine (`Engine/Plugins/MetaHuman`): MetaHuman Animator, MetaHuman Creator, MetaHuman Live Link, MetaHuman Animation Tools, Capture Manager and Performance Capture Workflow (Mocap Manager panel).

- **Performance inputs** (`EDataInputType`): depth footage (iPhone/stereo), **audio** and **monocular video** (webcam or a normal phone).
- **Body from mono video:** monocular input exposes `bBodyTracking`, with automatic or manual body height (recommended range 145–190 cm). It can also export a "Performer Skeleton" with the actor's proportions.
- **Face:** from mono video or from audio (Speech2Face, with a "mood" setting).
- **Live:** MetaHuman Live Link accepts video (webcam) and audio sources.
- **Inference:** NNE with ONNX Runtime (`NNERuntimeORT`).
- **Agents:** 5.9 ships toolsets for the AI Assistant and MCP: `MetaHumanAnimatorToolset` (footage inspection and animation generation), `CaptureManagerToolset` (mono/stereo take ingest, Live Link Face, OBS), `MetaHumanGenerator` (character creation). Good for the dev log: every step lands in the Editor log.
- **Advantage over Rokoko:** no account, no seconds limit, no cost, fully local.

### Real test on 2026-11-10
- **Video:** a raw animation-reference recording of a team member (not game content, will not be used in the game): vertical phone, 1080×1920, 59.94 fps, 26.8 s (1,599 frames), full body in front of a green cloth, sword moves with a stick.
- **Project:** `D:\MegaJam\MHProbe59` (plugins MetaHuman, MetaHumanCharacter, MetaHumanAnimationTools, AllMetaHumanToolsets, CaptureManagerEditor, MetaHumanBodyTracker + MCP; template mannequin in `Content/Characters`). Everything was driven over MCP, without touching the UI.
- **MCP flow that worked:**
  1. `CaptureManagerToolset.CaptureManagerIngestTools.ScanForTakes` (`searchDirectory`).
  2. `IngestMonoVideo` (`videoFilePath`, `audioFilePath`, `slate`, `takeNumber`, `conversionParams`). Creates `/Game/CaptureManager/Imports/<slate>_1/CD_<slate>_1`. Frames go to `%LOCALAPPDATA%\CaptureManager\Media\<project>\` (433 MB as JPG for 27 s).
  3. `MetaHumanAnimatorToolset.MetaHumanAnimatorPerformanceTools.InspectFootageForPerformance` (`footage`).
  4. `GenerateAnimationFromFootage` (`footage`, `params{inputType:"MonoFootage", cameraIndex, processingFrameRange, enableOptions, disableOptions}`, `outputLocation{assetPath, assetName}`).
  5. `MetaHumanAnimatorExportTools.ExportPerformanceAnimationSequence` (`performance`, `options{bExportBody, bExportFace, targetSkeletonOrSkeletalMesh, bodyRetargeter, assetPath, assetName}`).
- **Gotcha:** with `rotation:"Auto"` the frames of the vertical phone video came out sideways. Pass `rotation:"CW_90"` (verified).
- **Face:** 4 s (240 frames) solved in 5.5 s on the RX 7700 XT, so inference runs on AMD. Only 120 of the 240 frames have data: in a full-body shot the face is small or turned. For faces, record separately and close up.
- **Body:** without the extra plugin `InspectFootageForPerformance` returns `bBodySolveAvailable=false`. The engine ships only the interface; the solver is the **MetaHuman Animator Markerless Motion Capture** plugin (experimental, free on Fab, Windows only, Mocapade technology; content mounts at `/MetaHumanBodyTracker`). It was installed from the owner's Fab library into the 5.9 engine.
- **Installed plugin:** `Engine/Plugins/Marketplace/MetaHumanBodyTracker_5.9` (v1.0.0), enabled in the `.uproject` as `MetaHumanBodyTracker`. It brings its own models (ViTPose, SAM2, Detectron2, SMPL-X), the MetaHuman skeleton and retargeters (`RTG_SMPL_MH`, `RTG_MH_IKRig`, plus UEFN mannequin variants). It includes attribution notices (ACCAD Open Motion Project, Motion Capture Data) to credit in the submission if we use it.
- **Body, whole video (1,598 frames, 26.7 s), body only** (`enableOptions:["BodyTracking"]`, `disableOptions:["FaceTracking"]`):
  - Result `Produced`: 1,598/1,598 frames with body data.
  - **Time: 2,024 s (34 min)**, about 75× the video length. Stages: 59 s, 438 s, 255 s, **1,141 s** and 130 s. The log says "Pipeline using unspecified GPU". With the Editor open, under 3 GB of 15 GB RAM were free.
  - What it means for the jam: 1 minute of mocap ≈ 75 minutes of processing. Record short takes trimmed to what we need, and process them on the ASSETS machine while working on something else.
- **Export:** `ExportPerformanceAnimationSequence` with `bExportBody:true`, `bExportFace:false`:
  - no target → `AS_take01_body_performer` (the performer's skeleton);
  - `targetSkeletonOrSkeletalMesh` = `SKM_Manny_Simple` and no retargeter → `AS_take01_body_Manny`. **Retargeting to the UE5 mannequin is automatic**; no IK Retargeter had to be built.
- **Visual review:** `EditorAppToolset.OpenEditorForAsset` + `SceneCaptureToolset.RecordViewport` (`source:"EditorViewport"`, the animation editor's `viewportId`) record the preview to MP4. The poses read clearly: two-handed grip, overhead swing, thrusts, low guard. The colored blocks in the MP4 are recording artifacts, not animation.
  - Evidence: `D:\MegaJam\Devlog\prejam_evidencia\2026-11-10_mha_cuerpo_comparacion.mp4` (original and mannequin side by side, roughly synced) and `..._poses.jpg`.
- **Not evaluated yet:** foot sliding, fine jitter, and whether the stick covering the arms confuses the wrists. Cleanup would happen in Unreal (curves or Control Rig) or in Cascadeur; the plugin ships `MetaHuman_ControlRig`.
- **Crash on exit (fixed):** if an animation editor (Persona) is still open, `QUIT_EDITOR` and `ExitEditor` crash during shutdown (`EXCEPTION_ACCESS_VIOLATION` in `UnrealEditor-Persona`). Saved work is safe, but unsaved packages survive only as autosaves and the next launch shows "Restore Packages". That is how the Performance asset was lost (it is not saved automatically after processing); Restore brought it back.
  - `Stop-EditorMcp.ps1` now closes every open asset first with `EditorAppToolset.CloseEditorForAsset`, which **requires** `unsavedChanges` (`Fail`/`Save`/`Discard`; the script uses `Save`). Without it the tool fails and the editor stays open: that is why the first two fix attempts failed. Verified: clean exit with the animation editor open.
  - The script warns if the Editor log shows a crash on exit.
  - Rule: save the Performance right after processing it.
- **Kit fix:** the dev log scripts now also read `JAM_MACHINE` from the user environment, so a process started before the variable existed does not log under the PC name.

## Rules read (2026-11-10)
Sources: the itch.io page and the Official Rules PDF (`2026_Epic_AI_MegaJam_Official_Rules_EN.pdf`, 16 pages, on Box; read in the browser, not downloaded).
- **Window:** Nov 12 00:00 ET to **Nov 15 20:59 ET**. itch.io shows the submission window in local time. Theme livestream: Nov 11, 14:00 ET. Daily livestream at 14:00 ET.
- **Team:** 1 to 5 people, unlimited agents. One submission per team. Each member's full name and Epic account email, plus who is the Team Owner. 18+ with an active Epic account (not "Cabined").
- **UE submission:** Windows package (development build), playable on Windows 10/11; zip `TeamName_GameName` under 1 GB (use butler for bigger builds). Upload to your own itch.io account, then submit to the jam. Unpackaged projects are not accepted.
- **Video:** 30–60 s, **1920×1080 or higher**, gameplay in the first 5 s.
- **Language (rule 7(i)): EVERYTHING in English:** in-game text, audio, video and entry information. That covers the public dev log and the UI.
- **Pre-jam preparation (7(h)):** AI tools, agent configurations, MCP settings, prompt libraries and scripts are allowed. **Epic may ask for them, or their version control history, to verify.** Everything else (gameplay, levels, art) is created during the jam, except declared third-party assets.
- **AI:** allowed everywhere, including inside the game. How much AI is used neither adds nor removes points. AI-generated content is **not** a sourced asset.
- **Sourced assets:** declared at submission, **not scored**, and we must hold the license.
- **Not allowed:** an obvious extension of an official Epic tutorial; staying within what the starter templates offer; requiring extra software to play; third-party trademarks or logos (only the Unreal Engine logo, per its branding guidelines); content above a Teen (13+) rating.
- **Scoring (1–5 each):** ambition and innovation, theme, fun, gameplay, visuals, level design and progression, audio. Ties are re-scored with fun counted double.
- **Modifiers:** Glass Box (dev log; must be submitted **together with the game during the jam**), Solo Driver, Best Team, Render Royalty, Pocket Powerhouse (mobile), Walkie-Talkie (UEFN only). At most one main prize and one modifier per submission.
- **Jurisdictions:** Iran, Iraq, North Korea, Somalia, Sudan, Syria, Crimea, Donetsk and Luhansk are excluded. Residents of Brazil do not receive cash prizes.
- **Resolved (2026-11-12):** the "AI tool credits" were announced in the itch.io email of Nov 11 (see the next section).

## Theme and partner perks (itch.io email, 2026-11-11, read 2026-11-12)
Source: "[itch.io] 2026 Epic AI MegaJam - Your Epic AI MegaJam perks + the theme reveal" (from the Epic Games jam account). Claim links and codes are for registered jammers only: do not share them outside the team. Using perks is optional and does not affect judging.

**Theme: DIRECTOR'S CUT.** "Your vision, your edits, your final cut." The feature that did not make it, the mechanic scoped out at 2 AM, the shelved idea: build it now. "Using AI agents? You're directing those too. Set the vision, delegate tasks, and decide what makes the cut." The theme is scored (rules: 1–5), so the game must show it, and the dev log should show the directing/cutting decisions.

| Perk | What | Deadline / notes |
|---|---|---|
| Anthropic | $25 Claude API credits (Console, not a Claude.ai subscription); last 90 days after claiming | Link opens Nov 12 00:00 PT, closes **Nov 27** 23:59 PT; first come, first served |
| SpaceXSI | $50 credits across **Cursor** and Grok Bot; one claim per account; credits last **7 days** from claiming | Claim by **Nov 15**. Relevant: the art laptop uses Cursor |
| Meshy | 1 free month of Meshy Pro, 1,000 credits (web or API) | New account by **Nov 17**; credit card required, auto-renews unless canceled |
| Hyper3D | Rodin Business subscription, 208 credits; code in the email | Redeem by **Nov 16**, benefits expire Nov 20. **Hyper3D has a $10,000 cash pool for teams that used Rodin**, and it depends on listing the tool in "Tools you used" |
| Tripo | 3,000 credits + 7 days of Tripo Pro | Valid Nov 11–17 PST; redeem the code by **Nov 16** |
| fal | $30 credits | Claim by **Nov 30**; use the personal account, not a team |
| PBRgen | Free access and credits, valid Nov 7–15 | Sign-up link in the email |
| Diversion | 1 month of cloud version control | Code in the email, by Nov 22. We use git, so low priority |
| Rokoko | 1 month of Rokoko Basic | Register first; a voucher arrives when the jam starts. Our main animation path is MetaHuman Animator (see above) |
| Reallusion | Free iClone Personal (up to 30 exports) + Character Creator 5 discounts | — |
| WeLoveIndies | Free sound and music, **non-commercial use only** | Account needed; check the license before using any track and list it as a sourced asset |
| NetApp + AWS | After the jam closes: first 25 UE jammers who email get a free technical session (AWS Game Dev Toolkit, Horde) | Not needed for the jam |

**Other points from the email**
- **Submission deadline:** Nov 15, 5:59 PM PST = **8:59 PM EST** (matches what we already had).
- **"Tools you used" field at submission:** list every tool we use. Partner prizes (Hyper3D's cash pool, the NVIDIA RTX Award) depend on it, so keep a running list in the dev log.
- **NVIDIA RTX Award:** requires NvRTX (NVIDIA's Unreal branch), or at least one feature from NvRTX, DLSS or ACE in the game, plus the NVIDIA questions at submission. We are on stock UE 5.9.0 (CL 60351404) and the engine is pinned until delivery, so this needs an owner decision.
- Questions and teammates: Unreal Engine Forums and the UEFN Discord. Kickoff replay is on Twitch, YouTube and X.
- **Owner decisions pending:** which perks to claim (each needs an account, and some a credit card), and whether to use Rodin (the "Hyper3D Rodin" row above was marked pending). Every generated asset must be logged; AI-generated content is not a sourced asset, but perk-tool output still goes in "Tools you used".
- **Kit changes:** the kit `CLAUDE.md` now covers mandatory English, 1080p video, Teen rating, no third-party logos, git history and no extra software. Decision (owner, 2026-11-10): the public dev log is written in English from the start; the templates are now in English.

## Idea: modular props from static meshes (2026-11-10)
Source: Epic's article "From static Fab assets to modular, configurable ones, entirely inside UE" (Franck Debaene, Epic Solutions Architect, X / @UnrealEngine, 2026-11-10), part of the MegaJam AI showcase series.
- **What it does:** Claude Code drives UE through the built-in MCP server (Experimental) with remote Python enabled, which gives it Geometry Scripting. A `/parametrize <mesh> <plain-language description>` skill analyzes a single-mesh asset, splits it into repeatable modules (drawers, legs, planks), closes the holes left by the split, and generates a spline-driven PCG graph (grammar nodes) that rebuilds the object at any size. About 10–20 min per asset; the skill hands the heavy work to a dedicated agent in `.claude/`.
- **Why it matters for us:** it is the same stack we already prepared (MCP, skills/agents in `.claude/`, Python over MCP). A prop-adaptation skill like this could be part of our pre-jam tooling (declared under rule 7(h)); the props themselves would be made during the jam.
- **Limits:** works on repetitive, mostly straight-edged objects (furniture, fences, shelves, facades, bridges). Not useful for rocks, vegetation or characters. The demo assets were chosen to work; messy meshes or shared UVs will take more iteration.
- **Licensing:** the article used CC-BY assets with attribution. Any Fab asset we split and modify must allow modification, and it is still a sourced asset to declare at submission.
- **Status:** idea only, not tested. If we try it before Nov 12, test on one simple prop (a fence or a shelf) and log the result here.

## Lessons from our previous project: tests and verification time (2026-11-10)
Source: two clean-up lots on Panthelia, our long-running Unreal project (UE 5.8, C++, about 7,300 automation tests), both closed on 2026-11-10. We carry over the rules, not the code. Each rule below cost us real hours once.

**What it cost.** In one week of November that project ran 1,333 headless test runs: 25.7 hours of machine time. 586 of those were mutation runs, each with two rebuilds. The pre-commit check took 334 s and the full suite about 15 minutes. After the two lots the same checks take about 90–105 s and about 6 minutes, with 37 pointless tests removed and everything else still green. None of the gain came from faster hardware.

### What to test in a 4-day jam
- **Test only logic that is easy to break and hard to see by playing:** scoring, save and load, state machines, generators, math. Everything else is verified by playing the packaged build.
- **Keep the whole suite under 30 seconds of test time.** A check that takes minutes stops being run.
- **Do not write tests that are not tests.** We deleted 37: scans of the source text, checks that a field exists, and copies of constants, colors and UI positions. They fail when someone edits a number, not when the game breaks.
- **No registers.** No test catalog, no list of "known failures", no exclusion list. A run is green or red, and a red test is fixed or deleted the same day. Our registers needed a full suite run just to be refreshed, and a stale one blocked the check before Unreal even started.
- **Read a test's body before deleting it; the name can lie.** Six of the 37 looked like layout checks by name and tested real behavior. Two reviewers caught it and we restored them.
- **Name tests by tier, and never let a test name also be a folder.** If `Game.Save.Validate` exists and someone adds `Game.Save.Validate.Slots`, Unreal's Automation tree keeps the folder and silently stops running the first test. We had one that had stopped running and nobody saw it, because a test that does not run does not fail.

### A test needs an oracle that can fail
- For every test, ask: **what wrong implementation would still pass this?** If the answer is "most of them", the test is decoration.
- **One check per fixture when state latches.** We checked two things in one session of a tool whose "write detected" flag stays set until the session ends. When the first check tripped it, the second passed for the wrong reason. Each check now runs in its own session.
- **Mutate by risk, not per test.** Break the code on purpose in 1 to 3 places per change (a wrong result, a skipped guard, an unwanted side effect) and confirm the matching test fails for the expected reason. Mutating every test was our largest single source of runs.

### Measure before optimizing
- **Get three numbers first:** Editor boot, the sum of the test durations, and the wall time. What is left over is overhead. In our 225 s check they were 37 s, 114 s and 225 s, and even the 114 s was inflated: each duration included a wait for the next Editor frame. The real work of the tests was about 45 s.
- **Measure the whole, not the part.** Booting without the startup map cut one test family from 273 s to 31 s, so it looked like a pure win. On the full check it was slower (254 s against 225 s) and one test failed, because other tests relied on what the map kept loaded.
- **Timings vary from run to run.** The same code measured between 87 s and 108 s. Compare two paths on the same tree and report a range, not the best run.

### Where the time really went
- **The engine's `Automation RunTests` path spends about 4.5 Editor frames on every test.** The controller and the worker talk through messages, and the worker closes each test on the next frame. A test that takes microseconds reported 9.4 ms. With a few dozen tests this does not matter; with thousands it was most of the run. If it ever matters: `FAutomationTestFramework::RunSmokeTests` shows the engine running tests back to back (`StartTestByName` then `StopTest`), and that is the pattern we followed.
  - Details that bit us when doing that: drive it from `FCoreDelegates::OnEndFrame`, not from `FTSTicker` (some tests tick the global ticker themselves); a test that queues latent commands must get a real engine frame before the first one runs; flush the logs written by other threads before closing a test; and stop the engine's Automation worker, because it also advances latent commands and can close your test first.
- **Editor boot is most of a small run.** Flags that helped a headless run: `-unattended -nopause -nosplash -NullRHI -nosound -nocrashreports`.
  - `-nocrashreports`: every ensure a test triggers on purpose spent 2–10 s preparing a crash report nobody reads (one test went from 13.6 s to 0.9 s).
  - The startup map: loading our test level cost 14 s of every boot. **Give the jam project a small, nearly empty Editor startup map**, not the gameplay level.
  - `bEnableUncontrolledChangelists=False` (`[/Script/SourceControl.SourceControlPreferences]`, Editor ini): it scanned all 26,000 assets at boot.
  - Join several test filters with `+` in one run, so boot is paid once.
  - Exit with a forced exit once the report is on disk: the Editor's orderly shutdown took about 5 s.
- **Filters match by substring unless anchored.** Start a filter with `^` and end it with a dot (`^Game.Save.`), or `Game.Save` also selects `Tools.Whatever.Game.SaveSlots`.

### Costs that grow with everything that is loaded
- **A tool operation must not cost in proportion to all loaded content.** One of our Editor tools hashed every loaded package on each call: 2.9 s per call, 741 s of a 1,031 s suite, and just as slow in real use. If a tool truly has to read everything, do it in parallel (`ParallelFor`) and keep the decision on the game thread. With that, and with the tools booting without the map, it went down to 158 s.
- **Do not key a cache on file size plus modification time.** Unreal returns file times with **one-second** precision on Windows, so two same-size writes within a second look identical. A reviewer caught this before it shipped.
- **Do not call `CollectGarbage(RF_NoFlags)` in a test.** It unloads every asset nothing references. The next test that needs the player content reloads it, about 12 s each time. Use `GARBAGE_COLLECTION_KEEPFLAGS`.
- **Watch how long the player Blueprint takes to load, from day one.** Ours takes about 12 s. We did not find out why; the log points at skinned meshes and motion-matching data loaded through hard references. Whatever a character Blueprint hard-references is loaded with it, every time it is needed. Use soft references for anything not needed at spawn.
- **A test loads what it needs and cleans up what it loads.** Tests that leaned on the startup map's leftovers, or that loaded a level and left it initialized, were the ones that broke when the environment changed.

### Verdicts and evidence
- **Decide pass or fail from the report, not only from the exit code.** Our runner cross-checks the list of tests it asked for against the list that actually ran, and refuses a run where they differ.
- **Guard against a silently small selection.** A typo in a filter, or a module that failed to load, gives a green run with half the tests. Require a minimum count.
- **Count error-level ensures in the log even when every test passes.** An ensure during module start-up fires before any test exists to see it.
- **Every run writes to its own new folder** with a small summary file. Never reuse a report folder.
- **When a suite fails and no code changed, compare the engine version in the two logs first.** Our engine updated itself from 5.8.3 to 5.8.4 overnight and broke four tests. For the jam: stay on 5.9.0 and do not let the launcher update it before submission.

### Working with agents and scripts
- **A second model reviews anything that guards data, and every finding is checked in the code before acting on it.** The one-second timestamp problem was missed by the author and by one reviewer, and found by the other.
- **One build or test run at a time per machine.** A build cannot link while an Editor process holds the DLLs, and a measurement taken during someone else's build is worthless.
- **Long commands run in the background and report when done.** No polling loops.
- **Windows PowerShell 5.1 reads a `.ps1` without a BOM as ANSI.** Accented characters in the script turn into garbage; ours corrupted a source comment. Keep scripts ASCII or save them with a BOM.
- **`ValidateSet` ignores case, `-ceq` does not.** `-Mode direct` passed validation and then took the wrong branch. Normalize the value once.
- **In JavaScript, `text.replace(a, b)` expands `$` patterns inside `b`.** A replacement text that contained a dollar sign followed by a backtick pasted the whole preceding document into itself three times. Use `split(a).join(b)` or slices for literal replacements.
- **Put one-off scripts in a file, not inline in a shell command.** Nested quoting failed on us more than once in a single afternoon.

### The short version for the jam
1. A handful of tests on pure logic, under 30 s in total. Play the packaged build for everything else.
2. Green or red. No lists of exceptions.
3. Anchored filters, one boot per run, a nearly empty startup map.
4. Before optimizing anything, measure boot, test time and wall time.
5. No tool call or test may cost in proportion to everything that is loaded.
6. Stay on UE 5.9.0 until we submit.

**Kit change (2026-11-10):** the kit `CLAUDE.md` now has a "Tests y verificación" section with these rules (plus "every test must be able to fail" and the script traps), so every agent working on the jam project loads them without having to open this log.

## Open decisions (owner)
- Moving assets between machines: GitHub LFS (1 GB free quota) or a cloud folder.
- AI provider credits: Rodin, Unreal Canvas, ElevenLabs.
- Team name and members (each with their Epic email).

## Reminders
- The Claude Code sign-in on ASSETS expires between Nov 15 and 18: run `claude auth login` on Nov 11 or 12.
- After the jam: `Tools/Devlog/Install-DevlogHooks.ps1 -Uninstall` on each machine.
- Start the EmbeddingGemma server with `D:\Models\EmbeddingGemma-2\start_embedding_server.bat` (about 1.5 GB of VRAM).
