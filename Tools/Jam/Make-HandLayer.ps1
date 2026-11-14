# Usage: Make-HandLayer.ps1 <spec.json> [-Template <1920x1080 screenshot>] [-PreviewOut <png>] [-CropX ... ]
# Builds a first-person hands layer from character-map sprites (the format of Sprites.json: one char per art pixel,
# '.' transparent, a palette letter otherwise). Art canvas 160x90 (3x scale: one art pixel = 3 file pixels = ClipY/90 screen
# pixels), written as a 480x270 PNG (pivot [0, 0]) that gb.HandOverlay and the game draw full screen.
# spec.json: { "out": "path.png", "palette": { "K": "#000000", ... },
#              "sprites": [ { "x": 98, "y": 70, "rows": [ "...KK", ... ] } ] }   (x, y = art pixel of the map's top left)
# With -Template, also writes a preview: the template with the layer drawn over it at 12 screen px per art pixel
# (1080p), cropped to CropX/CropY/CropW/CropH and scaled by -Zoom.
param([string]$Spec, [string]$Template = '', [string]$PreviewOut = '',
    [int]$CropX = 0, [int]$CropY = 0, [int]$CropW = 1920, [int]$CropH = 1080, [double]$Zoom = 1.0)
Add-Type -AssemblyName System.Drawing
$s = Get-Content $Spec -Raw -Encoding UTF8 | ConvertFrom-Json
$pal = @{}
foreach ($p in $s.palette.PSObject.Properties) { $pal[$p.Name] = [System.Drawing.ColorTranslator]::FromHtml($p.Value) }
$art = New-Object System.Drawing.Bitmap 160, 90, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$bad = 0
$specDir = Split-Path (Resolve-Path $Spec).Path -Parent
foreach ($sp in $s.sprites) {
    # "rowsFile": the map in a text file next to the spec (one row per line), easier to edit than JSON strings.
    if ($sp.PSObject.Properties['rowsFile']) {
        $rows = @(Get-Content (Join-Path $specDir $sp.rowsFile) -Encoding UTF8 | Where-Object { $_.Length -gt 0 })
        $sp | Add-Member -NotePropertyName rows -NotePropertyValue $rows -Force
    }
    $w0 =([string]$sp.rows[0]).Length
    for ($r = 0; $r -lt $sp.rows.Count; $r++) { if (([string]$sp.rows[$r]).Length -ne $w0) { Write-Host "row $r is $(([string]$sp.rows[$r]).Length) wide, row 0 is $w0"; $bad++ } }
    for ($r = 0; $r -lt $sp.rows.Count; $r++) {
        $row = [string]$sp.rows[$r]
        for ($c = 0; $c -lt $row.Length; $c++) {
            $ch = [string]$row[$c]
            if ($ch -eq '.') { continue }
            if (-not $pal.ContainsKey($ch)) { Write-Host "unknown palette char '$ch' at row $r col $c"; $bad++; continue }
            $ax = [int]$sp.x + $c; $ay = [int]$sp.y + $r
            if ($ax -ge 0 -and $ax -lt 160 -and $ay -ge 0 -and $ay -lt 90) { $art.SetPixel($ax, $ay, $pal[$ch]) }
        }
    }
}
if ($bad -gt 0) { exit 1 }
# 480x270 file: each art pixel is a 3x3 block.
$file = New-Object System.Drawing.Bitmap 480, 270, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for ($y = 0; $y -lt 90; $y++) { for ($x = 0; $x -lt 160; $x++) {
    $c = $art.GetPixel($x, $y)
    if ($c.A -eq 0) { continue }
    for ($dy = 0; $dy -lt 3; $dy++) { for ($dx = 0; $dx -lt 3; $dx++) { $file.SetPixel($x * 3 + $dx, $y * 3 + $dy, $c) } }
} }
$outDir = Split-Path $s.out -Parent
if ($outDir) { New-Item -ItemType Directory -Force $outDir | Out-Null }
$file.Save($s.out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "layer $($s.out)"
if ($Template -and $PreviewOut) {
    $tpl = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Template).Path)
    $g = [System.Drawing.Graphics]::FromImage($tpl)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $g.DrawImage($art, (New-Object System.Drawing.Rectangle 0, 0, 1920, 1080))
    $g.Dispose()
    $pw = [int]($CropW * $Zoom); $ph = [int]($CropH * $Zoom)
    $prev = New-Object System.Drawing.Bitmap $pw, $ph
    $g2 = [System.Drawing.Graphics]::FromImage($prev)
    $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g2.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $g2.DrawImage($tpl, (New-Object System.Drawing.Rectangle 0, 0, $pw, $ph), (New-Object System.Drawing.Rectangle $CropX, $CropY, $CropW, $CropH), [System.Drawing.GraphicsUnit]::Pixel)
    $g2.Dispose()
    $prev.Save($PreviewOut, [System.Drawing.Imaging.ImageFormat]::Png)
    $prev.Dispose(); $tpl.Dispose()
    Write-Host "preview $PreviewOut"
}
$file.Dispose(); $art.Dispose()
