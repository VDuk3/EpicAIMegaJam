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
    return $s
}

$prompts = New-Object System.Collections.Generic.List[object]
$toolCount = @{}
$sessions = @{}
$agentRuns = New-Object System.Collections.Generic.List[object]

foreach ($machineDir in Get-ChildItem (Join-Path $root 'Devlog\raw') -Directory -ErrorAction SilentlyContinue) {
    $dayDir = Join-Path $machineDir.FullName $Date
    if (-not (Test-Path $dayDir)) { continue }
    foreach ($f in Get-ChildItem $dayDir -Filter 'claude-*.jsonl') {
        foreach ($line in [IO.File]::ReadAllLines($f.FullName, $utf8)) {
            try { $o = $line | ConvertFrom-Json } catch { continue }
            $sessions["$($o.machine)/$($o.session)"] = $true
            if ($o.event -eq 'UserPromptSubmit') {
                $prompts.Add([pscustomobject]@{ ts = $o.ts_utc; machine = $o.machine; text = (Redact ([string]$o.payload.prompt)) })
            } elseif ($o.event -eq 'PostToolUse') {
                $k = [string]$o.payload.tool_name
                if ($toolCount.ContainsKey($k)) { $toolCount[$k]++ } else { $toolCount[$k] = 1 }
            }
        }
    }
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
[void]$sb.AppendLine("- Claude Code sessions: $($sessions.Count)")
[void]$sb.AppendLine("- Team prompts to agents: $($prompts.Count)")
[void]$sb.AppendLine("- External agent runs: $(@($agentRuns | Where-Object event -eq 'start').Count)")
[void]$sb.AppendLine("- MCP calls inside Unreal: $mcpCalls")
[void]$sb.AppendLine()
[void]$sb.AppendLine("### Tools used by Claude Code")
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
