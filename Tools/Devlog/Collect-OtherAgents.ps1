<#
Collect-OtherAgents.ps1 - Copia a la bitacora cruda de una maquina SIN repo las conversaciones
de Claude Code y Gemini CLI, que en esas maquinas no pasan por los hooks de la jam.

- Claude Code guarda cada sesion en %USERPROFILE%\.claude\projects\<proyecto>\<sesion>.jsonl
  (y sus subagentes en <sesion>\subagents\*.jsonl). Van a
  <LogRoot>\<MAQUINA>\<fecha>\transcripts\claude-<sesion>[-<subagente>].jsonl
- Gemini CLI guarda sus chats en %USERPROFILE%\.gemini\tmp\<proyecto>\ (chats\*.json, logs.json).
  Van a <LogRoot>\<MAQUINA>\<fecha>\gemini\<proyecto>\...
Solo lo tocado desde el inicio de la jam; sin imagenes base64 (JamLogCopy.ps1); solo copia si
cambio. Nunca borra ni modifica los originales. Lo llama Sync-DevlogToCentral.ps1.
(OpenCode guarda sus sesiones en una base SQLite: se lanza SIEMPRE con Invoke-LoggedAgent.ps1.)
#>
param(
    [string]$ClaudeHome = (Join-Path $env:USERPROFILE '.claude'),
    [string]$GeminiHome = (Join-Path $env:USERPROFILE '.gemini'),
    [string]$LogRoot = 'C:\JamLog\raw',
    [string]$Since = '2026-11-12T05:00:00Z'
)
. (Join-Path $PSScriptRoot 'JamLogCopy.ps1')
$sinceUtc = [DateTime]::Parse($Since).ToUniversalTime()
$machine = $env:JAM_MACHINE
if (-not $machine) { $machine = [Environment]::GetEnvironmentVariable('JAM_MACHINE', 'User') }
if (-not $machine) { $machine = $env:COMPUTERNAME }

function DayDir([IO.FileInfo]$f) {
    $c = $f.CreationTimeUtc; if ($c -lt $sinceUtc) { $c = $sinceUtc }
    return (Join-Path $LogRoot ("$machine\" + $c.ToString('yyyy-MM-dd')))
}

$nc = 0
$projects = Join-Path $ClaudeHome 'projects'
if (Test-Path -LiteralPath $projects) {
    foreach ($f in Get-ChildItem -LiteralPath $projects -Recurse -File -Filter '*.jsonl' -ErrorAction SilentlyContinue) {
        if ($f.LastWriteTimeUtc -lt $sinceUtc) { continue }
        # <proyecto>\<sesion>.jsonl  o  <proyecto>\<sesion>\subagents\<agente>.jsonl
        if ($f.Directory.Name -eq 'subagents') { $name = 'claude-' + $f.Directory.Parent.Name + '-' + $f.BaseName + '.jsonl' }
        else { $name = 'claude-' + $f.BaseName + '.jsonl' }
        $dest = Join-Path (Join-Path (DayDir $f) 'transcripts') $name
        if (Test-JamLogCurrent $f $dest) { continue }
        try { Copy-JamLogSlim $f.FullName $dest; $nc++ } catch { Write-Warning "No se pudo copiar $($f.FullName): $($_.Exception.Message)" }
    }
}

$ng = 0
$gtmp = Join-Path $GeminiHome 'tmp'
if (Test-Path -LiteralPath $gtmp) {
    foreach ($f in Get-ChildItem -LiteralPath $gtmp -Recurse -File -Include '*.json', '*.jsonl' -ErrorAction SilentlyContinue) {
        if ($f.LastWriteTimeUtc -lt $sinceUtc) { continue }
        $rel = $f.FullName.Substring($gtmp.Length).TrimStart('\')
        $dest = Join-Path (Join-Path (DayDir $f) 'gemini') $rel
        if (Test-JamLogCurrent $f $dest) { continue }
        try { Copy-JamLogSlim $f.FullName $dest; $ng++ } catch { Write-Warning "No se pudo copiar $($f.FullName): $($_.Exception.Message)" }
    }
}
Write-Output "claude: $nc transcripts copiados; gemini: $ng archivos copiados"
