<#
Setup-IncomingShare.ps1 - Configura la carpeta compartida de entrada de assets (SETUP F1c).
REQUIERE PowerShell ELEVADA. Idempotente: se puede correr varias veces.

Antes (lo hace la persona, el agente no escribe contrasenas):
  net user jamshare * /add          (pide la contrasena; luego darsela a las otras maquinas)

Que hace:
  1. Perfil de la red Wi-Fi indicada -> Privado.
  2. Firewall: dos reglas propias (grupo "JamIncoming"), solo perfil Privado y solo LocalSubnet:
     SMB TCP 445 y ping ICMPv4. NO toca las reglas de "Compartir archivos e impresoras" de
     Windows (vienen como "Private, Public" en un solo objeto; activarlas abriria tambien Publico).
  3. NTFS en <JamRoot>\Incoming: corta la herencia de C:\ (que da Modificar = BORRAR a
     "Authenticated Users") y deja:
       SYSTEM, Administradores, <usuario actual>: control total
       <ShareUser>: en la raiz solo leer/recorrer (sin herencia);
                    en ASSETS\ y LAPTOP\: crear archivos y carpetas, escribir, anexar, leer, atributos.
                    SIN Eliminar ni Eliminar subcarpetas y archivos.  _Imported\: sin acceso.
       OWNER RIGHTS: solo lectura (si no, el dueno de un archivo nuevo podria cambiar su ACL
                    y darse permiso de borrar).
  4. Recurso compartido "Incoming": Cambiar para <ShareUser>, Total para el usuario actual.
  5. Tarea programada "JamContentBackup" (diaria 04:00, DESACTIVADA) que llama a Backup-Content.ps1.
     Activarla cuando exista el proyecto: ajustar -ProjectDir en la accion y Enable-ScheduledTask.

Limitacion conocida: sin permiso de borrar tampoco se puede RENOMBRAR. Herramientas que escriben
un temporal y luego lo renombran sobre el final fallaran al sobrescribir. robocopy y la copia del
Explorador sobrescriben en el sitio (sirve). -Uninstall no existe: ver "Deshacer" al final.

Uso:
  powershell -NoProfile -ExecutionPolicy Bypass -File C:\GameJam\Tools\Jam\Setup-IncomingShare.ps1 `
      -WifiProfile Romelt_5G -BackupDest "$env:USERPROFILE\OneDrive\JamBackup"
#>
param(
    [string]$JamRoot = 'C:\GameJam',
    [string]$ShareUser = 'jamshare',
    [Parameter(Mandatory = $true)][string]$WifiProfile,
    [string]$BackupDest = (Join-Path $env:USERPROFILE 'OneDrive\JamBackup'),
    [string]$ProjectDirPlaceholder = 'C:\GameJam\CAMBIAR_POR_EL_PROYECTO',
    [string]$OwnerUser = $env:USERNAME
)
$ErrorActionPreference = 'Stop'
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { Write-Host 'Hace falta una PowerShell ELEVADA.'; exit 1 }
if (-not (Get-LocalUser -Name $ShareUser -ErrorAction SilentlyContinue)) {
    Write-Host "No existe el usuario local '$ShareUser'. Crealo primero: net user $ShareUser * /add"; exit 1
}
$inc = Join-Path $JamRoot 'Incoming'
foreach ($s in 'ASSETS', 'LAPTOP', '_Imported') { [void][IO.Directory]::CreateDirectory((Join-Path $inc $s)) }
$pc = $env:COMPUTERNAME
$su = "$pc\$ShareUser"
$ou = "$pc\$OwnerUser"

Write-Host '== 1. Perfil de red'
Set-NetConnectionProfile -Name $WifiProfile -NetworkCategory Private
Get-NetConnectionProfile -Name $WifiProfile | ForEach-Object { "  $($_.Name): $($_.NetworkCategory)" }

Write-Host '== 2. Firewall (grupo JamIncoming, solo Private + LocalSubnet)'
Get-NetFirewallRule -Group 'JamIncoming' -ErrorAction SilentlyContinue | Remove-NetFirewallRule
[void](New-NetFirewallRule -DisplayName 'Jam Incoming - SMB-In (TCP 445)' -Group 'JamIncoming' -Direction Inbound -Protocol TCP -LocalPort 445 -Profile Private -RemoteAddress LocalSubnet -Action Allow)
[void](New-NetFirewallRule -DisplayName 'Jam Incoming - Ping (ICMPv4 echo)' -Group 'JamIncoming' -Direction Inbound -Protocol ICMPv4 -IcmpType 8 -Profile Private -RemoteAddress LocalSubnet -Action Allow)
Get-NetFirewallRule -Group 'JamIncoming' | ForEach-Object { "  $($_.DisplayName) | $($_.Profile) | Enabled=$($_.Enabled)" }

Write-Host '== 3. NTFS'
# SIDs para no depender del idioma de Windows: S-1-5-18 SYSTEM, S-1-5-32-544 Administradores, S-1-3-4 OWNER RIGHTS.
& icacls $inc /inheritance:r /grant:r '*S-1-5-18:(OI)(CI)(F)' '*S-1-5-32-544:(OI)(CI)(F)' "${ou}:(OI)(CI)(F)" "${su}:(RX)" '*S-1-3-4:(OI)(CI)(RX)' | Out-Null
if ($LASTEXITCODE -ne 0) { throw "icacls raiz fallo ($LASTEXITCODE)" }
foreach ($s in 'ASSETS', 'LAPTOP') {
    $p = Join-Path $inc $s
    & icacls $p /reset /T /C | Out-Null    # que hereden solo de la raiz recien fijada
    & icacls $p /grant:r "${su}:(OI)(CI)(RD,WD,AD,REA,WEA,X,RA,WA,RC,S)" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "icacls $s fallo ($LASTEXITCODE)" }
}
& icacls (Join-Path $inc '_Imported') /reset /T /C | Out-Null
& icacls $inc
foreach ($s in 'ASSETS', 'LAPTOP', '_Imported') { & icacls (Join-Path $inc $s) }

Write-Host '== 4. Recurso compartido'
if (Get-SmbShare -Name 'Incoming' -ErrorAction SilentlyContinue) { Remove-SmbShare -Name 'Incoming' -Force }
[void](New-SmbShare -Name 'Incoming' -Path $inc -ChangeAccess $su -FullAccess $ou -Description 'Jam: entrada de assets (escribir si, borrar no)')
Get-SmbShareAccess -Name 'Incoming' | ForEach-Object { "  $($_.AccountName): $($_.AccessRight) $($_.AccessControlType)" }
Write-Host "  Ruta para las otras maquinas: \\$pc\Incoming\  (o por IP si el nombre no resuelve)"

Write-Host '== 5. Tarea programada JamContentBackup (desactivada)'
$script = Join-Path $JamRoot 'Tools\Jam\Backup-Content.ps1'
$arg = "-NoProfile -ExecutionPolicy Bypass -File `"$script`" -ProjectDir `"$ProjectDirPlaceholder`" -Dest `"$BackupDest`""
$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $arg
$trigger = New-ScheduledTaskTrigger -Daily -At 4am
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -DontStopIfGoingOnBatteries -AllowStartIfOnBatteries
$principal = New-ScheduledTaskPrincipal -UserId $ou -LogonType Interactive -RunLevel Limited
[void](Register-ScheduledTask -TaskName 'JamContentBackup' -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Force)
[void](Disable-ScheduledTask -TaskName 'JamContentBackup')
Get-ScheduledTask -TaskName 'JamContentBackup' | ForEach-Object { "  $($_.TaskName): $($_.State)" }
Write-Host 'LISTO.'
<#
Deshacer (despues de la jam, PowerShell elevada):
  Remove-SmbShare -Name Incoming -Force
  Get-NetFirewallRule -Group JamIncoming | Remove-NetFirewallRule
  Unregister-ScheduledTask -TaskName JamContentBackup -Confirm:$false
  icacls C:\GameJam\Incoming /reset /T
  net user jamshare /delete
#>
