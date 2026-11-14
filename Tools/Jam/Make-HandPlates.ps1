# Usage: Make-HandPlates.ps1 <in_dir> <out_dir>
# For each 1920x1080 game screenshot: a 213x120 plate (1 art pixel = 9 screen pixels, the hands' scale in AGBHUD, ClipY/120),
# area-averaged, to trace the pixel-art hands 1:1 in Pixelorama over the 3D gun.
param([string]$InDir, [string]$OutDir)
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null
foreach ($f in Get-ChildItem $InDir -Filter *.png) {
    $src = [System.Drawing.Bitmap]::FromFile($f.FullName)
    $w = 213; $h = 120
    $plate = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($plate)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    # 213 art px cover 1917 screen px: crop the last 3 columns.
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), (New-Object System.Drawing.Rectangle 0, 0, 1917, 1080), [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()
    $base = [IO.Path]::GetFileNameWithoutExtension($f.Name)
    $plate.Save((Join-Path $OutDir "$($base)_plate213x120.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $plate.Dispose(); $src.Dispose()
    Write-Host "plate $base"
}
