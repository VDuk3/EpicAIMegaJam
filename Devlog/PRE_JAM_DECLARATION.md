# Pre-jam work declaration

The jam rules (Official Rules 7(h)) allow preparing AI tools, agent configurations, MCP settings, prompt libraries and scripts before **Nov 12, 2026 at 00:00 ET**, but not building the game. Anything made earlier must be declared. This file declares it. The full log of the preparation, with dates and results, is in `PREJAM_PREP.md`.

**No game content was created before the jam:** no levels, no mechanics, no game assets and no gameplay code.

## Tools and scripts prepared before the jam (2026-11-09 to 2026-11-11)
| Item | What it is | Where |
|---|---|---|
| Claude Code dev log hook | Logs every Claude Code event as JSONL | `.claude/hooks/devlog-hook.ps1`, `Devlog/.jamroot` |
| `Install-DevlogHooks.ps1` | Installs the hook in the Claude Code user settings | `Tools/Devlog/` |
| `Invoke-LoggedAgent.ps1` | Wrapper that logs every run of an external agent | `Tools/Devlog/` |
| `Collect-UnrealAgentLog.ps1` | Extracts agent actions (MCP, Assistant, Canvas) from the Editor log | `Tools/Devlog/` |
| `New-DevlogDay.ps1` | Builds the redacted daily draft | `Tools/Devlog/` |
| `Start-EditorMcp.ps1`, `Stop-EditorMcp.ps1`, `Test-UnrealMcp.ps1` | Open, close and test the UE 5.9 Editor with MCP | `Tools/Jam/` |
| `Package-Jam.ps1` | Win64 packaging and submission zip (tested recipe) | `Tools/Jam/` |
| `Backup-Content.ps1` | Daily mirror copy of the game project without build folders (the `Content/` has no git history) | `Tools/Jam/` |
| `Setup-IncomingShare.ps1` | Sets up the LAN folder where the other machines drop assets (write allowed, delete denied) | `Tools/Jam/` |
| `AssetSearch` | Local semantic search over 37,668 licensed assets (public index `codepawl/asset-index`, EmbeddingGemma 2 embeddings). Contains no assets, only metadata | `Tools/AssetSearch/` |
| Dev log templates | `README.md`, `TEMPLATE_DAY.md`, this file, `PREJAM_PREP.md` | `Devlog/` |
| Project `CLAUDE.md` | Working rules for the agents | root |

## Technical tests before the jam (no game content)
- 2026-11-09: the UE 5.9 Editor MCP server answered (handshake, toolset list and a real call) in an empty project.
- 2026-11-09: built an empty C++ module (only `IMPLEMENT_PRIMARY_GAME_MODULE`) to validate the toolchain. The project was deleted.
- 2026-11-09: test packaging of Epic's ThirdPersonBP template (386 MB zip, boots correctly). The test project was deleted.
- 2026-11-10: MetaHuman Animator body capture from a single phone video, driven entirely over MCP, exported to the UE5 mannequin. The test video is a raw recording of a team member swinging a stick; it is not game content and will not be used in the game. Test project: `MHProbe59` (tooling probe only).
- 2026-11-11: central machine (CENTRAL) setup, details in `SETUP_REPORT.md`: 10 PASS, 0 FAIL, 0 NEEDS-HUMAN (the LAN asset share was tested from the ASSETS machine: write and overwrite allowed, delete and rename denied). Editor MCP 5/5, C++ build with MSVC 14.50.35737, test packaging of Epic's ThirdPersonBP template (385.8 MB zip, boots) and of an empty C++ module (311.5 MB), dev log hooks and wrapper verified. All test projects were deleted.

## Prompts, prompt templates or assets prepared before the jam
- (none so far; anything prepared will be listed here with its date)

## AI tools we plan to use
- Claude Code (Anthropic): main agent for code and for the Editor through MCP.
- MetaHuman Animator (UE 5.9) + the Fab plugin "MetaHuman Animator Markerless Motion Capture" (Epic, free): body animation from single-camera video, tested on 2026-11-10 with a test take that is not game content.
- (to complete: Unreal Assistant, Unreal Canvas and its provider, Fab generation, Modly, ElevenLabs…)
