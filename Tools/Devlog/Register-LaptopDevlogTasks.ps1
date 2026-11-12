<#
Register-LaptopDevlogTasks.ps1 - Tareas programadas de la bitacora en una maquina SIN repo.
No necesita administrador (tareas del usuario actual, solo con la sesion iniciada).

- JamDevlogPreJamCleanup: una sola vez, 12 nov 04:50 UTC (10 min antes de la jam). Borra
  C:\JamLog\raw (registros de las pruebas previas) SOLO si aun no empezo la jam
  (05:00 UTC); si la maquina estaba apagada y corre tarde, no borra nada.
- JamDevlogSync: desde el 12 nov 05:05 UTC, cada 10 min durante 6 dias, ejecuta
  Sync-DevlogToCentral.ps1.
Las horas se fijan en UTC y se convierten a la hora local de la maquina.

Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File Register-LaptopDevlogTasks.ps1 [-Unregister]
#>
param(
    [string]$BinDir = 'C:\JamLog\bin',
    [string]$RawDir = 'C:\JamLog\raw',
    [switch]$Unregister
)
$ErrorActionPreference = 'Stop'
foreach ($n in 'JamDevlogPreJamCleanup', 'JamDevlogSync') {
    if (Get-ScheduledTask -TaskName $n -ErrorAction SilentlyContinue) { Unregister-ScheduledTask -TaskName $n -Confirm:$false }
}
if ($Unregister) { Write-Host 'Tareas quitadas'; exit 0 }

$sync = Join-Path $PSScriptRoot 'Sync-DevlogToCentral.ps1'
if (-not (Test-Path $sync)) { throw "No existe $sync" }
[void][IO.Directory]::CreateDirectory($BinDir)
if ((Resolve-Path $PSScriptRoot).Path -ne (Resolve-Path $BinDir).Path) { Copy-Item -LiteralPath $sync -Destination $BinDir -Force }
$syncBin = Join-Path $BinDir 'Sync-DevlogToCentral.ps1'

$jamStartUtc = [DateTime]::Parse('2026-11-12T05:00:00Z').ToUniversalTime()
$principal = New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited

# Limpieza previa a la jam (con guarda horaria en UTC)
$code = @"
`$limit = [DateTime]::Parse('2026-11-12T05:00:00Z').ToUniversalTime()
`$log = 'C:\JamLog\cleanup.log'
if ([DateTime]::UtcNow -lt `$limit) { Remove-Item -LiteralPath '$RawDir' -Recurse -Force -ErrorAction SilentlyContinue; Add-Content -LiteralPath `$log -Value "`$([DateTime]::UtcNow.ToString('s'))Z raw borrado" } else { Add-Content -LiteralPath `$log -Value "`$([DateTime]::UtcNow.ToString('s'))Z omitido: ya empezo la jam" }
"@
$enc = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($code))
$a1 = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -WindowStyle Hidden -EncodedCommand $enc"
$t1 = New-ScheduledTaskTrigger -Once -At ($jamStartUtc.AddMinutes(-10).ToLocalTime())
$s1 = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
Register-ScheduledTask -TaskName 'JamDevlogPreJamCleanup' -Action $a1 -Trigger $t1 -Settings $s1 -Principal $principal | Out-Null

# Sincronizacion cada 10 minutos
$a2 = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$syncBin`""
$t2 = New-ScheduledTaskTrigger -Once -At ($jamStartUtc.AddMinutes(5).ToLocalTime()) -RepetitionInterval (New-TimeSpan -Minutes 10) -RepetitionDuration (New-TimeSpan -Days 6)
$s2 = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 5)
Register-ScheduledTask -TaskName 'JamDevlogSync' -Action $a2 -Trigger $t2 -Settings $s2 -Principal $principal | Out-Null

Get-ScheduledTask -TaskName 'JamDevlogPreJamCleanup', 'JamDevlogSync' | ForEach-Object {
    $i = $_ | Get-ScheduledTaskInfo
    '{0}: estado={1}, proxima={2}' -f $_.TaskName, $_.State, $i.NextRunTime
}
