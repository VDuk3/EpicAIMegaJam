<#
Sync-DevlogToCentral.ps1 - Copia los registros crudos de una maquina SIN repo (C:\JamLog\raw)
a su carpeta en el recurso Incoming de la CENTRAL. La CENTRAL los pasa a Devlog\raw con
Import-RemoteDevlog.ps1 y van al repo con los demas.

- Antes de copiar ejecuta los Collect-*.ps1 que haya en su misma carpeta (sesiones de Codex,
  transcripts de Cursor).
- Solo copia (robocopy /E): el recurso no deja borrar ni renombrar, y no hace falta.
- Registro en C:\JamLog\sync.log. Codigos: 0 OK, 2 error de robocopy (>= 8).

Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File Sync-DevlogToCentral.ps1 [-Dest <ruta>]
#>
param(
    [string]$Source = 'C:\JamLog\raw',
    [string]$Dest = '\\192.168.31.188\Incoming\LAPTOP\_devlog_raw',
    [string]$Log = 'C:\JamLog\sync.log'
)
foreach ($collect in Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'Collect-*.ps1' -File -ErrorAction SilentlyContinue) {
    try { $c = & $collect.FullName -LogRoot $Source 2>&1 | Out-String; Add-Content -LiteralPath $Log -Value ("{0}Z {1}" -f [DateTime]::UtcNow.ToString('s'), $c.Trim()) } catch { Add-Content -LiteralPath $Log -Value ("{0}Z {1} error: {2}" -f [DateTime]::UtcNow.ToString('s'), $collect.Name, $_.Exception.Message) }
}
if (-not (Test-Path -LiteralPath $Source)) { Add-Content -LiteralPath $Log -Value "$([DateTime]::UtcNow.ToString('s'))Z sin origen $Source"; exit 0 }
$out = & robocopy $Source $Dest /E /FFT /R:1 /W:2 /NP /NJH /NDL 2>&1
$code = $LASTEXITCODE
Add-Content -LiteralPath $Log -Value ("{0}Z robocopy={1} -> {2}" -f [DateTime]::UtcNow.ToString('s'), $code, $Dest)
if ($code -ge 8) { Add-Content -LiteralPath $Log -Value ($out | Out-String); exit 2 }
exit 0
