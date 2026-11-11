<#
Install-DevlogHooks.ps1 - Instala los hooks de bitacora en la configuracion de USUARIO de
Claude Code (%USERPROFILE%\.claude\settings.json), apuntando con ruta absoluta al hook de
esta jam.

Por que a nivel de usuario: los hooks de proyecto (.claude/settings.json) solo se cargan si
Claude Code arranca justo en la raiz; una sesion abierta en una subcarpeta (p. ej. la del
.uproject) no registraria nada. El hook busca hacia arriba "Devlog\.jamroot", asi que fuera
de la jam no hace nada.

- Hace copia de seguridad de settings.json antes de tocarlo (settings.json.bak-<fecha>).
- Fusiona: conserva todos los demas ajustes y hooks; solo quita/pone las entradas que apuntan
  a devlog-hook.ps1 (idempotente, se puede correr varias veces).
- -Uninstall quita esas entradas.

Uso (desde la raiz de la jam):
  powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Devlog/Install-DevlogHooks.ps1
#>
param(
    [string]$JamRoot = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [string]$SettingsPath = (Join-Path $env:USERPROFILE '.claude\settings.json'),
    [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'
$hook = Join-Path $JamRoot '.claude\hooks\devlog-hook.ps1'
if (-not $Uninstall -and -not (Test-Path $hook)) { throw "No existe $hook" }
if (-not $Uninstall -and -not (Test-Path (Join-Path $JamRoot 'Devlog\.jamroot'))) { throw "Falta $JamRoot\Devlog\.jamroot (marca de la raiz de la jam)" }

$utf8 = New-Object System.Text.UTF8Encoding($false)
[void][IO.Directory]::CreateDirectory((Split-Path $SettingsPath -Parent))
$json = '{}'
if (Test-Path $SettingsPath) {
    Copy-Item $SettingsPath ("$SettingsPath.bak-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    $json = [IO.File]::ReadAllText($SettingsPath, $utf8)
    if (-not $json.Trim()) { $json = '{}' }
}
$s = $json | ConvertFrom-Json
if (-not ($s.PSObject.Properties.Name -contains 'hooks')) { $s | Add-Member -NotePropertyName hooks -NotePropertyValue ([pscustomobject]@{}) }

$cmd = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "' + ($hook -replace '\\', '/') + '"'
$events = [ordered]@{ SessionStart = 15; UserPromptSubmit = 15; PostToolUse = 15; SubagentStop = 15; Stop = 30; SessionEnd = 30 }

foreach ($ev in $events.Keys) {
    $existing = @()
    if ($s.hooks.PSObject.Properties.Name -contains $ev) { $existing = @($s.hooks.$ev) }
    # quitar entradas previas de este hook
    $kept = @()
    foreach ($group in $existing) {
        $inner = @($group.hooks | Where-Object { [string]$_.command -notmatch 'devlog-hook\.ps1' })
        if ($inner.Count -gt 0) { $group.hooks = $inner; $kept += $group }
    }
    if (-not $Uninstall) {
        $entry = [ordered]@{ hooks = @([ordered]@{ type = 'command'; command = $cmd; timeout = $events[$ev] }) }
        if ($ev -eq 'PostToolUse') { $entry = [ordered]@{ matcher = '*'; hooks = $entry.hooks } }
        $kept += [pscustomobject]$entry
    }
    if ($s.hooks.PSObject.Properties.Name -contains $ev) { $s.hooks.PSObject.Properties.Remove($ev) }
    if ($kept.Count -gt 0) { $s.hooks | Add-Member -NotePropertyName $ev -NotePropertyValue @($kept) }
}
[IO.File]::WriteAllText($SettingsPath, ($s | ConvertTo-Json -Depth 20), $utf8)
if ($Uninstall) { Write-Host "Hooks de bitacora quitados de $SettingsPath" } else { Write-Host "Hooks de bitacora instalados en $SettingsPath -> $hook" }
