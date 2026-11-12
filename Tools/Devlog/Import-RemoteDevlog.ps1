<#
Import-RemoteDevlog.ps1 - (CENTRAL) Pasa a Devlog\raw los registros crudos que las maquinas
SIN repo dejan en Incoming\<MAQUINA>\_devlog_raw (ver Sync-DevlogToCentral.ps1).

- Solo copia (robocopy /E): no borra nada en ninguno de los dos lados.
- Con -Register crea la tarea JamDevlogImport (cada 10 min desde el 12 nov 05:10 UTC,
  durante 6 dias). Codigos: 0 OK, 2 error de robocopy (>= 8).

Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Devlog/Import-RemoteDevlog.ps1 [-Register]
#>
param(
    [string]$JamRoot = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [string[]]$Machines = @('LAPTOP'),
    [switch]$Register
)
if ($Register) {
    $ErrorActionPreference = 'Stop'
    $self = Join-Path $PSScriptRoot 'Import-RemoteDevlog.ps1'
    if (Get-ScheduledTask -TaskName 'JamDevlogImport' -ErrorAction SilentlyContinue) { Unregister-ScheduledTask -TaskName 'JamDevlogImport' -Confirm:$false }
    $at = [DateTime]::Parse('2026-11-12T05:10:00Z').ToUniversalTime().ToLocalTime()
    $a = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$self`""
    $t = New-ScheduledTaskTrigger -Once -At $at -RepetitionInterval (New-TimeSpan -Minutes 10) -RepetitionDuration (New-TimeSpan -Days 6)
    $s = New-ScheduledTaskSettingsSet -StartWhenAvailable -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 5)
    $p = New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited
    Register-ScheduledTask -TaskName 'JamDevlogImport' -Action $a -Trigger $t -Settings $s -Principal $p | Out-Null
    $i = Get-ScheduledTaskInfo -TaskName 'JamDevlogImport'
    Write-Host "JamDevlogImport: proxima=$($i.NextRunTime)"
    exit 0
}
$log = Join-Path $JamRoot 'Devlog\raw\_import.log'
[void][IO.Directory]::CreateDirectory((Split-Path $log -Parent))
$rc = 0
foreach ($m in $Machines) {
    $src = Join-Path $JamRoot "Incoming\$m\_devlog_raw"
    if (-not (Test-Path -LiteralPath $src)) { continue }
    & robocopy $src (Join-Path $JamRoot 'Devlog\raw') /E /R:1 /W:2 /NP /NJH /NJS /NDL /NFL | Out-Null
    $code = $LASTEXITCODE
    Add-Content -LiteralPath $log -Value ("{0}Z {1} robocopy={2}" -f [DateTime]::UtcNow.ToString('s'), $m, $code)
    if ($code -ge 8) { $rc = 2 }
}
exit $rc
