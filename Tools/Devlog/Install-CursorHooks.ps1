<#
Install-CursorHooks.ps1 - Instala el hook de bitacora de Cursor a nivel de USUARIO
(%USERPROFILE%\.cursor\hooks.json), para maquinas sin repo (p. ej. la LAPTOP de arte).

- Copia cursor-devlog-hook.ps1 (de la misma carpeta que este script) a C:\JamLog\bin\.
- Hace copia de seguridad de hooks.json (hooks.json.bak-<fecha>) y FUSIONA: conserva los
  demas hooks y solo quita/pone las entradas que apuntan a cursor-devlog-hook.ps1.
  Idempotente. -Uninstall quita esas entradas.
- Solo eventos que no deciden permisos (el hook nunca bloquea ni aprueba nada).

Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File Install-CursorHooks.ps1
#>
param(
    [string]$BinDir = 'C:\JamLog\bin',
    [string]$LogRoot = 'C:\JamLog\raw',
    [string]$HooksPath = (Join-Path $env:USERPROFILE '.cursor\hooks.json'),
    [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$hookName = 'cursor-devlog-hook.ps1'
$hook = Join-Path $BinDir $hookName

if (-not $Uninstall) {
    $src = Join-Path $PSScriptRoot $hookName
    if (-not (Test-Path $src)) { throw "No existe $src" }
    [void][IO.Directory]::CreateDirectory($BinDir)
    [void][IO.Directory]::CreateDirectory($LogRoot)
    Copy-Item -LiteralPath $src -Destination $hook -Force
}

[void][IO.Directory]::CreateDirectory((Split-Path $HooksPath -Parent))
$json = '{}'
if (Test-Path $HooksPath) {
    Copy-Item $HooksPath ("$HooksPath.bak-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    $json = [IO.File]::ReadAllText($HooksPath, $utf8)
    if (-not $json.Trim()) { $json = '{}' }
}
$s = $json | ConvertFrom-Json
if (-not ($s.PSObject.Properties.Name -contains 'version')) { $s | Add-Member -NotePropertyName version -NotePropertyValue 1 }
if (-not ($s.PSObject.Properties.Name -contains 'hooks')) { $s | Add-Member -NotePropertyName hooks -NotePropertyValue ([pscustomobject]@{}) }

$cmd = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "' + $hook + '" -LogRoot "' + $LogRoot + '"'
$events = 'sessionStart', 'sessionEnd', 'beforeSubmitPrompt', 'postToolUse', 'postToolUseFailure', 'subagentStop', 'afterAgentResponse', 'stop', 'preCompact'

$names = @($s.hooks.PSObject.Properties.Name)
foreach ($ev in ($names + $events | Select-Object -Unique)) {
    $kept = @()
    if ($names -contains $ev) { $kept = @($s.hooks.$ev | Where-Object { [string]$_.command -notmatch [regex]::Escape($hookName) }) }
    if (-not $Uninstall -and $events -contains $ev) { $kept += [pscustomobject][ordered]@{ command = $cmd; timeout = 15 } }
    if ($names -contains $ev) { $s.hooks.PSObject.Properties.Remove($ev) }
    if ($kept.Count -gt 0) { $s.hooks | Add-Member -NotePropertyName $ev -NotePropertyValue @($kept) }
}
[IO.File]::WriteAllText($HooksPath, ($s | ConvertTo-Json -Depth 20), $utf8)
if ($Uninstall) { Write-Host "Hooks de bitacora quitados de $HooksPath" } else { Write-Host "Hooks de bitacora instalados en $HooksPath -> $hook (logs en $LogRoot)" }
