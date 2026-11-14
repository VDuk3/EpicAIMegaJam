<#
Restore-CastlePack.ps1 - brings the castle part of the Medieval Kingdom pack (Hivemind, Fab) back from the Windows
Recycle Bin into Glyphbound/Content/CastleTown, where it was before 2026-11-13 (the first world plan kept 31 meshes
and sent the other 1,697 files to the Recycle Bin with the town).

It reads the Recycle Bin's own records ($I files: original path, size, deletion time) and moves each matching $R
item back to its original path. Nothing is overwritten: an item whose original path exists again is skipped.

  -List      only print what would be restored (no change)
  -All       restore every CastleTown item, town included (default: the castle part only, see $Exclude)

Run with the Editor closed.
#>
param(
    [switch]$List,
    [switch]$All,
    [string]$ContentDir = 'C:\GameJam\Glyphbound\Content'
)
$ErrorActionPreference = 'Stop'

# What the castle does not need: the town's houses and docks, the landscape, the water system, the scanned
# foliage, the demo mannequins. Matched against the path below Content\CastleTown\.
$Exclude = @(
    '^Water\\', '^Scanned_Foliage\\', '^Demo\\', '^Cinematics\\', '^SkySphere\\',
    '^Levels\\LevelInstances\\LI_(Building|LogStore)', '^Levels\\Persistant\\',
    '^Levels\\SubLevels\\(SL_BridgesAndWalls|SL_Houses|SL_Landscape|SL_Mountain|SL_Roads|SL_Town_Props|bloking_)',
    '^Static_Mesh\\(Village|DockWalls|Gangways|Hills|Mountains)\\',
    '^Textures\\(Landscape|Temp\\Landscape|Tile\\Land|Unique\\Land|Unique\\Mountain)\\',
    '^Materials\\LandscapeLayerInfo\\'
)

$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$bin = Join-Path ($ContentDir.Substring(0, 3)) ('$Recycle.Bin\' + $sid)
$castle = (Join-Path $ContentDir 'CastleTown') + '\'
$wholeFolders = @((Join-Path $ContentDir '__ExternalActors__\CastleTown'), (Join-Path $ContentDir '__ExternalObjects__\CastleTown'))

$restored = 0; $skipped = 0; $excluded = 0; $bytes = 0
foreach ($i in Get-ChildItem -LiteralPath $bin -Force -Filter '$I*') {
    $raw = [IO.File]::ReadAllBytes($i.FullName)
    if ($raw.Length -lt 30 -or [BitConverter]::ToInt64($raw, 0) -ne 2) { continue }
    $chars = [BitConverter]::ToInt32($raw, 24)
    $orig = [Text.Encoding]::Unicode.GetString($raw, 28, ($chars - 1) * 2)
    $isFile = $orig.StartsWith($castle, [StringComparison]::OrdinalIgnoreCase)
    $isFolder = $wholeFolders -contains $orig
    if (-not ($isFile -or $isFolder)) { continue }
    if ($isFile -and -not $All) {
        $rel = $orig.Substring($castle.Length)
        if ($Exclude | Where-Object { $rel -match $_ }) { $excluded++; continue }
    }
    $r = Join-Path $bin ('$R' + $i.Name.Substring(2))
    if (-not (Test-Path -LiteralPath $r)) { continue }
    if (Test-Path -LiteralPath $orig) { $skipped++; Write-Host "SKIP (exists): $orig"; continue }
    $bytes += [BitConverter]::ToInt64($raw, 8)
    if (-not $List) {
        $parent = Split-Path $orig -Parent
        if (-not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
        Move-Item -LiteralPath $r -Destination $orig
        Remove-Item -LiteralPath $i.FullName -Force
    }
    $restored++
}
$verb = if ($List) { 'would restore' } else { 'restored' }
Write-Host ("{0} {1} items ({2:N0} MB); left in the bin: {3} excluded; skipped: {4}" -f $verb, $restored, ($bytes / 1MB), $excluded, $skipped)
