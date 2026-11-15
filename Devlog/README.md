# Jam dev log (goal: the Glass Box prize for the best dev log)

Everything any agent or person does on the project must be recorded here. If it is not in the dev log, it did not happen as far as the judges are concerned.

**Language:** the Official Rules (7(i)) require every part of the submission in English, and that includes the published dev log. Write `days/*.md`, `media/` captions and `PRE_JAM_DECLARATION.md` in English. Raw logs (`raw/`) and the team's prompts to agents can be in any language. When a day entry quotes a prompt written in Spanish, include the original and an English translation.

## Layout

```
Devlog/
  README.md                  this file (the rules)
  TEMPLATE_DAY.md            template for the daily entry
  PRE_JAM_DECLARATION.md     what was prepared BEFORE Nov 12 (the jam requires declaring it)
  PREJAM_PREP.md             full pre-jam log: timeline, tests, tools evaluated, rules read
  raw/<MACHINE>/             raw automatic logs (NEVER published as they are)
    <YYYY-MM-DD>/claude-<session>.jsonl      every Claude Code event (hook)
    <YYYY-MM-DD>/transcripts/<session>.jsonl full transcript of each session
    <YYYY-MM-DD>/agents.jsonl                every run of an external agent
    <YYYY-MM-DD>/agents/<run>/               prompt, full output and diffstat of that run
    unreal/<log>.jsonl                       agent actions inside the Editor (MCP, Assistant, Canvas)
  days/<YYYY-MM-DD>_DRAFT.md                 automatic draft (already redacted)
  days/<YYYY-MM-DD>.md                       final entry for the day, written by a person (in English)
  media/<YYYY-MM-DD>/                        screenshots, gifs and clips of the day
```

## Rules for everyone (people and agents)

1. **Claude Code** is logged automatically by the **user-level** hooks that `Tools/Devlog/Install-DevlogHooks.ps1` installs on each machine. They log every session opened in the jam root or any subfolder (the root is the folder that contains `Devlog/.jamroot`). Do not disable them and do not delete `.jamroot`.
2. **Any other agent** (Codex, cursor-agent, OpenCode, DeepSeek, Gemini, generation scripts) is ALWAYS launched with:
   `powershell -File Tools/Devlog/Invoke-LoggedAgent.ps1 -Agent <name> -Model <model> -PromptFile <prompt.md> -Purpose "<what for>" -Command '<command>'`
   - Use `-Shell cmd` if the command contains `< file`.
   - That is what records it. An agent launched by hand leaves no record.
   - **Cursor** (the art LAPTOP, which has no repo) is logged by user-level Cursor hooks instead: `Tools/Devlog/Install-CursorHooks.ps1` installs `cursor-devlog-hook.ps1`, which writes `C:\JamLog\raw\<MACHINE>\<date>\cursor-<conversation>.jsonl` (plus transcripts). `Register-LaptopDevlogTasks.ps1` copies that folder every 10 minutes to `Incoming\LAPTOP\_devlog_raw` on CENTRAL, and CENTRAL's `JamDevlogImport` task (`Import-RemoteDevlog.ps1`) moves it into `Devlog/raw/`, where `New-DevlogDay.ps1` reads it. The same kit runs on ASSETS (destination `Incoming\ASSETS\_devlog_raw`).
   - **Codex chats** (desktop app, IDE extension or interactive CLI, i.e. anything not launched through `Invoke-LoggedAgent.ps1`) on machines without the repo: before each sync, `Collect-CodexSessions.ps1` copies the session files Codex keeps in `%USERPROFILE%\.codex\sessions` (full prompts, replies and tool calls) into `C:\JamLog\raw\<MACHINE>\<date>\codex-*.jsonl`. `New-DevlogDay.ps1` reads those too.
   - **Cursor transcripts:** `Collect-CursorTranscripts.ps1` also copies the full agent transcripts Cursor keeps in `%USERPROFILE%.cursorprojects*gent-transcripts`, so chats from before the hook was installed are not lost. The draft takes Cursor prompts from the transcript whenever there is one (it is complete; the hook missed some prompts before the mutex fix).
   - **Claude Code and Gemini CLI** on machines without the repo: `Collect-OtherAgents.ps1` copies their session files (`%USERPROFILE%.claudeprojects`, `%USERPROFILE%.gemini	mp`). OpenCode keeps its sessions in a SQLite database, so it is always launched through `Invoke-LoggedAgent.ps1`; on ASSETS the sync also sends that repo's `Devlogaw` (`-ExtraSource`).
   - **Size:** every collector strips base64 images (screenshots, renders) from the copies (`JamLogCopy.ps1`): one Codex session on ASSETS went from 110 MB to 4.2 MB with every message and tool call intact. `Import-RemoteDevlog.ps1` refuses files over 50 MB and logs them, because `Devlog/raw` goes to GitHub, which rejects files over 100 MB.
   - On CENTRAL, Codex and OpenCode always run through `Invoke-LoggedAgent.ps1`, so this collection is only needed on machines without the repo.
3. **Prompts as files:** external agents get their prompt in a `.md` under `Prompts/`, never typed loose in the terminal.
4. **Web or desktop tools** (Fab, cloud Canvas, audio or image generators, Blender by hand): in the day entry, write down the tool, the prompt, the result and the final asset, and save a screenshot in `media/`.
5. **Unreal Editor:** always close it with `Tools/Jam/Stop-EditorMcp.ps1`, which closes open asset editors first (an open animation editor crashes the Editor on exit) and collects the log. If it was closed some other way, or before deleting `Saved/`, run `powershell -File Tools/Devlog/Collect-UnrealAgentLog.ps1 -ProjectDir <folder of the .uproject>`.
6. **`JAM_MACHINE` variable:** each machine sets its own (`CENTRAL`, `ASSETS`…) so we know where each thing happened:
   `setx JAM_MACHINE CENTRAL`
7. **Small, frequent commits** with clear messages: the daily draft lists them. Commit `Devlog/raw/` too (the repo is private). Epic may ask for the version control history.
8. **End of day:** run `powershell -File Tools/Devlog/New-DevlogDay.ps1 -Date <YYYY-MM-DD>`. Then a person writes `days/<date>.md` from `TEMPLATE_DAY.md`, starting from the draft.
9. **Publish:** only the final entries `days/<date>.md` and the block notes `days/*_block.md` (never the automatic `*_DRAFT.md`: they quote raw prompts in Spanish and LAN details; they stay in the repo as evidence), `media/`, `SUBMISSION.md`, `PRE_JAM_DECLARATION.md` and `PREJAM_PREP.md`. Raw logs may contain keys or emails: the draft is already redacted, but check it before publishing. The dev log link goes in the submission, **before the deadline**.
10. **Times in UTC.** The jam ends on Nov 15 at 20:59 EST, which is **01:59 UTC on Nov 16**.
