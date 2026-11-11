# search.ps1 - lanzador de search.py con el Python que tiene numpy + requests.
# Python: variable JAM_PYTHON si existe; si no, el venv de Modly de la maquina de assets.
$py = if ($env:JAM_PYTHON) { $env:JAM_PYTHON } else { 'D:\Modly\extensions\hunyuan3d-mini\venv\Scripts\python.exe' }
try { [void](Invoke-WebRequest http://127.0.0.1:8766/health -UseBasicParsing -TimeoutSec 3) }
catch { Write-Host 'El servidor de embeddings (127.0.0.1:8766) no responde. Arrancar D:\Models\EmbeddingGemma-2\start_embedding_server.bat'; exit 1 }
[Console]::OutputEncoding = [Text.Encoding]::UTF8
& $py -I (Join-Path $PSScriptRoot 'search.py') @args
exit $LASTEXITCODE
