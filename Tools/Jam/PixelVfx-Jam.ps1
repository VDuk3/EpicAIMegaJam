<#
PixelVfx-Jam.ps1 - Everything the GBPixelVfx plugin needs from the machine, in one script:
build, pure-logic tests, the sprite material, baking Niagara effects to pixel-art flipbooks, the showcase run.

Why its own script: while the plugin is being written it is NOT enabled in Glyphbound.uproject
("EnabledByDefault": false in the .uplugin), so another chat's builds never compile a half-written plugin.
This script turns it on from the command line (UBT -EnablePlugin=, runtime -EnablePlugins=). NiagaraFluids is
only ever enabled here, on the bake command line: the packaged game does not carry it.

Machine rule: one build or run at a time. Every action takes the machine lock
(Glyphbound/Saved/machine.lock: who, what, UTC time) and removes it at the end. If someone else holds a lock
younger than 45 minutes, the script waits (up to -LockWaitMinutes) instead of running on top of it.

Usage (from the repo root):
  powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Build
  powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Test
  powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Materials
  powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Bake -Id FluidFire      # or -Id All
  powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Compare -Id FluidFire   # source vs baked: frame time + screenshot
  powershell -File Tools/Jam/PixelVfx-Jam.ps1 -Action Showcase                # every baked clip on a small lit stage
  Add -HardwareRT to Compare / Showcase to run that process with hardware ray tracing on (project config untouched).
  Add -Window to see the run instead of rendering offscreen.
Exit code: 0 = the action reported success (build Succeeded, tests green, "PVFX DONE" with no errors).
#>
param(
    [Parameter(Mandatory = $true)][ValidateSet('Build', 'Test', 'Materials', 'Bake', 'Compare', 'Showcase')][string]$Action,
    [string]$Id = 'All',
    [switch]$HardwareRT,
    [switch]$Window,
    [string]$Owner = 'pixelvfx',
    [int]$LockWaitMinutes = 40,
    [int]$TimeoutSeconds = 900,
    [string]$Project = '',
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9'
)
$ErrorActionPreference = 'Continue'
# $PSScriptRoot came out empty while this script's parameters were being bound (it has [Parameter()] attributes),
# so the default project path is resolved here instead of in the param block.
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $Project) { $Project = Join-Path $scriptDir '..\..\Glyphbound\Glyphbound.uproject' }
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$logDir = Join-Path $projDir 'Saved\Logs'
[void][IO.Directory]::CreateDirectory($logDir)
$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$lock = Join-Path $projDir 'Saved\machine.lock'
$pluginDir = Join-Path $projDir 'Plugins\GBPixelVfx'
$editor = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor.exe'
$editorCmd = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

function Wait-MachineLock {
    $deadline = [DateTime]::UtcNow.AddMinutes($LockWaitMinutes)
    while ($true) {
        $busy = $null
        if (Test-Path $lock) {
            $age = ([DateTime]::UtcNow - (Get-Item $lock).LastWriteTimeUtc).TotalMinutes
            if ($age -lt 45) { $busy = "lock held: " + ((Get-Content $lock -Raw) -replace '\s+', ' ') }
        }
        if (-not $busy) {
            # The interactive Editor holds the module DLLs and the GPU: never run on top of it.
            $ed = @(Get-Process -Name 'UnrealEditor', 'UnrealEditor-Cmd' -ErrorAction SilentlyContinue)
            if ($ed.Count -gt 0) { $busy = "an Unreal process is running (PID $($ed[0].Id))" }
        }
        if (-not $busy) { break }
        if ([DateTime]::UtcNow -gt $deadline) { Write-Host "LOCK TIMEOUT after $LockWaitMinutes min: $busy"; exit 5 }
        Write-Host "waiting for the machine: $busy"
        Start-Sleep 15
    }
    [IO.File]::WriteAllText($lock, ("{0} | {1} {2} | {3}" -f $Owner, $Action, $Id, [DateTime]::UtcNow.ToString('o')))
}
function Remove-MachineLock {
    if (Test-Path $lock) {
        $text = Get-Content $lock -Raw
        if ($text -like "$Owner |*") { Remove-Item $lock -Force -ErrorAction SilentlyContinue }
    }
}
function Clean-Line($line) { ($line -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }

# Runs a game-mode process of the plugin (bake / compare / showcase) and reads its log.
function Invoke-PixelVfxRun([string]$modeArg, [int]$resX, [int]$resY) {
    $log = Join-Path $logDir "pixelvfx_$($Action.ToLower())_$stamp.log"
    $map = '/Engine/Maps/Entry?game=/Script/GBPixelVfxBaker.GBPixelVfxBakeGameMode'
    $argList = @("`"$Project`"", $map, '-game', "-abslog=`"$log`"", '-windowed', "-ResX=$resX", "-ResY=$resY",
        '-unattended', '-nosplash', '-nosound', '-nocrashreports', '-EnablePlugins=GBPixelVfx,NiagaraFluids', $modeArg)
    if (-not $Window) { $argList += '-RenderOffscreen' }
    # Bakes count simulation frames: the fixed step must be in place before the first frame (BeginPlay is too late for it).
    if ($Action -eq 'Bake') { $argList += @('-UseFixedTimeStep', '-FPS=60') }
    if ($HardwareRT) {
        # r.RayTracing is read once at startup: it has to come from the ini layer, not from a console command.
        $argList += @('-dx12', '-ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=1,r.Lumen.HardwareRayTracing=1')
    }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = Start-Process -FilePath $editor -ArgumentList $argList -PassThru
    $timedOut = -not $p.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        # A game process started by this script, not the Editor: stopping it is safe.
        Write-Host "TIMEOUT after $TimeoutSeconds s: stopping the process."
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep 2
    }
    $sw.Stop()
    if (-not (Test-Path $log)) { Write-Host "No log at $log (exit $($p.ExitCode))"; return 2 }
    $lines = Get-Content $log -Encoding UTF8
    $ours = @($lines | Where-Object { $_ -match 'PVFX' })
    $bad = @($lines | Where-Object { $_ -match 'LogGBPixelVfx: (Error|Warning)|Fatal error|Unhandled Exception|Assertion failed|Ensure condition failed|appError' })
    $ours | Select-Object -First 200 | ForEach-Object { Clean-Line $_ }
    Write-Host '---- problems (LogGBPixelVfx warnings/errors, asserts, crashes) ----'
    if ($bad.Count -eq 0) { Write-Host '(none)' } else { $bad | Select-Object -First 40 | ForEach-Object { Clean-Line $_ } }
    $done = @($ours | Where-Object { $_ -match 'PVFX DONE' }).Count -gt 0
    $crashed = @($bad | Where-Object { $_ -match 'Fatal error|Unhandled Exception|Assertion failed|appError' }).Count -gt 0
    $errorCount = @($bad | Where-Object { $_ -match 'LogGBPixelVfx: Error' }).Count
    Write-Host ("PixelVfx {0}: done={1} crashed={2} errors={3} timedOut={4} wall={5:N0} s  log: {6}" -f $Action, $done, $crashed, $errorCount, $timedOut, $sw.Elapsed.TotalSeconds, $log)
    if ($done -and -not $crashed -and $errorCount -eq 0 -and -not $timedOut) { return 0 } else { return 1 }
}

$code = 1
Wait-MachineLock
try {
    switch ($Action) {
        'Build' {
            $log = Join-Path $logDir 'build_pixelvfx.log'
            $bat = Join-Path $EngineDir 'Engine\Build\BatchFiles\Build.bat'
            $sw = [Diagnostics.Stopwatch]::StartNew()
            & cmd.exe /d /c "`"$bat`" GlyphboundEditor Win64 Development -project=`"$Project`" -waitmutex -EnablePlugin=GBPixelVfx > `"$log`" 2>&1"
            $code = $LASTEXITCODE
            $sw.Stop()
            $errors = Select-String -Path $log -Pattern '(: error |: fatal error |error LNK|Error: |ERROR: |UHT.*[Ee]rror)' | Select-Object -First 60
            if ($errors) { $errors | ForEach-Object { $_.Line.Trim() } }
            $result = Select-String -Path $log -Pattern '^Result: ' | Select-Object -Last 1
            if ($result) { Write-Host $result.Line } else { Write-Host "No 'Result:' line found (exit $code). See $log" }
            Write-Host ("Build time: {0:N0} s  (exit {1})  log: {2}" -f $sw.Elapsed.TotalSeconds, $code, $log)
        }
        'Test' {
            $runDir = Join-Path $projDir "Saved\TestRuns\pixelvfx_$stamp"
            [void][IO.Directory]::CreateDirectory($runDir)
            $log = Join-Path $runDir 'editor.log'
            $argList = @("`"$Project`"", '-ExecCmds="Automation RunTests ^GBPixelVfx.; Quit"', '-TestExit="Automation Test Queue Empty"',
                "-ReportExportPath=`"$runDir`"", "-abslog=`"$log`"", '-EnablePlugins=GBPixelVfx',
                '-unattended', '-nopause', '-nosplash', '-NullRHI', '-nosound', '-nocrashreports')
            $sw = [Diagnostics.Stopwatch]::StartNew()
            $p = Start-Process -FilePath $editorCmd -ArgumentList $argList -PassThru -WindowStyle Hidden
            if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
                Write-Host "TIMEOUT after $TimeoutSeconds s: stopping the test process."
                Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
                $code = 3
            } else {
                $sw.Stop()
                $index = Join-Path $runDir 'index.json'
                if (-not (Test-Path $index)) {
                    Write-Host "FAIL: no report was written (exit $($p.ExitCode)). Did the module load? Log: $log"
                    Select-String -Path $log -Pattern 'Error:|error:|No automation tests matched' -ErrorAction SilentlyContinue | Select-Object -First 15 | ForEach-Object { $_.Line }
                    $code = 2
                } else {
                    $report = Get-Content $index -Raw -Encoding UTF8 | ConvertFrom-Json
                    $tests = @($report.tests)
                    $failed = @($tests | Where-Object { $_.state -ne 'Success' })
                    foreach ($t in $failed) {
                        Write-Host "FAILED: $($t.fullTestPath) [$($t.state)]"
                        foreach ($e in @($t.entries)) { if ($e.event.type -eq 'Error') { Write-Host "   $($e.event.message)" } }
                    }
                    Write-Host ("Tests: {0} run, {1} failed, wall time {2:N0} s. Report: {3}" -f $tests.Count, $failed.Count, $sw.Elapsed.TotalSeconds, $runDir)
                    if ($tests.Count -lt 1) { Write-Host 'FAIL: no test ran.'; $code = 4 }
                    elseif ($failed.Count -gt 0) { $code = 1 } else { $code = 0 }
                }
            }
        }
        'Materials' {
            $script = Join-Path $pluginDir 'Tools\make_pixelvfx_material.py'
            $log = Join-Path $logDir "pixelvfx_materials_$stamp.log"
            # Compiling a material needs a renderer: commandlets only start one with -AllowCommandletRendering.
            $argList = @("`"$Project`"", '-run=pythonscript', "-script=`"$script`"", "-abslog=`"$log`"",
                '-AllowCommandletRendering', '-unattended', '-nopause', '-nosplash', '-nosound', '-nocrashreports')
            $p = Start-Process -FilePath $editorCmd -ArgumentList $argList -PassThru -WindowStyle Hidden
            if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; Write-Host 'TIMEOUT'; $code = 3 }
            else {
                $lines = Get-Content $log -Encoding UTF8
                $ours = @($lines | Where-Object { $_ -match 'make_pixelvfx_material:' })
                $errors = @($lines | Where-Object { $_ -match 'LogPython: Error|make_pixelvfx_material: ERROR|Traceback|Fatal error|Assertion failed' })
                $ours | ForEach-Object { Clean-Line $_ }
                $errors | Select-Object -First 40 | ForEach-Object { Clean-Line $_ }
                $asset = Join-Path $projDir 'Content\Glyphbound\Materials\M_PixelVfxSprite.uasset'
                $ok = (@($ours | Where-Object { $_ -match 'make_pixelvfx_material: OK' }).Count -gt 0) -and (Test-Path $asset) -and ($errors.Count -eq 0)
                Write-Host ("Materials: ok={0} log: {1}" -f $ok, $log)
                $code = if ($ok) { 0 } else { 1 }
            }
        }
        'Bake' { $code = Invoke-PixelVfxRun "-PvfxBake=$Id" 640 360 }
        'Compare' { $code = Invoke-PixelVfxRun "-PvfxCompare=$Id" 1280 720 }
        'Showcase' { $code = Invoke-PixelVfxRun '-PvfxShowcase' 1280 720 }
    }
}
finally { Remove-MachineLock }
exit $code
