# Make-CrackTexture.ps1 - draws the crack mask used on things that break after a few hits (AGBBreakable).
#   powershell -File Tools/Jam/Make-CrackTexture.ps1
# White lines on black. Each crack starts bright at its seed and gets darker towards its tips, so a material that
# shows only what is brighter than a threshold makes the cracks grow as the threshold drops (M_GB_Cracks, "Stage").
# Drawn with System.Drawing from a fixed random seed: the same picture every run, no source image.
param([string]$Out = 'C:\GameJam\Glyphbound\SourceArt\Environment\Breakable\T_GB_Cracks.png', [int]$Seed = 20261114)
Add-Type -AssemblyName System.Drawing
$S = 1024
$bmp = New-Object System.Drawing.Bitmap $S, $S
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.Clear([System.Drawing.Color]::Black)
$rnd = New-Object System.Random $Seed

function Crack([double]$x, [double]$y, [double]$angle, [double]$bright, [double]$width, [int]$depth) {
    # A crack walks in short straight pieces, turning a little each time, losing brightness; now and then it forks.
    $steps = 14 + $rnd.Next(10)
    for ($i = 0; $i -lt $steps; $i++) {
        $len = 22 + $rnd.NextDouble() * 34
        $angle += ($rnd.NextDouble() - 0.5) * 1.1
        $nx = $x + [Math]::Cos($angle) * $len
        $ny = $y + [Math]::Sin($angle) * $len
        $v = [int][Math]::Max(0, [Math]::Min(255, $bright))
        $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, $v, $v, $v)), ([single][Math]::Max(1.2, $width))
        $pen.StartCap = 'Round'; $pen.EndCap = 'Round'
        $g.DrawLine($pen, [single]$x, [single]$y, [single]$nx, [single]$ny)
        $pen.Dispose()
        $x = $nx; $y = $ny
        $bright -= 6.5
        $width *= 0.94
        if ($bright -lt 105) { return }
        if ($depth -lt 3 -and $rnd.NextDouble() -lt 0.22) {
            Crack $x $y ($angle + (($rnd.Next(2) * 2 - 1) * (0.6 + $rnd.NextDouble() * 0.7))) ($bright - 14) ($width * 0.8) ($depth + 1)
        }
    }
}

# Five places where the thing is hit; from each, cracks run out in several directions.
$seeds = @(@(512, 512), @(250, 330), @(770, 300), @(300, 760), @(740, 740))
foreach ($s in $seeds) {
    $arms = 5 + $rnd.Next(3)
    for ($a = 0; $a -lt $arms; $a++) {
        $ang = ($a / $arms) * 2 * [Math]::PI + $rnd.NextDouble() * 0.6
        Crack $s[0] $s[1] $ang 255 7.0 0
    }
}
$g.Dispose()
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "written $Out"
