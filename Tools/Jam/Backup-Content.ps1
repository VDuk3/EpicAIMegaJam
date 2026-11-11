<#
Backup-Content.ps1 - Copia diaria del proyecto del juego (el Content/ no tiene historial en git).
Espeja con robocopy /MIR el proyecto SIN Binaries, Intermediate, Saved, DerivedDataCache ni Build.
Pensado para la tarea programada "JamContentBackup" (SETUP F1c punto 5).

Uso:
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Jam\Backup-Content.ps1 `
      -ProjectDir C:\GameJam\MiJuego -Dest C:\Users\<user>\OneDrive\JamBackup

Salida: <Dest>\<NombreProyecto>\ (espejo) y <Dest>\backup.log (una linea por ejecucion + log de robocopy).
OJO: /MIR borra en el destino lo que ya no existe en el origen. Por eso el script se niega a
correr si el destino esta dentro del proyecto (o al reves), o si el destino es la raiz de un disco.
Exit: 0 bien, 1 parametros invalidos, 2 robocopy fallo (codigo >= 8).
#>
param(
    [Parameter(Mandatory = $true)][string]$ProjectDir,
    [Parameter(Mandatory = $true)][string]$Dest
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ProjectDir -PathType Container)) { Write-Host "No existe el proyecto: $ProjectDir"; exit 1 }
$src = (Resolve-Path -LiteralPath $ProjectDir).Path.TrimEnd('\')
$uproj = @(Get-ChildItem -LiteralPath $src -Filter *.uproject -File)
if ($uproj.Count -ne 1) { Write-Host "Se esperaba exactamente un .uproject en $src (hay $($uproj.Count))"; exit 1 }
$name = [IO.Path]::GetFileNameWithoutExtension($uproj[0].Name)

$destRoot = [IO.Path]::GetFullPath($Dest).TrimEnd('\')
if ($destRoot -match '^[A-Za-z]:$') { Write-Host "El destino no puede ser la raiz de un disco: $Dest"; exit 1 }
$srcCmp = $src.ToLowerInvariant() + '\'
$dstCmp = $destRoot.ToLowerInvariant() + '\'
if ($dstCmp.StartsWith($srcCmp) -or $srcCmp.StartsWith($dstCmp)) { Write-Host "Origen y destino se solapan: $src / $destRoot"; exit 1 }

$target = Join-Path $destRoot $name
[void][IO.Directory]::CreateDirectory($target)
$log = Join-Path $destRoot 'backup.log'
$start = Get-Date
Add-Content -LiteralPath $log -Value ("==== {0} {1} -> {2}" -f $start.ToString('s'), $src, $target)

& robocopy $src $target /MIR /XD Binaries Intermediate Saved DerivedDataCache Build .vs .git /R:2 /W:5 /NP /NFL /NDL "/LOG+:$log" | Out-Null
$code = $LASTEXITCODE
$secs = [int]((Get-Date) - $start).TotalSeconds
Add-Content -LiteralPath $log -Value ("==== fin robocopy={0} ({1} s)" -f $code, $secs)
if ($code -ge 8) { Write-Host "FALLO robocopy (codigo $code). Ver $log"; exit 2 }
Write-Host "Copia OK (robocopy $code, $secs s) -> $target"
exit 0
