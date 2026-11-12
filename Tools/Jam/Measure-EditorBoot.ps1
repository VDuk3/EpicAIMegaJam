<#
Measure-EditorBoot.ps1 - Boots the Editor N times (Start-EditorMcp / Stop-EditorMcp) and reports
"Total Editor Startup Time" from the log plus the number of mounted plugins.
Timings vary from run to run: report the range, not the best run (Devlog/PREJAM_PREP.md).

Usage: powershell -File Tools/Jam/Measure-EditorBoot.ps1 -Label baseline -Runs 2
Appends one line per run to Glyphbound/Saved/Logs/boot_measurements.csv
#>
param(
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject'),
    [string]$Label = 'run',
    [int]$Runs = 2,
    [int]$Port = 8059
)
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$name = [IO.Path]::GetFileNameWithoutExtension($Project)
$log = Join-Path $projDir "Saved\Logs\$name.log"
$csv = Join-Path $projDir 'Saved\Logs\boot_measurements.csv'
if (-not (Test-Path $csv)) { 'utc,label,run,startup_seconds,mounted_plugins,mcp_ready' | Set-Content -Path $csv -Encoding ascii }
for ($i = 1; $i -le $Runs; $i++) {
    $out = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Start-EditorMcp.ps1') -Project $Project -Port $Port
    $startCode = $LASTEXITCODE
    $ready = ($out | Select-String 'MCP listo en (\d+) s' | Select-Object -Last 1)
    $readyS = if ($ready) { $ready.Matches[0].Groups[1].Value } else { '' }
    # The startup line is written a moment after the MCP server starts: wait for it.
    $secs = ''
    for ($w = 0; $w -lt 30 -and -not $secs; $w++) {
        $m = Select-String -Path $log -Pattern 'Total Editor Startup Time, took ([\d\.]+)' | Select-Object -Last 1
        if ($m) { $secs = $m.Matches[0].Groups[1].Value } else { Start-Sleep 2 }
    }
    $mounted = @(Select-String -Path $log -Pattern 'Mounting (Engine|Project) plugin').Count
    Write-Host "[$Label] run ${i}: startup=$secs s, mounted plugins=$mounted, MCP ready=$readyS s (start exit $startCode)"
    ('{0},{1},{2},{3},{4},{5}' -f [DateTime]::UtcNow.ToString('o'), $Label, $i, $secs, $mounted, $readyS) | Add-Content -Path $csv -Encoding ascii
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Stop-EditorMcp.ps1') -Port $Port | Select-Object -Last 2
    Start-Sleep 3
}
