# Make-ScreenTexture.ps1 - draws the picture shown on the wall screens of the long corridor (future base).
#   powershell -File Tools/Jam/Make-ScreenTexture.ps1
# The Spaceship Corridor model ships a stock infographic with a third party's watermark on it; the jam allows no
# foreign brands, so the screens show our own picture instead: the status board of the containment seal.
# Everything is drawn here with System.Drawing (no source image); the text is in English.
param([string]$Out = 'C:\GameJam\Glyphbound\SourceArt\Environment\ShipCorridor\T_ShipCorridor_Screen.png')
Add-Type -AssemblyName System.Drawing
$S = 1024
$bmp = New-Object System.Drawing.Bitmap $S, $S
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.TextRenderingHint = 'AntiAliasGridFit'

function Col($r, $gr, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $gr, $b) }
function Pen($c, $w) { New-Object System.Drawing.Pen $c, $w }
function Brush($c) { New-Object System.Drawing.SolidBrush $c }
function Font($size, $bold = $false) {
    $style = [System.Drawing.FontStyle]::Regular
    if ($bold) { $style = [System.Drawing.FontStyle]::Bold }
    New-Object System.Drawing.Font 'Consolas', $size, $style, ([System.Drawing.GraphicsUnit]::Pixel)
}

$cyan = Col 120 225 255
$dim = Col 60 140 210
$faint = Col 40 90 170 110
$white = Col 225 245 255
$amber = Col 255 180 70
$red = Col 255 80 70

# background: deep blue, lighter toward the middle
$rect = New-Object System.Drawing.Rectangle 0, 0, $S, $S
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, (Col 3 8 34), (Col 8 30 96), 90.0
$g.FillRectangle($bg, $rect)
for ($i = 0; $i -lt $S; $i += 64) {
    $g.DrawLine((Pen $faint 1), $i, 0, $i, $S)
    $g.DrawLine((Pen $faint 1), 0, $i, $S, $i)
}

# frame with corner brackets
$g.DrawRectangle((Pen $dim 3), 28, 28, $S - 56, $S - 56)
foreach ($c in @(@(28, 28, 1, 1), @(996, 28, -1, 1), @(28, 996, 1, -1), @(996, 996, -1, -1))) {
    $g.DrawLine((Pen $cyan 8), $c[0], $c[1], $c[0] + 70 * $c[2], $c[1])
    $g.DrawLine((Pen $cyan 8), $c[0], $c[1], $c[0], $c[1] + 70 * $c[3])
}

# header
$g.DrawString('CONTAINMENT STATUS', (Font 64 $true), (Brush $white), 60, 56)
$g.DrawString('SUBLEVEL 3 // SERVICE CORRIDOR', (Font 30), (Brush $cyan), 64, 132)
$g.DrawLine((Pen $cyan 4), 60, 180, 964, 180)

# left panel: five bars
$g.DrawRectangle((Pen $dim 2), 60, 210, 420, 380)
$labels = 'AIR', 'PWR', 'COOL', 'SEAL', 'LOCK'
$values = 0.92, 0.74, 0.58, 0.87, 0.31
for ($i = 0; $i -lt 5; $i++) {
    $x = 86 + $i * 78
    $h = [int](280 * $values[$i])
    $g.DrawRectangle((Pen $dim 2), $x, 240, 50, 280)
    $c = $cyan
    if ($values[$i] -lt 0.4) { $c = $red } elseif ($values[$i] -lt 0.65) { $c = $amber }
    $g.FillRectangle((Brush $c), $x + 5, 240 + 280 - $h + 5, 40, $h - 10)
    $g.DrawString($labels[$i], (Font 24 $true), (Brush $white), $x - 6, 536)
}

# right panel: ring gauge
$g.DrawRectangle((Pen $dim 2), 510, 210, 454, 380)
$g.DrawArc((Pen $faint 30), 617, 265, 240, 240, 135, 270)
$g.DrawArc((Pen $cyan 30), 617, 265, 240, 240, 135, 235)
$g.DrawString('87%', (Font 72 $true), (Brush $white), 662, 345)
$g.DrawString('SEAL FIELD', (Font 30 $true), (Brush $cyan), 652, 528)

# middle: field strength over time
$g.DrawRectangle((Pen $dim 2), 60, 620, 904, 170)
for ($i = 1; $i -lt 4; $i++) { $g.DrawLine((Pen $faint 2), 60, 620 + $i * 42, 964, 620 + $i * 42) }
$ys = 700, 690, 705, 684, 676, 692, 668, 672, 655, 700, 742, 715, 690, 678, 684, 670, 662, 668, 656
$pts = New-Object 'System.Drawing.Point[]' $ys.Count
for ($i = 0; $i -lt $ys.Count; $i++) { $pts[$i] = New-Object System.Drawing.Point (76 + $i * 48), $ys[$i] }
$g.DrawLines((Pen $cyan 5), $pts)
$g.FillEllipse((Brush $amber), 76 + 10 * 48 - 9, 742 - 9, 18, 18)
$g.DrawString('FIELD STRENGTH / 24 H', (Font 22), (Brush $dim), 72, 628)

# bottom: status lines
$lines = @(@('SEAL FIELD ......... HOLDING', $white), @('BREACH ALARMS ...... 0', $white), @('LAST INSPECTION .... OVERDUE', $amber))
for ($i = 0; $i -lt 3; $i++) { $g.DrawString($lines[$i][0], (Font 38 $true), (Brush $lines[$i][1]), 64, 812 + $i * 52) }

$g.Dispose()
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "written $Out"
