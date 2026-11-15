<#
New-DevlogDay.ps1 - Arma el BORRADOR de la entrada diaria de la bitacora a partir de los
registros crudos de TODAS las maquinas (Devlog/raw/*/<fecha>/) y de git.

El borrador sale ya REDACTADO (claves, tokens, correos) porque es lo que se publica.
Los crudos (Devlog/raw) nunca se publican tal cual.

Uso:  powershell -File Tools/Devlog/New-DevlogDay.ps1 -Date 2026-11-12
Salida: Devlog/days/<fecha>_DRAFT.md   (la persona lo completa usando TEMPLATE_DAY.md). Sale en ingles: la bitacora publica va en ingles (Official Rules 7(i)).
#>
param([string]$Date = ([DateTime]::UtcNow.ToString('yyyy-MM-dd')))

$utf8 = New-Object System.Text.UTF8Encoding($false)
$root = (Get-Location).Path

function Redact([string]$s) {
    if (-not $s) { return $s }
    $s = $s -replace 'sk-[A-Za-z0-9_\-]{16,}', '[KEY-REDACTED]'
    $s = $s -replace '(gh[pousr]_[A-Za-z0-9]{20,})', '[TOKEN-REDACTED]'
    $s = $s -replace '(?i)(bearer\s+)[A-Za-z0-9\._\-]{12,}', '$1[TOKEN-REDACTED]'
    $s = $s -replace '(?i)((api[_-]?key|token|secret|password)\s*[:=]\s*)\S+', '$1[REDACTED]'
    $s = $s -replace '[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}', '[EMAIL]'
    # Windows user folders carry a person's name; invite codes are personal referral codes.
    $s = $s -replace '(?i)([A-Z]:[\\/]+Users[\\/]+)[^\\/\s"''`)]+', '$1<user>'
    $s = $s -replace '(?i)(invite_code=)[A-Za-z0-9]+', '$1[REDACTED]'
    $s = $s -replace '\b(10|192\.168|172\.(1[6-9]|2[0-9]|3[01]))(\.\d{1,3}){1,3}\b', '[LAN-IP]'
    $s = $s -replace '(?i)\bDESKTOP-[A-Z0-9]+\b', '[HOST]'
    return $s
}

$prompts = New-Object System.Collections.Generic.List[object]
$toolCount = @{}
$sessions = @{}
$agentRuns = New-Object System.Collections.Generic.List[object]
$harnessNotices = 0

foreach ($machineDir in Get-ChildItem (Join-Path $root 'Devlog\raw') -Directory -ErrorAction SilentlyContinue) {
    $dayDir = Join-Path $machineDir.FullName $Date
    if (-not (Test-Path $dayDir)) { continue }
    # claude-*.jsonl: hook de Claude Code; cursor-*.jsonl: hook de Cursor (Tools/Devlog/cursor-devlog-hook.ps1)
    foreach ($f in Get-ChildItem $dayDir -File | Where-Object { $_.Name -match '^(claude|cursor)-.*\.jsonl$' }) {
        foreach ($line in [IO.File]::ReadAllLines($f.FullName, $utf8)) {
            try { $o = $line | ConvertFrom-Json } catch { continue }
            $agentName = if ($o.agent) { [string]$o.agent } else { 'claude-code' }
            $sessions["$agentName/$($o.machine)/$($o.session)"] = $agentName
            if ($o.event -ceq 'beforeSubmitPrompt') {
                # Si hay transcript de esta conversacion, los prompts salen de ahi (es completo; el
                # hook perdio prompts antes del arreglo del mutex).
                if (@(Get-ChildItem (Join-Path $dayDir 'transcripts') -Filter "cursor-$($o.session)-*.jsonl" -ErrorAction SilentlyContinue).Count -gt 0) { continue }
                $prompts.Add([pscustomobject]@{ ts = $o.ts_utc; machine = "$($o.machine), Cursor"; text = (Redact ([string]$o.payload.prompt)) })
            } elseif ($o.event -ceq 'postToolUse') {
                $k = 'Cursor: ' + [string]$o.payload.tool_name
                if ($toolCount.ContainsKey($k)) { $toolCount[$k]++ } else { $toolCount[$k] = 1 }
            } elseif ($o.event -eq 'UserPromptSubmit') {
                # Los avisos automaticos del arnes (fin de tareas en segundo plano, eventos de
                # otras sesiones, CI...) llegan como UserPromptSubmit pero no son prompts del equipo.
                if ([string]$o.payload.prompt -match '^\s*<(task-notification|system-reminder|side-sessions-event|ci-monitor-event)\b') { $harnessNotices++; continue }
                $prompts.Add([pscustomobject]@{ ts = $o.ts_utc; machine = $o.machine; text = (Redact ([string]$o.payload.prompt)) })
            } elseif ($o.event -eq 'PostToolUse') {
                $k = [string]$o.payload.tool_name
                if ($toolCount.ContainsKey($k)) { $toolCount[$k]++ } else { $toolCount[$k] = 1 }
            }
        }
    }
    # codex-rollout-*.jsonl: sesiones de Codex copiadas por Collect-CodexSessions.ps1 (formato
    # propio de Codex: una linea por item; los mensajes del usuario incluyen contexto inyectado
    # que empieza por '<' o '# AGENTS.md', y eso no es un prompt del equipo).
    foreach ($f in Get-ChildItem $dayDir -File -Filter 'codex-*.jsonl') {
        $sessions["codex/$($machineDir.Name)/$($f.BaseName)"] = 'codex'
        foreach ($line in [IO.File]::ReadAllLines($f.FullName, $utf8)) {
            try { $o = $line | ConvertFrom-Json } catch { continue }
            if ($o.type -ne 'response_item' -or -not $o.payload) { continue }
            $pt = [string]$o.payload.type
            if ($pt -eq 'message' -and $o.payload.role -eq 'user') {
                foreach ($c in @($o.payload.content)) {
                    $t = [string]$c.text
                    if (-not $t -or $t -match '^\s*(<|# AGENTS\.md)') { continue }
                    $prompts.Add([pscustomobject]@{ ts = [string]$o.timestamp; machine = "$($machineDir.Name), Codex"; text = (Redact $t) })
                }
            } elseif ($pt -in @('function_call', 'custom_tool_call', 'local_shell_call')) {
                $name = [string]$o.payload.name; if (-not $name) { $name = 'shell' }
                $k = 'Codex: ' + $name
                if ($toolCount.ContainsKey($k)) { $toolCount[$k]++ } else { $toolCount[$k] = 1 }
            }
        }
    }
    # transcripts\cursor-<id>-<id>.jsonl: transcript completo de un chat de Cursor (del hook o de
    # Collect-CursorTranscripts.ps1). Es la fuente de los prompts de esa conversacion; las
    # herramientas salen del hook si existe (trae nombres MCP) y si no, del transcript.
    # Los prompts van en <user_query>, con la hora en <timestamp>.
    $tdir = Join-Path $dayDir 'transcripts'
    if (Test-Path $tdir) {
        foreach ($f in Get-ChildItem $tdir -File -Filter 'cursor-*.jsonl') {
            if ($f.Name -notmatch '^cursor-([0-9a-fA-F\-]{36})-') { continue }
            $cid = $Matches[1]
            $hasHook = Test-Path (Join-Path $dayDir "cursor-$cid.jsonl")
            $sessions["cursor/$($machineDir.Name)/$cid"] = 'cursor'
            foreach ($line in [IO.File]::ReadAllLines($f.FullName, $utf8)) {
                try { $o = $line | ConvertFrom-Json } catch { continue }
                foreach ($c in @($o.message.content)) {
                    if ($o.role -eq 'user' -and $c.type -eq 'text' -and ([string]$c.text) -match '(?s)<user_query>\s*(.*?)\s*</user_query>') {
                        $q = $Matches[1]
                        $ts = $f.LastWriteTimeUtc.ToString('o')
                        if (([string]$c.text) -match '<timestamp>\w+, (\w+ \d+, \d{4}), (\d+:\d+ [AP]M) \(UTC([+-]\d+)\)</timestamp>') {
                            try { $ts = ([DateTime]::ParseExact("$($Matches[1]) $($Matches[2])", 'MMM d, yyyy h:mm tt', [Globalization.CultureInfo]::InvariantCulture)).AddHours(-[int]$Matches[3]).ToString('yyyy-MM-ddTHH:mm:00Z') } catch {}
                        }
                        $prompts.Add([pscustomobject]@{ ts = $ts; machine = "$($machineDir.Name), Cursor"; text = (Redact $q) })
                    } elseif (-not $hasHook -and $o.role -eq 'assistant' -and $c.type -eq 'tool_use') {
                        $k = 'Cursor: ' + [string]$c.name
                        if ($toolCount.ContainsKey($k)) { $toolCount[$k]++ } else { $toolCount[$k] = 1 }
                    }
                }
            }
        }
    }
    # transcripts\claude-<sesion>.jsonl SIN hook (claude-<sesion>.jsonl en la carpeta del dia):
    # sesiones de Claude Code de maquinas sin repo, recogidas por Collect-OtherAgents.ps1.
    # Los de subagentes (claude-<sesion>-<agente>.jsonl) no aportan prompts del equipo.
    if (Test-Path $tdir) {
        foreach ($f in Get-ChildItem $tdir -File -Filter 'claude-*.jsonl') {
            if ($f.BaseName -notmatch '^claude-([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})$') { continue }
            $sid = $Matches[1]
            if (Test-Path (Join-Path $dayDir "claude-$sid.jsonl")) { continue }
            $sessions["claude-code/$($machineDir.Name)/$sid"] = 'claude-code'
            foreach ($line in [IO.File]::ReadAllLines($f.FullName, $utf8)) {
                try { $o = $line | ConvertFrom-Json } catch { continue }
                if ($o.type -eq 'user' -and -not $o.isMeta -and $o.message) {
                    $texts = @()
                    if ($o.message.content -is [string]) { $texts = @($o.message.content) }
                    else { $texts = @($o.message.content | Where-Object { $_.type -eq 'text' } | ForEach-Object { [string]$_.text }) }
                    foreach ($t in $texts) {
                        if (-not $t -or $t -match '^\s*<') { if ($t -match '^\s*<(task-notification|system-reminder|side-sessions-event|ci-monitor-event)\b') { $harnessNotices++ }; continue }
                        $prompts.Add([pscustomobject]@{ ts = [string]$o.timestamp; machine = "$($machineDir.Name), Claude Code"; text = (Redact $t) })
                    }
                } elseif ($o.type -eq 'assistant' -and $o.message) {
                    foreach ($c in @($o.message.content)) {
                        if ($c.type -eq 'tool_use') { $k = [string]$c.name; if ($toolCount.ContainsKey($k)) { $toolCount[$k]++ } else { $toolCount[$k] = 1 } }
                    }
                }
            }
        }
    }
    # gemini\: chats de Gemini CLI (Collect-OtherAgents.ps1); solo se cuentan los archivos
    $gdir = Join-Path $dayDir 'gemini'
    if (Test-Path $gdir) { foreach ($g in Get-ChildItem $gdir -Recurse -File -Filter 'session-*.json') { $sessions["gemini/$($machineDir.Name)/$($g.BaseName)"] = 'gemini' } }
    $aj = Join-Path $dayDir 'agents.jsonl'
    if (Test-Path $aj) {
        foreach ($line in [IO.File]::ReadAllLines($aj, $utf8)) {
            try { $o = $line | ConvertFrom-Json } catch { continue }
            if ($o.event -eq 'end') { $agentRuns.Add($o) }
            elseif ($o.event -eq 'start') { $agentRuns.Add($o) }
        }
    }
}

$mcpCalls = 0
foreach ($u in Get-ChildItem (Join-Path $root 'Devlog\raw') -Recurse -Filter *.jsonl -ErrorAction SilentlyContinue | Where-Object { $_.Directory.Name -eq 'unreal' }) {
    foreach ($line in [IO.File]::ReadAllLines($u.FullName, $utf8)) {
        if ($line -match ('"log_ts":"' + $Date.Replace('-', '.'))) { if ($line -match 'Dispatching tool') { $mcpCalls++ } }
    }
}

$gitLog = ''
try { $gitLog = (git log --since="$Date 00:00:00 +0000" --until="$Date 23:59:59 +0000" --pretty=format:'- `%h` %s' 2>$null | Out-String) } catch {}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# Day $Date - generated DRAFT (complete it with TEMPLATE_DAY.md)")
[void]$sb.AppendLine()
[void]$sb.AppendLine("## Automatic data")
[void]$sb.AppendLine("- Claude Code sessions: $(@($sessions.Values | Where-Object { $_ -eq 'claude-code' }).Count)")
[void]$sb.AppendLine("- Cursor conversations: $(@($sessions.Values | Where-Object { $_ -eq 'cursor' }).Count)")
[void]$sb.AppendLine("- Codex sessions (desktop or IDE, collected): $(@($sessions.Values | Where-Object { $_ -eq 'codex' }).Count)")
[void]$sb.AppendLine("- Gemini CLI sessions (collected): $(@($sessions.Values | Where-Object { $_ -eq 'gemini' }).Count)")
[void]$sb.AppendLine("- Team prompts to agents: $($prompts.Count) (automatic harness notices left out: $harnessNotices)")
[void]$sb.AppendLine("- External agent runs: $(@($agentRuns | Where-Object event -eq 'start').Count)")
[void]$sb.AppendLine("- MCP calls inside Unreal: $mcpCalls")
[void]$sb.AppendLine()
[void]$sb.AppendLine("### Tools used by the agents (Claude Code; Cursor and Codex marked)")
foreach ($k in ($toolCount.Keys | Sort-Object { -$toolCount[$_] })) { [void]$sb.AppendLine("- $k : $($toolCount[$k])") }
[void]$sb.AppendLine()
[void]$sb.AppendLine("### External agents")
foreach ($r in ($agentRuns | Where-Object event -eq 'end')) {
    $s = $agentRuns | Where-Object { $_.event -eq 'start' -and $_.run -eq $r.run } | Select-Object -First 1
    [void]$sb.AppendLine(("- {0} {1} ({2}) exit={3} {4}s - {5}" -f $r.ts_utc, $r.agent, $s.model, $r.exit_code, $r.seconds, (Redact ([string]$s.purpose))))
}
[void]$sb.AppendLine()
[void]$sb.AppendLine("### Commits of the day")
[void]$sb.AppendLine($gitLog)
[void]$sb.AppendLine("### Prompts (redacted, in order; translate any quoted in the day entry)")
foreach ($p in ($prompts | Sort-Object ts)) {
    $t = $p.text; if ($t.Length -gt 600) { $t = $t.Substring(0, 600) + ' ...' }
    [void]$sb.AppendLine(("- **{0}** [{1}] {2}" -f $p.ts, $p.machine, ($t -replace "`r?`n", ' ')))
}

$outDir = Join-Path $root 'Devlog\days'
[void][IO.Directory]::CreateDirectory($outDir)
$out = Join-Path $outDir "$($Date)_DRAFT.md"
[IO.File]::WriteAllText($out, $sb.ToString(), $utf8)
Write-Host "Draft: $out"
