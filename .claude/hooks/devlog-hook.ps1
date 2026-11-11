# devlog-hook.ps1 - Hook de Claude Code para la bitacora de la jam.
#
# Que hace: cada evento de Claude Code (inicio de sesion, prompt del usuario, cada
# herramienta usada, fin de subagente, fin de turno, fin de sesion) se agrega como UNA
# linea JSON a:
#     Devlog/raw/<MAQUINA>/<AAAA-MM-DD>/claude-<session_id>.jsonl
# En Stop y SessionEnd copia ademas el transcript completo de la sesion a
#     Devlog/raw/<MAQUINA>/<AAAA-MM-DD>/transcripts/<session_id>.jsonl
#
# Instalacion: a NIVEL DE USUARIO (%USERPROFILE%\.claude\settings.json, ver
# Tools/Devlog/Install-DevlogHooks.ps1), no a nivel de proyecto: los hooks de proyecto solo
# se cargan si Claude Code arranca justo en la raiz, y una sesion abierta en una subcarpeta
# no dejaria rastro (probado 2026-11-09).
#
# Reglas de diseno:
#  - FAIL-OPEN: pase lo que pase sale con exit 0 y sin stdout. Un fallo de la bitacora
#    nunca debe bloquear al agente.
#  - No depende del esquema exacto de cada evento: guarda el payload completo, solo
#    recorta los campos largos (tool_response, tool_input, prompt) a MaxChars.
#  - UTF-8 de punta a punta (PowerShell 5.1 rompe la n con tilde si se lee stdin con la
#    codificacion por defecto).
#  - Maquina: variable de entorno JAM_MACHINE si existe; si no, COMPUTERNAME.
#  - Fechas en UTC (dos maquinas, un solo eje de tiempo).

$MaxChars = 4000

try {
    $reader = New-Object System.IO.StreamReader([Console]::OpenStandardInput(), (New-Object System.Text.UTF8Encoding($false)))
    $raw = $reader.ReadToEnd()
    if (-not $raw) { exit 0 }
    $p = $raw | ConvertFrom-Json

    # Raiz de la jam: se busca HACIA ARRIBA desde la carpeta de la sesion un "Devlog\.jamroot".
    # Asi funciona aunque Claude Code se abra en una subcarpeta (p. ej. la del .uproject), y
    # este hook, instalado a nivel de usuario, no hace NADA en sesiones fuera de la jam.
    $root = $null
    foreach ($start in @([string]$p.cwd, [string]$env:CLAUDE_PROJECT_DIR, (Get-Location).Path)) {
        if (-not $start) { continue }
        $d = $start
        while ($d) {
            if (Test-Path -LiteralPath (Join-Path $d 'Devlog\.jamroot')) { $root = $d; break }
            $parent = Split-Path $d -Parent
            if ($parent -eq $d) { break }
            $d = $parent
        }
        if ($root) { break }
    }
    if (-not $root) { exit 0 }

    $machine = $env:JAM_MACHINE
    if (-not $machine) { $machine = [Environment]::GetEnvironmentVariable('JAM_MACHINE','User') }
if (-not $machine) { $machine = $env:COMPUTERNAME }
    $nowUtc = [DateTime]::UtcNow
    $day = $nowUtc.ToString('yyyy-MM-dd')
    $sid = [string]$p.session_id
    if (-not $sid) { $sid = 'nosession' }

    $dir = Join-Path $root "Devlog\raw\$machine\$day"
    [void][IO.Directory]::CreateDirectory($dir)

    # Recorta campos largos sin perder el resto del payload.
    foreach ($field in 'tool_response', 'tool_input', 'prompt', 'last_assistant_message') {
        if ($p.PSObject.Properties.Name -contains $field) {
            $v = $p.$field
            $s = if ($v -is [string]) { $v } else { $v | ConvertTo-Json -Depth 8 -Compress }
            if ($s.Length -gt $MaxChars) {
                $p.$field = $s.Substring(0, $MaxChars) + " ...[recortado: $($s.Length) chars]"
            }
        }
    }

    $entry = [ordered]@{
        ts_utc  = $nowUtc.ToString('o')
        machine = $machine
        agent   = 'claude-code'
        event   = [string]$p.hook_event_name
        session = $sid
        payload = $p
    }
    $line = ($entry | ConvertTo-Json -Depth 10 -Compress)
    $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($line + "`n")
    # Claude Code lanza herramientas en paralelo -> varios hooks escriben a la vez en el
    # mismo archivo. Se abre con FileShare.ReadWrite y se reintenta si choca (si no, el
    # evento se perderia en silencio).
    $target = Join-Path $dir "claude-$sid.jsonl"
    $written = $false
    for ($i = 0; $i -lt 40 -and -not $written; $i++) {
        try {
            $fs = New-Object System.IO.FileStream($target, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
            try { $fs.Write($bytes, 0, $bytes.Length) } finally { $fs.Dispose() }
            $written = $true
        } catch { Start-Sleep -Milliseconds (25 + (Get-Random -Maximum 75)) }
    }
    if (-not $written) { throw "No se pudo escribir $target tras 40 intentos" }

    # Transcript completo: la fuente mas rica para la bitacora.
    if (($p.hook_event_name -eq 'Stop' -or $p.hook_event_name -eq 'SessionEnd' -or $p.hook_event_name -eq 'SubagentStop') -and $p.transcript_path) {
        $t = [string]$p.transcript_path
        if (Test-Path -LiteralPath $t) {
            $tdir = Join-Path $dir 'transcripts'
            [void][IO.Directory]::CreateDirectory($tdir)
            Copy-Item -LiteralPath $t -Destination (Join-Path $tdir "$sid.jsonl") -Force
        }
    }
} catch {
    try {
        $fallback = Join-Path ([IO.Path]::GetTempPath()) 'jam-devlog-hook-errors.log'
        Add-Content -Path $fallback -Value ("{0} {1}" -f [DateTime]::UtcNow.ToString('o'), $_.Exception.Message)
    } catch {}
}
exit 0
