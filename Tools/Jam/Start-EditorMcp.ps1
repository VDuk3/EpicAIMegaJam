<#
Start-EditorMcp.ps1 - Abre el Editor UE 5.9 con el servidor MCP y espera a que este listo.
Uso:  powershell -File Tools/Jam/Start-EditorMcp.ps1 -Project D:\Jam\MiJuego\MiJuego.uproject -Port 8059
#>
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [int]$Port = 8059,
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [int]$TimeoutSeconds = 900
)
$exe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path $exe)) { throw "No existe $exe" }
$name = [IO.Path]::GetFileNameWithoutExtension($Project)
$log = Join-Path (Split-Path $Project -Parent) "Saved\Logs\$name.log"
$start = Get-Date
$p = Start-Process -FilePath $exe -ArgumentList "`"$Project`"", '-ModelContextProtocolStartServer', "-ModelContextProtocolPort=$Port" -PassThru
$t = 0
while ($t -lt $TimeoutSeconds) {
    Start-Sleep 5; $t += 5
    if ($p.HasExited) { Write-Host "El Editor se cerro (exit $($p.ExitCode))"; exit 1 }
    if ((Test-Path $log) -and (Get-Item $log).LastWriteTime -gt $start) {
        $m = Select-String $log -Pattern 'Starting MCP server|MCP server listener could not start' | Select-Object -Last 1
        if ($m) { Write-Host $m.Line; if ($m.Line -match 'could not') { exit 1 }; Write-Host "MCP listo en $t s (PID $($p.Id))"; exit 0 }
    }
}
Write-Host "Timeout esperando MCP ($TimeoutSeconds s). La primera apertura compila shaders: reintentar con mas tiempo."
exit 1
