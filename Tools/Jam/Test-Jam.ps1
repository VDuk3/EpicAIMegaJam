<#
Test-Jam.ps1 - Runs the pure-logic automation tests headless and decides pass/fail from the report.
Rules (Devlog/PREJAM_PREP.md, "Lessons from our previous project"):
  - anchored filters: start with ^ and end with a dot; join several with + (one Editor boot);
  - every run writes to its own new folder; the verdict comes from the report, not only the exit code;
  - a run with fewer tests than -MinTests is a failure (guards against a typo in the filter);
  - one build or test run at a time per machine; the Editor must be closed.

Usage:
  powershell -File Tools/Jam/Test-Jam.ps1                              # the whole suite: ^Glyphbound.
  powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.QTE.'   # only what was touched
  powershell -File Tools/Jam/Test-Jam.ps1 -Filter '^Glyphbound.QTE.+^Glyphbound.Spells.'
Exit code: 0 = every selected test passed and at least MinTests ran.
#>
param(
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject'),
    [string]$Filter = '^Glyphbound.',
    [int]$MinTests = 1,
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [int]$TimeoutSeconds = 600
)
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$runDir = Join-Path $projDir "Saved\TestRuns\$stamp"
[void][IO.Directory]::CreateDirectory($runDir)
$log = Join-Path $runDir 'editor.log'
$exe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$argList = @(
    "`"$Project`"",
    "-ExecCmds=`"Automation RunTests $Filter; Quit`"",
    '-TestExit="Automation Test Queue Empty"',
    "-ReportExportPath=`"$runDir`"",
    "-abslog=`"$log`"",
    '-unattended', '-nopause', '-nosplash', '-NullRHI', '-nosound', '-nocrashreports'
)
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
    Write-Host "TIMEOUT after $TimeoutSeconds s. The Editor process (PID $($p.Id)) is still running; log: $log"
    exit 3
}
$sw.Stop()
$index = Join-Path $runDir 'index.json'
if (-not (Test-Path $index)) {
    Write-Host "FAIL: no report was written (exit $($p.ExitCode)). Did the module load? Log: $log"
    Select-String -Path $log -Pattern 'Error:|error:|No automation tests matched' -ErrorAction SilentlyContinue | Select-Object -First 15 | ForEach-Object { $_.Line }
    exit 2
}
$report = Get-Content $index -Raw -Encoding UTF8 | ConvertFrom-Json
$tests = @($report.tests)
$failed = @($tests | Where-Object { $_.state -ne 'Success' })
$testSeconds = 0.0
foreach ($t in $tests) { $testSeconds += [double]$t.duration }
foreach ($t in $failed) {
    Write-Host "FAILED: $($t.fullTestPath) [$($t.state)]"
    foreach ($e in @($t.entries)) { if ($e.event.type -eq 'Error') { Write-Host "   $($e.event.message)" } }
}
$ensures = @(Select-String -Path $log -Pattern 'Ensure condition failed' -ErrorAction SilentlyContinue).Count
Write-Host ("Tests: {0} run, {1} failed, test time {2:N2} s, wall time {3:N0} s, ensures in log: {4}" -f $tests.Count, $failed.Count, $testSeconds, $sw.Elapsed.TotalSeconds, $ensures)
Write-Host "Report: $runDir"
if ($tests.Count -lt $MinTests) { Write-Host "FAIL: only $($tests.Count) tests ran, expected at least $MinTests (check the filter)."; exit 4 }
if ($failed.Count -gt 0) { exit 1 }
exit 0
