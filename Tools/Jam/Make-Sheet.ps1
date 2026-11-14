# Make-Sheet.ps1 - tiles named PNG files into one sheet without labels (review of renders at a readable size).
#   powershell -File Tools/Jam/Make-Sheet.ps1 -Dir <folder> -Names a,b,c,d -Out <file.png> [-Cols 2] [-W 800] [-H 500]
param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [Parameter(Mandatory = $true)][string[]]$Names,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Cols = 2,
    [int]$W = 800,
    [int]$H = 500
)
Add-Type -AssemblyName System.Drawing
$Names = @($Names | ForEach-Object { $_ -split "," })
$rows = [math]::Ceiling($Names.Count / $Cols)
$bmp = New-Object System.Drawing.Bitmap ($Cols * $W), ($rows * $H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$font = New-Object System.Drawing.Font 'Consolas', 11
$i = 0
foreach ($n in $Names) {
    $p = Join-Path $Dir ($n + '.png')
    if (Test-Path $p) {
        $img = [System.Drawing.Image]::FromFile($p)
        $x = ($i % $Cols) * $W; $y = [math]::Floor($i / $Cols) * $H
        $g.DrawImage($img, $x, $y, $W, $H)
        $img.Dispose()
        $g.DrawString($n, $font, [System.Drawing.Brushes]::Yellow, $x + 4, $y + 4)
    }
    $i++
}
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output $Out
