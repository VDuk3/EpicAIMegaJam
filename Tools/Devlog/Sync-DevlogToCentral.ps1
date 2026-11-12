<#
Sync-DevlogToCentral.ps1 - Copia los registros crudos de una maquina SIN repo (C:\JamLog\raw)
a su carpeta en el recurso Incoming de la CENTRAL. La CENTRAL los pasa a Devlog\raw con
Import-RemoteDevlog.ps1 y van al repo con los demas.

- Antes de copiar, si Collect-CodexSessions.ps1 esta en la misma carpeta, recoge las sesiones
  de Codex de esta maquina.
- Solo copia (robocopy /E): el recurso no deja borrar ni renombrar, y no hace falta.
- Registro en C:\JamLog\sync.log. Codigos: 0 OK, 2 error de robocopy (>= 8).

Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File Sync-DevlogToCentral.ps1 [-Dest <ruta>]
#>
param(
    [string]$Source = 'C:\JamLog\raw',
    [string]$Dest = '\\192.168.31.188\Incoming\LAPTOP\_devlog_raw',
    [string]$Log = 'C:\JamLog\sync.log'
)
$collect = Join-Path $PSScriptRoot 'Collect-CodexSessions.ps1'
if (Test-Path -LiteralPath $collect) {
    try { $c = & $collect -LogRoot $Source 2>&1 | Out-String; Add-Content -LiteralPath $Log -Value ("{0}Z {1}" -f [DateTime]::UtcNow.ToString('s'), $c.Trim()) } catch { Add-Content -LiteralPath $Log -Value ("{0}Z codex error: {1}" -f [DateTime]::UtcNow.ToString('s'), $_.Exception.Message) }
}
if (-not (Test-Path -LiteralPath $Source)) { Add-Content -LiteralPath $Log -Value "$([DateTime]::UtcNow.ToString('s'))Z sin origen $Source"; exit 0 }
$out = & robocopy $Source $Dest /E /FFT /R:1 /W:2 /NP /NJH /NDL 2>&1
$code = $LASTEXITCODE
Add-Content -LiteralPath $Log -Value ("{0}Z robocopy={1} -> {2}" -f [DateTime]::UtcNow.ToString('s'), $code, $Dest)
if ($code -ge 8) { Add-Content -LiteralPath $Log -Value ($out | Out-String); exit 2 }
exit 0
