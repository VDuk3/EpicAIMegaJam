# Usage: Make-HandPlates.ps1 <in_dir> <out_dir> [-Scale 4]
# For each 1920x1080 game screenshot: a plate of the whole screen at the hands' pixel scale (Scale screen pixels per art
# pixel: 4 -> 480x270, the scale of the approved first-person staff hand), area-averaged, to trace the pixel-art hands
# 1:1 in Pixelorama over the 3D gun.
param([string]$InDir, [string]$OutDir, [int]$Scale = 4)
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null
$w = [int][Math]::Floor(1920 / $Scale); $h = [int][Math]::Floor(1080 / $Scale)
foreach ($f in Get-ChildItem $InDir -Filter *.png) {
    $src = [System.Drawing.Bitmap]::FromFile($f.FullName)
    $plate = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($plate)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    # w art px cover w*Scale screen px: any leftover columns on the right are cropped.
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), (New-Object System.Drawing.Rectangle 0, 0, ($w * $Scale), ($h * $Scale)), [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()
    $base = [IO.Path]::GetFileNameWithoutExtension($f.Name)
    $plate.Save((Join-Path $OutDir "$($base)_plate$($w)x$($h).png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $plate.Dispose(); $src.Dispose()
    Write-Host "plate $base ($($w)x$($h))"
}
