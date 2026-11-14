# Usage: Crop-Image.ps1 <in.png> <out.png> <x> <y> <w> <h> [-Grid 12]
# Crops a region of a screenshot; with -Grid N draws a faint line every N pixels (the art-pixel grid of the hands, 12 at 1080p for 3x)
# and a stronger one every 10 cells, to place pixel-art hands over a 3D gun template.
param([string]$In, [string]$Out, [int]$X, [int]$Y, [int]$W, [int]$H, [int]$Grid = 0)
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $In).Path)
$dst = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), (New-Object System.Drawing.Rectangle $X, $Y, $W, $H), [System.Drawing.GraphicsUnit]::Pixel)
if ($Grid -gt 0) {
    $thin = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(60, 255, 0, 255)), 1
    $thick = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(160, 255, 0, 255)), 1
    for ($gx = [Math]::Ceiling($X / $Grid) * $Grid; $gx -lt $X + $W; $gx += $Grid) {
        $pen = if ((($gx / $Grid) % 10) -eq 0) { $thick } else { $thin }
        $g.DrawLine($pen, $gx - $X, 0, $gx - $X, $H)
    }
    for ($gy = [Math]::Ceiling($Y / $Grid) * $Grid; $gy -lt $Y + $H; $gy += $Grid) {
        $pen = if ((($gy / $Grid) % 10) -eq 0) { $thick } else { $thin }
        $g.DrawLine($pen, 0, $gy - $Y, $W, $gy - $Y)
    }
}
$g.Dispose()
$dst.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$dst.Dispose(); $src.Dispose()
