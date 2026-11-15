<#
Verify-Jam.ps1 - The end-of-block check in one command (run it in the background; one notice at the end).

  suite (Test-Jam) -> package (Package-Jam: also builds the Game target) -> packaged full smoke
  [-Visits: packaged FogVisit, CastleVisit and Enemies3DBase on their real maps]

Why this order and nothing more (measured 2026-11-15):
  - The packaged full smoke runs the same 559 steps as the uncooked one (about 465 s each) on the cooked
    content we ship, so the uncooked full run is not repeated here.
  - Package-Jam compiles the Game target itself: no separate Build-Jam -Target Glyphbound.
  - Fog, Valkyrie, Castle, Enemies3D... are inside the full run: no extra filtered packaged runs.
  - The map visits (about 9 min together) only when the work touched those maps, the fog or the AI: -Visits.
While iterating, keep using the filtered offscreen smoke on the uncooked game, down to a sub-block:
  powershell -File Tools/Jam/Smoke-Jam.ps1 -Only Enemies3D:tactics -Offscreen   (spaces in step names are ignored)

Usage:
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Jam/Verify-Jam.ps1 [-Visits] [-ZipName TeamTBD_Glyphbound]
Prints one line per stage and a final VERIFY GREEN / VERIFY RED. Exit code 0 only when everything is green.
#>
param(
    [switch]$Visits,
    [string]$ZipName = 'TeamTBD_Glyphbound',
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject')
)
$ErrorActionPreference = 'Continue'
$Project = (Resolve-Path $Project).Path
$smoke = Join-Path $PSScriptRoot 'Smoke-Jam.ps1'
$red = New-Object System.Collections.Generic.List[string]
$sw = [System.Diagnostics.Stopwatch]::StartNew()

function Stage([string]$Name, [string]$File, [string[]]$StageArgs, [string]$OkPattern, [string]$KeepPattern) {
    $t = [System.Diagnostics.Stopwatch]::StartNew()
    $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $File @StageArgs 2>&1 | ForEach-Object { "$_" }
    $keep = @($out | Where-Object { $_ -match $KeepPattern })
    $ok = (@($out | Where-Object { $_ -match $OkPattern }).Count -gt 0) -and (@($out | Where-Object { $_ -match 'FAILED' }).Count -eq 0)
    $mark = if ($ok) { 'ok ' } else { 'RED' }
    Write-Host ("[{0}] {1} ({2:N0} s)" -f $mark, $Name, $t.Elapsed.TotalSeconds)
    $keep | Select-Object -First 8 | ForEach-Object { Write-Host ("      " + $_.Substring(0, [Math]::Min(240, $_.Length))) }
    if (-not $ok) { $script:red.Add($Name) }
    return $ok
}

$smokeOk = 'Smoke: done=True crashed=False LogGB errors=0 timedOut=False'
$smokeKeep = 'FAILED|Smoke:'
[void](Stage 'suite' (Join-Path $PSScriptRoot 'Test-Jam.ps1') @() 'Tests: \d+ run, 0 failed' 'Tests:|FAILED|Fail')
if (Stage 'package' (Join-Path $PSScriptRoot 'Package-Jam.ps1') @('-Project', $Project, '-ZipName', $ZipName) 'BUILD SUCCESSFUL' 'ZIP:|BUILD |ERROR') {
    [void](Stage 'packaged full smoke' $smoke @('-Packaged', '-Offscreen') $smokeOk $smokeKeep)
    if ($Visits) {
        [void](Stage 'packaged FogVisit' $smoke @('-Packaged', '-Only', 'FogVisit', '-Map', '/Game/Glyphbound/Maps/FutureBase', '-Offscreen', '-TimeoutSeconds', '900') $smokeOk $smokeKeep)
        [void](Stage 'packaged CastleVisit' $smoke @('-Packaged', '-Only', 'CastleVisit', '-Map', '/Game/Glyphbound/Maps/PastCastle', '-Offscreen', '-TimeoutSeconds', '900') $smokeOk $smokeKeep)
        [void](Stage 'packaged Enemies3DBase' $smoke @('-Packaged', '-Only', 'Enemies3DBase', '-Map', '/Game/Glyphbound/Maps/FutureBase', '-Offscreen', '-TimeoutSeconds', '900') $smokeOk $smokeKeep)
    }
}
if ($red.Count -eq 0) {
    Write-Host ("VERIFY GREEN ({0:N0} s)" -f $sw.Elapsed.TotalSeconds)
    exit 0
}
Write-Host ("VERIFY RED: {0} ({1:N0} s)" -f ($red -join ', '), $sw.Elapsed.TotalSeconds)
exit 1
