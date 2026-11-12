<#
Sync-DevlogToCentral.ps1 - Copia los registros crudos de una maquina SIN repo (C:\JamLog\raw)
a su carpeta en el recurso Incoming de la CENTRAL. La CENTRAL los pasa a Devlog\raw con
Import-RemoteDevlog.ps1 y van al repo con los demas.

- Antes de copiar ejecuta los recolectores que haya en su misma carpeta: Collect-CodexSessions,
  Collect-CursorTranscripts y Collect-OtherAgents (Claude Code, Gemini CLI).
- Solo copia (robocopy /E): el recurso no deja borrar ni renombrar, y no hace falta.
- Registro en C:\JamLog\sync.log. Codigos: 0 OK, 2 error de robocopy (>= 8).

Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File Sync-DevlogToCentral.ps1 [-Dest <ruta>]
#>
param(
    [string]$Source = 'C:\JamLog\raw',
    [string]$Dest = '\\192.168.31.188\Incoming\LAPTOP\_devlog_raw',
    [string]$Log = 'C:\JamLog\sync.log',
    # Otras carpetas Devlog\raw de esta maquina (p. ej. la del repo en ASSETS, donde escribe
    # Invoke-LoggedAgent.ps1). Solo se envian archivos modificados desde el 12 nov.
    [string[]]$ExtraSource = @()
)
$collectors = 'Collect-CodexSessions.ps1', 'Collect-CursorTranscripts.ps1', 'Collect-OtherAgents.ps1'
foreach ($collect in Get-ChildItem -LiteralPath $PSScriptRoot -File -ErrorAction SilentlyContinue | Where-Object { $collectors -contains $_.Name }) {
    try { $c = & $collect.FullName -LogRoot $Source 2>&1 | Out-String; Add-Content -LiteralPath $Log -Value ("{0}Z {1}" -f [DateTime]::UtcNow.ToString('s'), $c.Trim()) } catch { Add-Content -LiteralPath $Log -Value ("{0}Z {1} error: {2}" -f [DateTime]::UtcNow.ToString('s'), $collect.Name, $_.Exception.Message) }
}
if (-not (Test-Path -LiteralPath $Source)) { Add-Content -LiteralPath $Log -Value "$([DateTime]::UtcNow.ToString('s'))Z sin origen $Source"; exit 0 }
$rc = 0
$out = & robocopy $Source $Dest /E /FFT /R:1 /W:2 /NP /NJH /NDL /XF *.part 2>&1
$code = $LASTEXITCODE
Add-Content -LiteralPath $Log -Value ("{0}Z robocopy={1} -> {2}" -f [DateTime]::UtcNow.ToString('s'), $code, $Dest)
if ($code -ge 8) { Add-Content -LiteralPath $Log -Value ($out | Out-String); $rc = 2 }
foreach ($x in $ExtraSource) {
    if (-not $x -or -not (Test-Path -LiteralPath $x)) { continue }
    $out = & robocopy $x $Dest /E /FFT /R:1 /W:2 /NP /NJH /NDL /XF *.part /MAXAGE:20261112 2>&1
    $code = $LASTEXITCODE
    Add-Content -LiteralPath $Log -Value ("{0}Z robocopy={1} {2} -> {3}" -f [DateTime]::UtcNow.ToString('s'), $code, $x, $Dest)
    if ($code -ge 8) { Add-Content -LiteralPath $Log -Value ($out | Out-String); $rc = 2 }
}
exit $rc
