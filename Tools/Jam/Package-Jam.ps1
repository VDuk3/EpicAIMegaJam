<#
Package-Jam.ps1 - Empaqueta el juego para la entrega de la jam y arma el zip.
Receta probada en UE 5.9.0 (CL 60351404) el 2026-11-09:
  - UAT BuildCookRun Win64 Development con -build -cook -stage -pak -iostore -archive.
  - NO usar -nocompileeditor: con plugins no-default activos (p. ej. AllToolsets) el cook
    busca <Proyecto>Editor.target y falla si no se compilo.
  - Config/DefaultGame.ini con bUseZenStore=False (en la maquina de assets el cook murio
    escribiendo en la carpeta de Zen; causa no confirmada). El script lo comprueba.
  - El zip EXCLUYE los .pdb (simbolos de depuracion, ~340 MB que no hacen falta).
  - Limite de la jam: zip < 1 GB, nombre TeamName_GameName.zip.

Uso:
  powershell -File Tools/Jam/Package-Jam.ps1 -Project D:\Jam\MiJuego\MiJuego.uproject -ZipName Equipo_Juego
#>
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [Parameter(Mandatory = $true)][string]$ZipName,
    [string]$EngineDir = 'C:\Program Files\Epic Games\UE_5.9',
    [string]$Config = 'Development'
)
$ErrorActionPreference = 'Stop'
$projDir = Split-Path $Project -Parent
$buildDir = Join-Path $projDir 'Build'
$logFile = Join-Path $projDir 'Saved\uat_package.log'
[void][IO.Directory]::CreateDirectory((Split-Path $logFile -Parent))

$game = Join-Path $projDir 'Config\DefaultGame.ini'
$zenOff = (Test-Path $game) -and (Select-String $game -Pattern '^\s*bUseZenStore\s*=\s*False' -Quiet)
if (-not $zenOff) {
    Write-Host 'AVISO: falta bUseZenStore=False en Config/DefaultGame.ini. Se agrega.'
    [void][IO.Directory]::CreateDirectory((Split-Path $game -Parent))   # proyecto sin Config/ (2026-11-11)
    Add-Content $game "`r`n[/Script/UnrealEd.ProjectPackagingSettings]`r`nbUseZenStore=False`r`n"
}

$uat = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUAT.bat'
$uatArgs = @('BuildCookRun', "-project=`"$Project`"", '-platform=Win64', "-clientconfig=$Config",
          '-build', '-cook', '-stage', '-pak', '-iostore', '-archive', "-archivedirectory=`"$buildDir`"",
          '-noP4', '-utf8output', '-unattended')
Write-Host "UAT -> $logFile"
& cmd.exe /d /c "`"$uat`" $($uatArgs -join ' ') > `"$logFile`" 2>&1"
$code = $LASTEXITCODE
Get-Content $logFile -Tail 4
if ($code -ne 0) {
    Write-Host "FALLO UAT (exit $code). Primeras lineas de error:"
    Select-String $logFile -Pattern 'Error:|Exception|Cook failed' | Select-Object -First 15 | ForEach-Object Line
    exit $code
}

$win = Join-Path $buildDir 'Windows'
$zip = Join-Path $projDir "$ZipName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Remove-Item (Join-Path $win '*\Saved') -Recurse -Force -ErrorAction SilentlyContinue
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [System.IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    Get-ChildItem $win -Recurse -File | Where-Object { $_.Extension -ne '.pdb' } | ForEach-Object {
        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z, $_.FullName, $_.FullName.Substring($win.Length + 1), 'Optimal')
    }
} finally { $z.Dispose() }
$mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host "ZIP: $zip ($mb MB)"
if ($mb -ge 1024) { Write-Host 'FALLO: el zip supera 1 GB (limite de la jam). Usar butler o recortar contenido.'; exit 2 }
exit 0
