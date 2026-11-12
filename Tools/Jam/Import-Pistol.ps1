<#
Import-Pistol.ps1 - Imports the cyberpunk pistol (meshes, textures, materials) from Glyphbound/SourceArt into
/Game/Glyphbound/Weapons/Pistol with the Editor's Python commandlet (Tools/Jam/import_pistol.py).
Content is not in git: run this on CENTRAL after the Blender export changes. The Editor must be closed.

Usage:
  powershell -File Tools/Jam/Import-Pistol.ps1
Output: the script's lines, the error lines of the log, OK or FAILED. Exit code 0 = imported and saved.
#>
param(
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject'),
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [int]$TimeoutSeconds = 900
)
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$script = (Resolve-Path (Join-Path $PSScriptRoot 'import_pistol.py')).Path
$exe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path $exe)) { Write-Host "No editor at $exe"; exit 2 }

$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$logDir = Join-Path $projDir 'Saved\Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "import_pistol_$stamp.log"

# Materials compile here, so the commandlet needs a renderer (no -NullRHI).
$argList = @("`"$Project`"", '-run=pythonscript', "-script=`"$script`"", "-abslog=`"$log`"",
    '-AllowCommandletRendering', '-unattended', '-nopause', '-nosplash', '-nosound', '-nocrashreports')

$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
$timedOut = -not $p.WaitForExit($TimeoutSeconds * 1000)
if ($timedOut) {
    Write-Host "TIMEOUT after $TimeoutSeconds s: stopping the commandlet."
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
}
$sw.Stop()

if (-not (Test-Path $log)) { Write-Host "No log at $log (exit $($p.ExitCode))"; exit 2 }
$lines = Get-Content $log -Encoding UTF8
$ours = @($lines | Where-Object { $_ -match 'import_pistol:' })
$errors = @($lines | Where-Object { $_ -match 'LogPython: Error|import_pistol: ERROR|Traceback|Fatal error|Assertion failed|Unhandled Exception|LogFbx: Error' })
$ok = @($ours | Where-Object { $_ -match 'import_pistol: OK' }).Count -gt 0

$ours | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
if ($errors.Count -gt 0) {
    Write-Host '---- errors ----'
    $errors | Select-Object -First 60 | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
}
Write-Host ("Import: ok={0} errors={1} exit={2} wall={3:N0} s  log: {4}" -f $ok, $errors.Count, $p.ExitCode, $sw.Elapsed.TotalSeconds, $log)
if ($ok -and $errors.Count -eq 0 -and -not $timedOut) { Write-Host 'OK'; exit 0 }
Write-Host 'FAILED'
exit 1
