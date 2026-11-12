<#
Make-Materials.ps1 - Builds the project's materials from text (Tools/Jam/make_materials.py) with the Editor's
Python commandlet. The material .uasset files are not in git, so run this after a fresh clone and whenever
make_materials.py changes. The Editor must be closed (it would hold the asset files).

Usage:
  powershell -File Tools/Jam/Make-Materials.ps1
Output: OK and the saved assets, or the error lines of the log.
Exit code: 0 = every material saved without Python errors.
#>
param(
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject'),
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [int]$TimeoutSeconds = 900
)
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$script = (Resolve-Path (Join-Path $PSScriptRoot 'make_materials.py')).Path
$exe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path $exe)) { Write-Host "No editor at $exe"; exit 2 }

$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$logDir = Join-Path $projDir 'Saved\Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "make_materials_$stamp.log"

# Compiling a material needs a renderer: no -NullRHI here, and commandlets only start one with -AllowCommandletRendering.
$argList = @("`"$Project`"", '-run=pythonscript', "-script=`"$script`"", "-abslog=`"$log`"",
    '-AllowCommandletRendering', '-unattended', '-nopause', '-nosplash', '-nosound', '-nocrashreports')

$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
$timedOut = -not $p.WaitForExit($TimeoutSeconds * 1000)
if ($timedOut) {
    # A commandlet, not the interactive Editor: stopping it is safe.
    Write-Host "TIMEOUT after $TimeoutSeconds s: stopping the commandlet."
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
}
$sw.Stop()

if (-not (Test-Path $log)) { Write-Host "No log at $log (exit $($p.ExitCode))"; exit 2 }
$lines = Get-Content $log -Encoding UTF8
$ours = @($lines | Where-Object { $_ -match 'make_materials:' })
$errors = @($lines | Where-Object { $_ -match 'LogPython: Error|make_materials: ERROR|Traceback|Fatal error|Assertion failed|Unhandled Exception' })
$ok = @($ours | Where-Object { $_ -match 'make_materials: OK' }).Count -gt 0

$asset = Join-Path $projDir 'Content\Glyphbound\Materials\M_PixelSurface.uasset'
$assetExists = Test-Path $asset
$decal = Join-Path $projDir 'Content\Glyphbound\Materials\M_PixelDecal.uasset'
$decalExists = Test-Path $decal

$ours | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
if ($errors.Count -gt 0) {
    Write-Host '---- errors ----'
    $errors | Select-Object -First 60 | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
}
Write-Host ("Materials: ok={0} asset={1} decal={2} errors={3} exit={4} wall={5:N0} s  log: {6}" -f $ok, $assetExists, $decalExists, $errors.Count, $p.ExitCode, $sw.Elapsed.TotalSeconds, $log)
if ($ok -and $assetExists -and $decalExists -and $errors.Count -eq 0 -and -not $timedOut) { Write-Host 'OK'; exit 0 }
Write-Host 'FAILED'
exit 1
