<#
Build-Jam.ps1 - Compiles the Glyphbound C++ module (Editor target by default) and prints only what matters.
One build at a time per machine: do not run this while another build, a package or a test run is going.
The Editor must be closed (a full build cannot link while the Editor holds the DLLs).

Usage:
  powershell -File Tools/Jam/Build-Jam.ps1                 # GlyphboundEditor Win64 Development
  powershell -File Tools/Jam/Build-Jam.ps1 -Target Glyphbound   # game target

Output: full log in Glyphbound/Saved/Logs/build_last.log; the console shows errors (first 60) and the result line.
Exit code: 0 = Result: Succeeded.
#>
param(
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject'),
    [string]$Target = 'GlyphboundEditor',
    [string]$Config = 'Development',
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [int]$MaxErrors = 60
)
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$logDir = Join-Path $projDir 'Saved\Logs'
[void][IO.Directory]::CreateDirectory($logDir)
$log = Join-Path $logDir 'build_last.log'
$bat = Join-Path $EngineDir 'Engine\Build\BatchFiles\Build.bat'
$sw = [Diagnostics.Stopwatch]::StartNew()
& cmd.exe /d /c "`"$bat`" $Target Win64 $Config -project=`"$Project`" -waitmutex > `"$log`" 2>&1"
$code = $LASTEXITCODE
$sw.Stop()
$errors = Select-String -Path $log -Pattern '(: error |: fatal error |error LNK|Error: |ERROR: |UHT.*[Ee]rror)' | Select-Object -First $MaxErrors
if ($errors) { $errors | ForEach-Object { $_.Line.Trim() } }
$result = Select-String -Path $log -Pattern '^Result: ' | Select-Object -Last 1
if ($result) { Write-Host $result.Line } else { Write-Host "No 'Result:' line found (exit $code). See $log" }
Write-Host ("Build time: {0:N0} s  (exit {1})  log: {2}" -f $sw.Elapsed.TotalSeconds, $code, $log)
exit $code
