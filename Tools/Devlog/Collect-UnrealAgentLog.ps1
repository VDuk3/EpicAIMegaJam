<#
Collect-UnrealAgentLog.ps1 - Rescata del log del Editor todo lo que hicieron los agentes
DENTRO de Unreal: llamadas MCP (Claude u otro cliente), Unreal Assistant y Unreal Canvas.

Por que hace falta: esas acciones no pasan por los hooks de Claude Code. El Editor las
escribe en <Proyecto>/Saved/Logs/*.log (lineas LogModelContextProtocol: Running tool...).
El Editor rota el log en cada arranque (los viejos quedan como *-backup-*.log), asi que
corre esto AL FINAL DE CADA DIA y SIEMPRE antes de borrar Saved/.

Stop-EditorMcp.ps1 lo llama solo al cerrar el Editor (antes de que el log se rote).

Uso:  powershell -File Tools/Devlog/Collect-UnrealAgentLog.ps1 -ProjectDir <carpeta del .uproject>
Salida: <raiz del repo>/Devlog/raw/<MAQUINA>/unreal/<proyecto>__<log>.jsonl
        (la raiz del repo es la carpeta dos niveles arriba de este script, aunque el
        .uproject viva en una subcarpeta; idempotente: reescribe por log)
Categorias verificadas en el codigo de UE 5.9: LogModelContextProtocol, LogAIAssistant
(Unreal Assistant), LogUnrealCanvas* (Canvas). Que el contenido de cada linea sea util
para la bitacora (p. ej. si Canvas escribe el prompt) NO esta verificado.
#>
param(
    [string]$ProjectDir = '.',
    [string]$OutRoot = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [string]$Pattern = 'LogModelContextProtocol|LogAIAssistant|LogUnrealCanvas|LogToolsetRegistry'
)
$utf8 = New-Object System.Text.UTF8Encoding($false)
$machine = if ($env:JAM_MACHINE) { $env:JAM_MACHINE } elseif ([Environment]::GetEnvironmentVariable('JAM_MACHINE','User')) { [Environment]::GetEnvironmentVariable('JAM_MACHINE','User') } else { $env:COMPUTERNAME }
$logs = Join-Path $ProjectDir 'Saved\Logs'
if (-not (Test-Path $logs)) { Write-Host "No existe $logs"; exit 0 }
$proj = Split-Path (Resolve-Path $ProjectDir).Path -Leaf
$outDir = Join-Path $OutRoot "Devlog\raw\$machine\unreal"
[void][IO.Directory]::CreateDirectory($outDir)
$total = 0
foreach ($f in Get-ChildItem $logs -Filter *.log) {
    $lines = Select-String -Path $f.FullName -Pattern $Pattern -Encoding UTF8 -ErrorAction SilentlyContinue
    if (-not $lines) { continue }
    $sb = New-Object System.Text.StringBuilder
    foreach ($l in $lines) {
        $ts = ''
        if ($l.Line -match '^\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]') { $ts = $Matches[1] }
        $tool = ''
        if ($l.Line -match "tool: '([^']+)'") { $tool = $Matches[1] }
        $obj = [ordered]@{ log_ts = $ts; machine = $machine; agent = 'unreal-editor'; source_log = $f.Name; tool = $tool; line = $l.Line.Trim() }
        [void]$sb.Append(($obj | ConvertTo-Json -Compress)).Append("`n")
        $total++
    }
    [IO.File]::WriteAllText((Join-Path $outDir ($proj + '__' + $f.BaseName + '.jsonl')), $sb.ToString(), $utf8)
}
Write-Host "Lineas de agentes en Unreal recolectadas: $total -> $outDir"
