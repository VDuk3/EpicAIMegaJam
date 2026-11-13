<#
Import-Entity.ps1 - Builds the Entity (final boss, phase 1) in Unreal from its one source file,
Glyphbound/SourceArt/Enemies/Entity/Entity.blend: Blender (headless) splits and exports the FBX parts and the textures to a
temporary folder (Tools/Blender/entity_export_fbx.py), the Editor's Python commandlet (Tools/Jam/import_entity.py) imports
them into /Game/Glyphbound/Enemies/Entity with their materials, and the temporary folder is deleted.
Content is not in git: run this on CENTRAL after Entity.blend or the scripts change. The Editor must be closed.

Usage:
  powershell -File Tools/Jam/Import-Entity.ps1
Output: the script's lines, the error lines of the log, OK or FAILED. Exit code 0 = imported and saved.
#>
param(
    [string]$Project = (Join-Path $PSScriptRoot '..\..\Glyphbound\Glyphbound.uproject'),
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [string]$Blender = 'C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe',
    [int]$TimeoutSeconds = 900,
    # Import into /Game/Glyphbound/Enemies/Entity_Preview instead, for the owner's review before the real import.
    [switch]$Preview
)
$Project = (Resolve-Path $Project).Path
$projDir = Split-Path $Project -Parent
$script = (Resolve-Path (Join-Path $PSScriptRoot 'import_entity.py')).Path
$exe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path $exe)) { Write-Host "No editor at $exe"; exit 2 }

$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$logDir = Join-Path $projDir 'Saved\Logs'
New-Item -ItemType Directory -Force $logDir | Out-Null
$log = Join-Path $logDir "import_entity_$stamp.log"

# Materials compile here, so the commandlet needs a renderer (no -NullRHI).
$argList = @("`"$Project`"", '-run=pythonscript', "-script=`"$script`"", "-abslog=`"$log`"",
    '-AllowCommandletRendering', '-unattended', '-nopause', '-nosplash', '-nosound', '-nocrashreports')

$sw = [Diagnostics.Stopwatch]::StartNew()

# 1. From the one source file (Entity.blend), export the FBX parts and textures into a temporary folder.
$source = Join-Path $projDir 'SourceArt\Enemies\Entity'
$blend = Join-Path $source 'Entity.blend'
$blenderTools = (Resolve-Path (Join-Path $PSScriptRoot '..\Blender')).Path
if (-not (Test-Path $Blender)) { Write-Host "No Blender at $Blender"; exit 2 }
if (-not (Test-Path $blend)) { Write-Host "No source at $blend"; exit 2 }
$stage = Join-Path $projDir "Saved\EntityStage_$stamp"
New-Item -ItemType Directory -Force $stage | Out-Null
$out = & $Blender -b $blend --python (Join-Path $blenderTools 'entity_export_fbx.py') -- $stage 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $out -notmatch 'REPORT' -or $out -match 'Traceback') {
    Write-Host 'Blender step entity_export_fbx.py failed:'; Write-Host $out; Remove-Item -Recurse -Force $stage; exit 1
}
($out -split "`n") | Where-Object { $_ -match '^(SPLIT|EYE|PART)' } | ForEach-Object { Write-Host "Blender: $($_.Trim())" }
if ($Preview) { $env:GB_ENTITY_DIR = '/Game/Glyphbound/Enemies/Entity_Preview' }
else {
    $env:GB_ENTITY_DIR = '/Game/Glyphbound/Enemies/Entity'
    Copy-Item (Join-Path $stage 'export_report.json') (Join-Path $source 'export_report.json') -Force
}

# 2. Import into Unreal from the temporary folder.
$env:GB_ENTITY_STAGE = $stage
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
$timedOut = -not $p.WaitForExit($TimeoutSeconds * 1000)
if ($timedOut) {
    Write-Host "TIMEOUT after $TimeoutSeconds s: stopping the commandlet."
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
}
$sw.Stop()
# 3. The temporary files go away: Entity.blend and the Unreal assets are the only copies.
Remove-Item -Recurse -Force $stage

if (-not (Test-Path $log)) { Write-Host "No log at $log (exit $($p.ExitCode))"; exit 2 }
$lines = Get-Content $log -Encoding UTF8
$ours = @($lines | Where-Object { $_ -match 'import_entity:' })
$errors = @($lines | Where-Object { $_ -match 'LogPython: Error|import_entity: ERROR|Traceback|Fatal error|Assertion failed|Unhandled Exception|LogFbx: Error' })
$ok = @($ours | Where-Object { $_ -match 'import_entity: OK' }).Count -gt 0

$ours | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
if ($errors.Count -gt 0) {
    Write-Host '---- errors ----'
    $errors | Select-Object -First 60 | ForEach-Object { ($_ -replace '^\[[^\]]*\]\[[^\]]*\]', '').Trim() }
}
Write-Host ("Import: ok={0} errors={1} exit={2} wall={3:N0} s  log: {4}" -f $ok, $errors.Count, $p.ExitCode, $sw.Elapsed.TotalSeconds, $log)
if ($ok -and $errors.Count -eq 0 -and -not $timedOut) { Write-Host 'OK'; exit 0 }
Write-Host 'FAILED'
exit 1
