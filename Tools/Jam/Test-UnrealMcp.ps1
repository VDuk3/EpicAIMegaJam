<#
Test-UnrealMcp.ps1 - Prueba de aceptacion del servidor MCP del Editor (UE 5.9).
Hace: initialize -> notifications/initialized -> tools/list -> list_toolsets ->
call_tool EditorToolset.LevelToolset.GetCurrentLevel. Imprime PASS/FAIL por paso.

Uso:  powershell -File Tools/Jam/Test-UnrealMcp.ps1 -Port 8059
#>
param([int]$Port = 8059)
$u = "http://127.0.0.1:$Port/mcp"
$fail = 0

function Invoke-Mcp($body, $sid) {
    $h = @{ 'Accept' = 'application/json, text/event-stream' }
    if ($sid) { $h['Mcp-Session-Id'] = $sid }
    $r = Invoke-WebRequest -Uri $u -Method Post -Headers $h -ContentType 'application/json' -Body ($body | ConvertTo-Json -Depth 8 -Compress) -UseBasicParsing -TimeoutSec 30
    $txt = if ($r.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($r.Content) } else { [string]$r.Content }
    if ($txt -match '^\s*(event:|data:)') { $txt = (($txt -split "`n") | Where-Object { $_ -like 'data:*' } | ForEach-Object { $_.Substring(5) }) -join '' }
    $json = $null
    if ($txt.Trim()) { $json = $txt | ConvertFrom-Json }
    return @{ Json = $json; Sid = $r.Headers['Mcp-Session-Id'] }
}
# Recien abierto, el Editor corre una "slow task" (shaders, carga) que bloquea las tools y
# responde "Not run: ... slow task". Se reintenta hasta 5 minutos.
function Invoke-McpTool($body, $sid) {
    $limit = (Get-Date).AddMinutes(5)
    while ($true) {
        $r = Invoke-Mcp $body $sid
        $t = [string]$r.Json.result.content[0].text
        if ($t -notmatch 'slow task' -or (Get-Date) -gt $limit) { return $r }
        Start-Sleep 5
    }
}
function Step($name, [scriptblock]$b) {
    try { $msg = & $b; Write-Host "PASS  $name  $msg" } catch { $script:fail++; Write-Host "FAIL  $name  $($_.Exception.Message)" }
}

$sid = $null
Step 'initialize' {
    $r = Invoke-Mcp @{ jsonrpc = '2.0'; id = 1; method = 'initialize'; params = @{ protocolVersion = '2025-06-18'; capabilities = @{}; clientInfo = @{ name = 'jam-probe'; version = '1' } } } $null
    $script:sid = $r.Sid
    $v = $r.Json.result.serverInfo.version
    if (-not $v) { throw 'sin serverInfo' }
    "server=$($r.Json.result.serverInfo.name) version=$v project=$($r.Json.result._meta.'com.epicgames.unrealengine/instance'.projectName)"
}
Step 'initialized' { [void](Invoke-Mcp @{ jsonrpc = '2.0'; method = 'notifications/initialized' } $sid); 'ok' }
Step 'tools/list' {
    $r = Invoke-Mcp @{ jsonrpc = '2.0'; id = 2; method = 'tools/list' } $sid
    $n = @($r.Json.result.tools).Count; if ($n -lt 1) { throw 'cero tools' }; "tools=$n ($((@($r.Json.result.tools) | ForEach-Object name) -join ', '))"
}
Step 'list_toolsets' {
    $r = Invoke-McpTool @{ jsonrpc = '2.0'; id = 3; method = 'tools/call'; params = @{ name = 'list_toolsets'; arguments = @{} } } $sid
    $t = $r.Json.result.content[0].text
    $n = @(($t -split "`n") | Where-Object { $_ -match '^- ' }).Count
    if ($n -lt 10) { throw "solo $n toolsets" }; "toolsets~$n"
}
Step 'call_tool GetCurrentLevel' {
    $r = Invoke-McpTool @{ jsonrpc = '2.0'; id = 4; method = 'tools/call'; params = @{ name = 'call_tool'; arguments = @{ toolset_name = 'EditorToolset.LevelToolset'; tool_name = 'GetCurrentLevel'; arguments = @{} } } } $sid
    if ($r.Json.result.isError) { throw $r.Json.result.content[0].text }
    "level=$($r.Json.result.structuredContent.returnValue)"
}
if ($fail -eq 0) { Write-Host 'RESULTADO: PASS'; exit 0 } else { Write-Host "RESULTADO: FAIL ($fail pasos)"; exit 1 }
