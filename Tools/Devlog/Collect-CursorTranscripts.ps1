<#
Collect-CursorTranscripts.ps1 - Copia los transcripts de agente de Cursor a la bitacora cruda
de una maquina SIN repo. Cubre las conversaciones de antes de instalar el hook (o en las que
el hook fallo): Cursor guarda cada chat de agente completo en
  %USERPROFILE%\.cursor\projects\<workspace>\agent-transcripts\<id>\<id>.jsonl
Copia los que se tocaron desde el inicio de la jam a
  <LogRoot>\<MAQUINA>\<AAAA-MM-DD UTC de creacion>\transcripts\cursor-<id>-<id>.jsonl
(el mismo nombre que usa cursor-devlog-hook.ps1, asi no hay duplicados). Solo copia si
cambiaron. Nunca borra ni modifica las originales. Lo llama Sync-DevlogToCentral.ps1.
#>
param(
    [string]$CursorHome = (Join-Path $env:USERPROFILE '.cursor'),
    [string]$LogRoot = 'C:\JamLog\raw',
    [string]$Since = '2026-11-12T05:00:00Z'
)
. (Join-Path $PSScriptRoot 'JamLogCopy.ps1')
$sinceUtc = [DateTime]::Parse($Since).ToUniversalTime()
$machine = $env:JAM_MACHINE
if (-not $machine) { $machine = [Environment]::GetEnvironmentVariable('JAM_MACHINE', 'User') }
if (-not $machine) { $machine = $env:COMPUTERNAME }
$n = 0
$projects = Join-Path $CursorHome 'projects'
if (Test-Path -LiteralPath $projects) {
    foreach ($tdir in Get-ChildItem -LiteralPath $projects -Directory -ErrorAction SilentlyContinue) {
        $at = Join-Path $tdir.FullName 'agent-transcripts'
        if (-not (Test-Path -LiteralPath $at)) { continue }
        foreach ($f in Get-ChildItem -LiteralPath $at -Recurse -File -Filter '*.jsonl' -ErrorAction SilentlyContinue) {
            if ($f.LastWriteTimeUtc -lt $sinceUtc) { continue }
            $created = $f.CreationTimeUtc
            if ($created -lt $sinceUtc) { $created = $sinceUtc }
            $id = $f.Directory.Name
            if ($id -eq 'agent-transcripts') { $id = $f.BaseName }
            $destDir = Join-Path $LogRoot ("$machine\" + $created.ToString('yyyy-MM-dd') + '\transcripts')
            $dest = Join-Path $destDir ('cursor-' + $id + '-' + $f.Name)
            if (Test-JamLogCurrent $f $dest) { continue }
            try {
                Copy-JamLogSlim $f.FullName $dest   # sin imagenes base64 (JamLogCopy.ps1)
                $n++
            } catch { Write-Warning "No se pudo copiar $($f.FullName): $($_.Exception.Message)" }
        }
    }
}
Write-Output "cursor: $n transcripts copiados"
