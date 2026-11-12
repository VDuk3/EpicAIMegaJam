<#
Collect-CodexSessions.ps1 - Copia las sesiones de Codex (app de escritorio, extension o CLI
interactiva) a la bitacora cruda de una maquina SIN repo, para que no se pierdan los chats que
no se lanzan con Invoke-LoggedAgent.ps1.

Codex guarda cada sesion completa (prompts, respuestas, herramientas) en
  %USERPROFILE%\.codex\sessions\AAAA\MM\DD\rollout-*.jsonl  (y archived_sessions\)
Este script copia las que se tocaron desde el inicio de la jam a
  <LogRoot>\<MAQUINA>\<AAAA-MM-DD UTC de creacion>\codex-<nombre original>.jsonl
Solo copia si cambiaron (tamano o fecha). Nunca borra ni modifica las originales.
Lo llama Sync-DevlogToCentral.ps1 antes de cada envio, si esta en la misma carpeta.
#>
param(
    [string]$CodexHome = (Join-Path $env:USERPROFILE '.codex'),
    [string]$LogRoot = 'C:\JamLog\raw',
    [string]$Since = '2026-11-12T05:00:00Z'
)
. (Join-Path $PSScriptRoot 'JamLogCopy.ps1')
$sinceUtc = [DateTime]::Parse($Since).ToUniversalTime()
$machine = $env:JAM_MACHINE
if (-not $machine) { $machine = [Environment]::GetEnvironmentVariable('JAM_MACHINE', 'User') }
if (-not $machine) { $machine = $env:COMPUTERNAME }
$n = 0
foreach ($sub in 'sessions', 'archived_sessions') {
    $dir = Join-Path $CodexHome $sub
    if (-not (Test-Path -LiteralPath $dir)) { continue }
    foreach ($f in Get-ChildItem -LiteralPath $dir -Recurse -File -Filter 'rollout-*.jsonl' -ErrorAction SilentlyContinue) {
        if ($f.LastWriteTimeUtc -lt $sinceUtc) { continue }
        $created = $f.CreationTimeUtc
        if ($created -lt $sinceUtc) { $created = $sinceUtc }
        $destDir = Join-Path $LogRoot ("$machine\" + $created.ToString('yyyy-MM-dd'))
        $dest = Join-Path $destDir ('codex-' + $f.Name)
        if (Test-JamLogCurrent $f $dest) { continue }
        try {
            Copy-JamLogSlim $f.FullName $dest   # sin imagenes base64 (JamLogCopy.ps1)
            $n++
        } catch { Write-Warning "No se pudo copiar $($f.FullName): $($_.Exception.Message)" }
    }
}
Write-Output "codex: $n sesiones copiadas"
