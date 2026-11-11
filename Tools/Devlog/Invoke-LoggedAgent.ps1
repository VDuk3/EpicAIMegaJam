<#
Invoke-LoggedAgent.ps1 - Ejecuta CUALQUIER agente externo (Codex, cursor-agent, OpenCode,
DeepSeek dsh, Gemini, scripts de generacion de assets...) dejando registro en la bitacora.

Los hooks de .claude solo ven a Claude Code. Todo lo demas DEBE lanzarse por este wrapper,
o no existira en la bitacora.

Uso (desde la raiz del proyecto de la jam):
  powershell -File Tools/Devlog/Invoke-LoggedAgent.ps1 -Agent codex -Model gpt-6.1-sol `
      -PromptFile Prompts/review_x.md -Purpose "revision del sistema X" `
      -Shell cmd -Command 'codex exec -m gpt-6.1-sol -s read-only -C . - < Prompts/review_x.md'
  powershell -File Tools/Devlog/Invoke-LoggedAgent.ps1 -Agent cursor-agent -Model composer-2.5-fast `
      -PromptFile Prompts/tarea.md -Purpose "implementar X" `
      -Command "& 'C:\Users\<user>\AppData\Local\cursor-agent\cursor-agent.ps1' -p --trust --workspace . (Get-Content Prompts/tarea.md -Raw)"

Que guarda en Devlog/raw/<MAQUINA>/<AAAA-MM-DD>/:
  agents.jsonl                 una linea "start" y una "end" por ejecucion
  agents/<runId>/prompt.md     copia del prompt
  agents/<runId>/output.txt    stdout+stderr completos del agente
  agents/<runId>/diffstat.txt  git diff --stat al terminar (que archivos toco)
#>
param(
    [Parameter(Mandatory = $true)][string]$Agent,
    [Parameter(Mandatory = $true)][string]$Command,
    [string]$Model = '',
    [string]$PromptFile = '',
    [string]$Purpose = '',
    # powershell (por defecto): sirve para CLIs .ps1 como cursor-agent y para "& 'ruta' args".
    # cmd: cuando el comando usa redireccion de entrada "< prompt.md" (codex exec - < archivo).
    [ValidateSet('powershell', 'cmd')][string]$Shell = 'powershell'
)

$ErrorActionPreference = 'Continue'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$root = (Get-Location).Path
$machine = if ($env:JAM_MACHINE) { $env:JAM_MACHINE } elseif ([Environment]::GetEnvironmentVariable('JAM_MACHINE','User')) { [Environment]::GetEnvironmentVariable('JAM_MACHINE','User') } else { $env:COMPUTERNAME }
$start = [DateTime]::UtcNow
$day = $start.ToString('yyyy-MM-dd')
$runId = '{0}-{1}-{2}' -f $start.ToString('HHmmss'), $Agent, ([guid]::NewGuid().ToString('N').Substring(0, 6))
$dayDir = Join-Path $root "Devlog\raw\$machine\$day"
$runDir = Join-Path $dayDir "agents\$runId"
[void][IO.Directory]::CreateDirectory($runDir)
$jsonl = Join-Path $dayDir 'agents.jsonl'

function Add-Line($obj) {
    [IO.File]::AppendAllText($jsonl, (($obj | ConvertTo-Json -Depth 6 -Compress) + "`n"), $utf8)
}

$head = ''
try { $head = (git rev-parse --short HEAD 2>$null) } catch {}
if ($PromptFile -and (Test-Path -LiteralPath $PromptFile)) {
    Copy-Item -LiteralPath $PromptFile -Destination (Join-Path $runDir 'prompt.md') -Force
}

Add-Line ([ordered]@{
    ts_utc = $start.ToString('o'); machine = $machine; agent = $Agent; event = 'start'
    run = $runId; model = $Model; purpose = $Purpose; prompt_file = $PromptFile
    command = $Command; git_head = $head
})

$outFile = Join-Path $runDir 'output.txt'
# La salida de los CLIs nativos se decodifica como UTF-8 (si no, PS 5.1 usa la pagina OEM
# y la n con tilde sale rota) y se guarda en UTF-8 (Tee-Object de PS 5.1 escribe UTF-16).
$prevOut = [Console]::OutputEncoding
[Console]::OutputEncoding = $utf8
$sw = New-Object System.IO.StreamWriter($outFile, $false, $utf8)
try {
    if ($Shell -eq 'cmd') {
        & cmd.exe /d /c "$Command 2>&1" | ForEach-Object { $s = [string]$_; $sw.WriteLine($s); Write-Host $s }
    } else {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[Console]::OutputEncoding=[Text.Encoding]::UTF8; `$OutputEncoding=[Text.Encoding]::UTF8; $Command; exit `$LASTEXITCODE" 2>&1 |
            ForEach-Object { $s = [string]$_; $sw.WriteLine($s); Write-Host $s }
    }
    $code = $LASTEXITCODE
} finally { $sw.Dispose(); [Console]::OutputEncoding = $prevOut }
$end = [DateTime]::UtcNow

$diffstat = ''
try { $diffstat = (git diff --stat 2>$null | Out-String); [IO.File]::WriteAllText((Join-Path $runDir 'diffstat.txt'), $diffstat, $utf8) } catch {}
$tail = ''
if (Test-Path $outFile) { $tail = ((Get-Content $outFile -Tail 15 -Encoding UTF8 -ErrorAction SilentlyContinue) -join "`n") }

Add-Line ([ordered]@{
    ts_utc = $end.ToString('o'); machine = $machine; agent = $Agent; event = 'end'
    run = $runId; exit_code = $code; seconds = [int]($end - $start).TotalSeconds
    output_tail = $tail; diffstat = $diffstat
})
exit $code
