# JamLogCopy.ps1 - Funcion compartida por los Collect-*.ps1 (se carga con dot-source).
#
# Copy-JamLogSlim: copia un registro de agente linea a linea quitando los datos binarios en
# base64 (capturas y renders pegados en el chat). Sin esto, una sola sesion de Codex de ASSETS
# pesaba 110 MB (2026-11-12) y GitHub rechaza archivos de mas de 100 MB. El texto, los prompts
# y las llamadas a herramientas quedan intactos; cada imagen se sustituye por
# "[base64 omitted: N chars]". Las imagenes originales siguen en la maquina de origen.
#  - Lee con FileShare.ReadWrite (el agente puede estar escribiendo).
#  - Deja en la copia la fecha de modificacion del original: asi se detecta si cambio.
function Copy-JamLogSlim([string]$Source, [string]$Dest) {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $rxUri = New-Object regex('data:[a-z]+/[A-Za-z0-9.+\-]+;base64,[A-Za-z0-9+/=]{200,}', 'Compiled')
    $rxData = New-Object regex('"data":"[A-Za-z0-9+/=]{1000,}"', 'Compiled')
    $evUri = [System.Text.RegularExpressions.MatchEvaluator] { param($m) $i = $m.Value.IndexOf(','); $m.Value.Substring(0, $i + 1) + "[base64 omitted: $($m.Length - $i - 1) chars]" }
    $evData = [System.Text.RegularExpressions.MatchEvaluator] { param($m) '"data":"[base64 omitted: ' + ($m.Length - 10) + ' chars]"' }
    [void][IO.Directory]::CreateDirectory((Split-Path $Dest -Parent))
    $tmp = $Dest + '.part'
    $fs = New-Object IO.FileStream($Source, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $r = New-Object IO.StreamReader($fs, $utf8)
        $w = New-Object IO.StreamWriter($tmp, $false, $utf8)
        try {
            while ($null -ne ($line = $r.ReadLine())) {
                if ($line.Length -gt 2000) { $line = $rxData.Replace($rxUri.Replace($line, $evUri), $evData) }
                $w.Write($line); $w.Write("`n")
            }
        } finally { $w.Dispose(); $r.Dispose() }
    } finally { $fs.Dispose() }
    Move-Item -LiteralPath $tmp -Destination $Dest -Force
    (Get-Item -LiteralPath $Dest).LastWriteTimeUtc = (Get-Item -LiteralPath $Source).LastWriteTimeUtc
}

# Test-JamLogCurrent: true si la copia ya corresponde al original (misma fecha de modificacion).
function Test-JamLogCurrent([IO.FileInfo]$SourceFile, [string]$Dest) {
    if (-not (Test-Path -LiteralPath $Dest)) { return $false }
    return ((Get-Item -LiteralPath $Dest).LastWriteTimeUtc -eq $SourceFile.LastWriteTimeUtc)
}
