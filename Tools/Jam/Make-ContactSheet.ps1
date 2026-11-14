# Make-ContactSheet.ps1 - tiles the PNG files of a folder into labelled contact sheets (one PNG per page).
# Used to review many asset pictures or renders at once.
#   powershell -File Tools/Jam/Make-ContactSheet.ps1 -Dir <folder> -Out <prefix> [-Cols 6] [-Rows 4] [-Cell 256]
param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Cols = 6,
    [int]$Rows = 4,
    [int]$Cell = 256,
    [string]$Filter = '*.png'
)
Add-Type -AssemblyName System.Drawing
$files = Get-ChildItem -Path $Dir -Filter $Filter | Sort-Object Name
$per = $Cols * $Rows
$label = 22
$page = 0
for ($i = 0; $i -lt $files.Count; $i += $per) {
    $bmp = New-Object System.Drawing.Bitmap ($Cols * $Cell), ($Rows * ($Cell + $label))
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(24, 24, 24))
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $font = New-Object System.Drawing.Font 'Consolas', 9
    for ($k = 0; $k -lt $per -and ($i + $k) -lt $files.Count; $k++) {
        $f = $files[$i + $k]
        $x = ($k % $Cols) * $Cell
        $y = [math]::Floor($k / $Cols) * ($Cell + $label)
        try {
            $img = [System.Drawing.Image]::FromFile($f.FullName)
            $s = [math]::Min($Cell / $img.Width, $Cell / $img.Height)
            $w = [int]($img.Width * $s); $h = [int]($img.Height * $s)
            $g.DrawImage($img, $x + [int](($Cell - $w) / 2), $y + [int](($Cell - $h) / 2), $w, $h)
            $img.Dispose()
        } catch { }
        $name = $f.BaseName
        if ($name.Length -gt 34) { $name = $name.Substring(0, 34) }
        $g.DrawString($name, $font, [System.Drawing.Brushes]::White, $x + 2, $y + $Cell + 3)
    }
    $g.Dispose()
    $path = "{0}_{1}.png" -f $Out, $page
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output $path
    $page++
}
