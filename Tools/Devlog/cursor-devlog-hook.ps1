# cursor-devlog-hook.ps1 - Hook de Cursor para la bitacora de la jam (maquinas SIN repo,
# p. ej. la LAPTOP de arte). Pareja de .claude/hooks/devlog-hook.ps1 (Claude Code).
#
# Que hace: cada evento de Cursor se agrega como UNA linea JSON a
#     <LogRoot>\<MAQUINA>\<AAAA-MM-DD>\cursor-<conversation_id>.jsonl
# y en stop / sessionEnd / subagentStop copia el transcript a
#     <LogRoot>\<MAQUINA>\<AAAA-MM-DD>\transcripts\cursor-<conversation_id>.<ext>
# LogRoot por defecto: C:\JamLog\raw (Sync-DevlogToCentral.ps1 lo copia a la CENTRAL).
#
# Reglas de diseno (iguales que el hook de Claude Code):
#  - FAIL-OPEN y sin efectos: siempre exit 0 y una respuesta JSON neutra. Solo se registra
#    en eventos que NO deciden permisos (Install-CursorHooks.ps1 no lo pone en before*/pre*,
#    salvo beforeSubmitPrompt, que responde {"continue":true}).
#  - Guarda el payload completo, recorta los campos largos a MaxChars y quita user_email.
#  - UTF-8; fechas en UTC; maquina = JAM_MACHINE o COMPUTERNAME.
param([string]$LogRoot = 'C:\JamLog\raw')

$MaxChars = 4000
$event = ''
try {
    $raw = $null
    if ([Console]::IsInputRedirected) {
        $reader = New-Object System.IO.StreamReader([Console]::OpenStandardInput(), (New-Object System.Text.UTF8Encoding($false)))
        $raw = $reader.ReadToEnd()
    }
    if (-not $raw) { $raw = (@($input) | ForEach-Object { [string]$_ }) -join "`n" }
    if ($raw) {
        $p = $raw | ConvertFrom-Json
        $event = [string]$p.hook_event_name

        $machine = $env:JAM_MACHINE
        if (-not $machine) { $machine = [Environment]::GetEnvironmentVariable('JAM_MACHINE', 'User') }
        if (-not $machine) { $machine = $env:COMPUTERNAME }
        $nowUtc = [DateTime]::UtcNow
        $dir = Join-Path $LogRoot ("$machine\" + $nowUtc.ToString('yyyy-MM-dd'))
        [void][IO.Directory]::CreateDirectory($dir)

        $sid = [string]$p.conversation_id
        if (-not $sid) { $sid = [string]$p.session_id }
        if (-not $sid) { $sid = 'nosession' }
        $sid = $sid -replace '[^A-Za-z0-9_\-]', '_'

        if ($p.PSObject.Properties.Name -contains 'user_email') { $p.PSObject.Properties.Remove('user_email') }
        foreach ($field in 'prompt', 'text', 'tool_input', 'tool_output', 'output', 'result_json', 'edits', 'content', 'summary', 'task') {
            if ($p.PSObject.Properties.Name -contains $field) {
                $v = $p.$field
                $s = if ($v -is [string]) { $v } else { $v | ConvertTo-Json -Depth 8 -Compress }
                if ($s -and $s.Length -gt $MaxChars) { $p.$field = $s.Substring(0, $MaxChars) + " ...[recortado: $($s.Length) chars]" }
            }
        }

        $entry = [ordered]@{
            ts_utc  = $nowUtc.ToString('o')
            machine = $machine
            agent   = 'cursor'
            event   = $event
            session = $sid
            payload = $p
        }
        $bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes((($entry | ConvertTo-Json -Depth 10 -Compress)) + "`n")
        $target = Join-Path $dir "cursor-$sid.jsonl"
        # Varios hooks pueden escribir a la vez en el mismo archivo. FileMode.Append NO es atomico
        # entre procesos (cada uno fija la posicion al abrir y puede pisar la linea del otro: visto
        # en la LAPTOP el 2026-11-12), asi que la escritura va bajo un mutex con nombre por archivo.
        $md5 = [Security.Cryptography.MD5]::Create()
        $mname = 'Local\jam-devlog-' + (($md5.ComputeHash([Text.Encoding]::UTF8.GetBytes($target.ToLowerInvariant())) | ForEach-Object { $_.ToString('x2') }) -join '')
        $mtx = New-Object System.Threading.Mutex($false, $mname)
        $owned = $false
        try { $owned = $mtx.WaitOne(10000) } catch [System.Threading.AbandonedMutexException] { $owned = $true }
        $written = $false
        try {
            for ($i = 0; $i -lt 40 -and -not $written; $i++) {
                try {
                    $fs = New-Object System.IO.FileStream($target, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
                    try { $fs.Write($bytes, 0, $bytes.Length) } finally { $fs.Dispose() }
                    $written = $true
                } catch { Start-Sleep -Milliseconds (25 + (Get-Random -Maximum 75)) }
            }
        } finally { if ($owned) { $mtx.ReleaseMutex() }; $mtx.Dispose() }
        if (-not $written) { throw "No se pudo escribir $target" }

        if ($event -in @('stop', 'sessionEnd', 'subagentStop')) {
            foreach ($tp in @([string]$p.transcript_path, [string]$p.agent_transcript_path)) {
                if ($tp -and (Test-Path -LiteralPath $tp)) {
                    $tdir = Join-Path $dir 'transcripts'
                    [void][IO.Directory]::CreateDirectory($tdir)
                    $name = 'cursor-' + $sid + '-' + ([IO.Path]::GetFileName($tp))
                    Copy-Item -LiteralPath $tp -Destination (Join-Path $tdir $name) -Force
                }
            }
        }
    }
} catch {
    try { Add-Content -Path (Join-Path ([IO.Path]::GetTempPath()) 'jam-cursor-hook-errors.log') -Value ("{0} {1}" -f [DateTime]::UtcNow.ToString('o'), $_.Exception.Message) } catch {}
}
if ($event -eq 'beforeSubmitPrompt') { '{"continue":true}' } else { '{}' }
exit 0
