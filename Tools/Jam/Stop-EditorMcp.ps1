<#
Stop-EditorMcp.ps1 - Cierra el Editor de forma ordenada por MCP (consola: QUIT_EDITOR).
Cerrar la ventana a mano/CloseMainWindow no basta en 5.9 (queda colgado). Nunca matar el proceso.
ANTES: guardar el trabajo. QUIT_EDITOR puede mostrar el dialogo de guardar si hay cambios.
Uso:  powershell -File Tools/Jam/Stop-EditorMcp.ps1 -Port 8059
#>
param([int]$Port = 8059, [int]$TimeoutSeconds = 120)
$u = "http://127.0.0.1:$Port/mcp"
$h = @{ 'Accept' = 'application/json, text/event-stream' }
$init = @{ jsonrpc = '2.0'; id = 1; method = 'initialize'; params = @{ protocolVersion = '2025-06-18'; capabilities = @{}; clientInfo = @{ name = 'jam-stop'; version = '1' } } } | ConvertTo-Json -Depth 6 -Compress
$r = Invoke-WebRequest -Uri $u -Method Post -Headers $h -ContentType 'application/json' -Body $init -UseBasicParsing -TimeoutSec 20
$initTxt = if ($r.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($r.Content) } else { [string]$r.Content }
$meta = ($initTxt | ConvertFrom-Json).result._meta.'com.epicgames.unrealengine/instance'
$pid0 = $meta.processId
$projDir = $meta.projectDirectory
$h['Mcp-Session-Id'] = $r.Headers['Mcp-Session-Id']
# Cerrar primero los editores de assets abiertos: con el editor de animacion (Persona) abierto,
# QUIT_EDITOR crasheo al apagar (EXCEPTION_ACCESS_VIOLATION en UnrealEditor-Persona, 2026-11-10).
function Invoke-EdTool($tool, $toolArgs) {
    $b = @{ jsonrpc = '2.0'; id = 3; method = 'tools/call'; params = @{ name = 'call_tool'; arguments = @{ toolset_name = 'EditorToolset.EditorAppToolset'; tool_name = $tool; arguments = $toolArgs } } } | ConvertTo-Json -Depth 8 -Compress
    $x = Invoke-WebRequest -Uri $u -Method Post -Headers $h -ContentType 'application/json' -Body $b -UseBasicParsing -TimeoutSec 30
    $s = if ($x.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($x.Content) } else { [string]$x.Content }
    if ($s -match '^\s*(event:|data:)') { $s = (($s -split "`n") | Where-Object { $_ -like 'data:*' } | ForEach-Object { $_.Substring(5) }) -join '' }
    return ($s | ConvertFrom-Json)
}
try {
    $open = @((Invoke-EdTool 'GetOpenAssets' @{}).result.structuredContent.returnValue)
    foreach ($a in $open) {
        $p = if ($a -is [string]) { $a } elseif ($a.refPath) { $a.refPath } elseif ($a.objectPath) { $a.objectPath } else { [string]$a }
        if ($p) {
            # unsavedChanges es obligatorio (Fail/Save/Discard); sin el, la tool falla y el editor sigue abierto.
            $c = Invoke-EdTool 'CloseEditorForAsset' @{ assetPath = $p; unsavedChanges = 'Save' }
            if ($c.result.isError) { Write-Host "AVISO: no se pudo cerrar el editor de $p : $($c.result.content[0].text)" } else { Write-Host "Cerrado editor de asset: $p" }
        }
    }
    # CloseEditorForAsset vuelve antes de que la ventana se destruya; si QUIT_EDITOR llega en ese
    # momento, Persona crashea igual (2026-11-10, segunda prueba). Esperar a que no quede ninguno.
    if ($open.Count -gt 0) {
        $limit2 = (Get-Date).AddSeconds(20)
        while ((Get-Date) -lt $limit2 -and @((Invoke-EdTool 'GetOpenAssets' @{}).result.structuredContent.returnValue).Count -gt 0) { Start-Sleep 1 }
        Start-Sleep 3
    }
} catch { Write-Host "Aviso: no se pudieron cerrar los editores de assets ($($_.Exception.Message))" }
$quit = @{ jsonrpc = '2.0'; id = 2; method = 'tools/call'; params = @{ name = 'call_tool'; arguments = @{ toolset_name = 'ConsoleToolset.ConsoleToolset'; tool_name = 'RunConsoleCommand'; arguments = @{ Command = 'QUIT_EDITOR' } } } } | ConvertTo-Json -Depth 8 -Compress
# Si el Editor esta en una "slow task" (shaders/carga) la orden no se ejecuta: reintentar hasta 5 min.
$limit = (Get-Date).AddMinutes(5)
while ($true) {
    $txt = ''
    try {
        $q = Invoke-WebRequest -Uri $u -Method Post -Headers $h -ContentType 'application/json' -Body $quit -UseBasicParsing -TimeoutSec 20
        $txt = if ($q.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($q.Content) } else { [string]$q.Content }
    } catch { break }   # sin respuesta = el Editor ya se esta cerrando
    if ($txt -notmatch 'slow task' -or (Get-Date) -gt $limit) { break }
    Start-Sleep 5
}
$t = 0
while ($t -lt $TimeoutSeconds) {
    Start-Sleep 3; $t += 3
    if (-not (Get-Process -Id $pid0 -ErrorAction SilentlyContinue)) {
        Write-Host "Editor (PID $pid0) cerrado en $t s"
        $log = if ($projDir) { Get-ChildItem (Join-Path $projDir 'Saved\Logs') -Filter *.log -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1 }
        if ($log -and (Select-String $log.FullName -Pattern 'Unhandled Exception|Fatal error!' -Quiet)) { Write-Host "AVISO: el Editor crasheo al cerrar; ver $($log.FullName) y Saved\Crashes" }
        # Bitacora: rescatar las acciones de agentes del log ANTES de que el proximo arranque lo rote.
        $collector = Join-Path (Split-Path $PSScriptRoot -Parent) 'Devlog\Collect-UnrealAgentLog.ps1'
        if ($projDir -and (Test-Path $collector)) { & $collector -ProjectDir $projDir }
        exit 0
    }
}
Write-Host "El Editor (PID $pid0) sigue abierto: probablemente hay un dialogo de guardar. Revisarlo a mano; NO matar el proceso."
exit 1
