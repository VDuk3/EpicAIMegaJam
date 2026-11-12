<#
Smoke-Jam.ps1 - Runs the game by itself (console command "gb.Smoke": a scripted run of the main systems that
takes screenshots and quits) and reports what happened. It is NOT Play In Editor and nobody plays: it only
tells us whether the game starts, the systems run without crashing, and what the screen looks like.
A person still has to play the build.

Usage:
  powershell -File Tools/Jam/Smoke-Jam.ps1              # uncooked game with the Editor binaries (-game)
  powershell -File Tools/Jam/Smoke-Jam.ps1 -Packaged    # the packaged build in Glyphbound/Build/Windows
Output: the SMOKE lines, errors and warnings of our log category, and the list of screenshots.
Exit code: 0 = the script reached "SMOKE DONE" with no error lines and no crash.
#>
param(
    [switch]$Packaged,
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject'),
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [int]$TimeoutSeconds = 180
)
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$common = @('-windowed', '-ResX=1280', '-ResY=720', '-unattended', '-nosplash', '-nosound', '-ExecCmds="gb.Smoke"')
if ($Packaged) {
    $exe = Join-Path $projDir 'Build\Windows\Glyphbound.exe'
    if (-not (Test-Path $exe)) { Write-Host "No packaged build at $exe"; exit 2 }
    $shotDir = Join-Path $projDir 'Build\Windows\Glyphbound\Saved\Screenshots\Smoke'
    $log = Join-Path $projDir 'Build\Windows\Glyphbound\Saved\Logs\Glyphbound.log'
    $argList = $common
} else {
    $exe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor.exe'
    $shotDir = Join-Path $projDir 'Saved\Screenshots\Smoke'
    $log = Join-Path $projDir "Saved\Logs\smoke_$stamp.log"
    $argList = @("`"$Project`"", '-game', "-abslog=`"$log`"") + $common
}
if (Test-Path $shotDir) { Remove-Item (Join-Path $shotDir '*.png') -Force -ErrorAction SilentlyContinue }
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru
$timedOut = -not $p.WaitForExit($TimeoutSeconds * 1000)
if ($timedOut) {
    # This is the game process, not the Editor: stopping it is safe.
    Write-Host "TIMEOUT after $TimeoutSeconds s: stopping the game process."
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep 2
}
$sw.Stop()
if (-not (Test-Path $log)) { Write-Host "No log at $log (exit $($p.ExitCode))"; exit 2 }
$lines = Get-Content $log -Encoding UTF8
$smoke = @($lines | Where-Object { $_ -match 'SMOKE' })
$bad = @($lines | Where-Object { $_ -match 'LogGB: (Error|Warning)|Fatal error|Unhandled Exception|Assertion failed|Ensure condition failed|appError' })
$otherErrors = @($lines | Where-Object { $_ -match ': Error: ' -and $_ -notmatch 'LogGB' })
$smoke | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
Write-Host '---- problems in our code (LogGB warnings/errors, asserts, crashes) ----'
if ($bad.Count -eq 0) { Write-Host '(none)' } else { $bad | Select-Object -First 40 | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() } }
Write-Host "---- other engine errors: $($otherErrors.Count) ----"
$otherErrors | Select-Object -First 10 | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
$shots = @(Get-ChildItem $shotDir -Filter *.png -ErrorAction SilentlyContinue)
Write-Host "---- screenshots: $($shots.Count) in $shotDir ----"
$shots | ForEach-Object { Write-Host $_.Name }
$done = @($smoke | Where-Object { $_ -match 'SMOKE DONE' }).Count -gt 0
$crashed = @($bad | Where-Object { $_ -match 'Fatal error|Unhandled Exception|Assertion failed|appError' }).Count -gt 0
$errorCount = @($bad | Where-Object { $_ -match 'LogGB: Error' }).Count
Write-Host ("Smoke: done={0} crashed={1} LogGB errors={2} timedOut={3} wall={4:N0} s  log: {5}" -f $done, $crashed, $errorCount, $timedOut, $sw.Elapsed.TotalSeconds, $log)
if ($done -and -not $crashed -and $errorCount -eq 0 -and -not $timedOut) { exit 0 } else { exit 1 }
