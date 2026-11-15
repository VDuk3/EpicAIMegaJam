# Usage: Gun-Mask.ps1 <template.png> <out_mask.txt> -SeedX <art x> -SeedY <art y> [-Dark 160] [-Fill 0.55] [-Preview <png>]
# The gun's silhouette on a 1920x1080 template, at art resolution (160x90, one char per 12x12 block): a block is gun
# ('#') when at least Fill of its pixels are darker than Dark (the guns are dull grey-brown over a light floor; the
# floor's dark grout lines are too thin to fill a block), and it is connected to the seed block (a block known to be on the
# gun). Used to draw the first-person hands against the real gun and to check them (hand_shapes.js).
# -Preview writes the template with the mask's outline in magenta, to check the mask by eye.
param([string]$Template, [string]$Out, [int]$SeedX, [int]$SeedY, [int]$Dark = 160, [double]$Fill = 0.55, [string]$Preview = '')
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class GunMaskOps {
  public static double[] DarkShare(Bitmap b, int dark) {
    var d = b.LockBits(new Rectangle(0,0,b.Width,b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    var a = new byte[d.Stride*b.Height]; Marshal.Copy(d.Scan0, a, 0, a.Length); b.UnlockBits(d);
    var s = new double[160*90];
    for (int ay=0; ay<90; ay++) for (int ax=0; ax<160; ax++) { int c=0;
      for (int y=ay*12; y<ay*12+12; y++) for (int x=ax*12; x<ax*12+12; x++) { int o=y*d.Stride+x*4;
        int lum = (a[o+2]*30 + a[o+1]*59 + a[o]*11)/100; if (lum < dark) c++; }
      s[ay*160+ax] = c/144.0; }
    return s; }
}
"@
$img = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Template).Path)
$share = [GunMaskOps]::DarkShare($img, $Dark)
$isGun = New-Object 'bool[]' (160 * 90)
for ($i = 0; $i -lt $share.Length; $i++) { $isGun[$i] = $share[$i] -ge $Fill }
# Keep the 4-connected region of the seed.
$keep = New-Object 'bool[]' (160 * 90)
$stack = New-Object System.Collections.Generic.Stack[int]
$seed = $SeedY * 160 + $SeedX
if (-not $isGun[$seed]) { Write-Host "seed ($SeedX,$SeedY) is not dark enough"; exit 1 }
$stack.Push($seed); $keep[$seed] = $true
while ($stack.Count -gt 0) {
    $p = $stack.Pop(); $x = $p % 160; $y = [int][Math]::Floor($p / 160)
    foreach ($d in @(@(1,0),@(-1,0),@(0,1),@(0,-1))) {
        $nx = $x + $d[0]; $ny = $y + $d[1]
        if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge 160 -or $ny -ge 90) { continue }
        $q = $ny * 160 + $nx
        if ($isGun[$q] -and -not $keep[$q]) { $keep[$q] = $true; $stack.Push($q) }
    }
}
$lines = for ($y = 0; $y -lt 90; $y++) { -join (0..159 | ForEach-Object { if ($keep[$y * 160 + $_]) { '#' } else { '.' } }) }
[IO.File]::WriteAllLines($Out, $lines)
if ($Preview) {
    $g = [System.Drawing.Graphics]::FromImage($img)
    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Magenta), 2
    for ($y = 0; $y -lt 90; $y++) { for ($x = 0; $x -lt 160; $x++) {
        if (-not $keep[$y * 160 + $x]) { continue }
        if ($x -eq 159 -or -not $keep[$y * 160 + $x + 1]) { $g.DrawLine($pen, ($x + 1) * 12, $y * 12, ($x + 1) * 12, ($y + 1) * 12) }
        if ($x -eq 0 -or -not $keep[$y * 160 + $x - 1]) { $g.DrawLine($pen, $x * 12, $y * 12, $x * 12, ($y + 1) * 12) }
        if ($y -eq 89 -or -not $keep[($y + 1) * 160 + $x]) { $g.DrawLine($pen, $x * 12, ($y + 1) * 12, ($x + 1) * 12, ($y + 1) * 12) }
        if ($y -eq 0 -or -not $keep[($y - 1) * 160 + $x]) { $g.DrawLine($pen, $x * 12, $y * 12, ($x + 1) * 12, $y * 12) }
    } }
    $g.Dispose()
    $img.Save($Preview, [System.Drawing.Imaging.ImageFormat]::Png)
}
$img.Dispose()
Write-Host "mask $Out ($(($keep | Where-Object { $_ }).Count) blocks)"
