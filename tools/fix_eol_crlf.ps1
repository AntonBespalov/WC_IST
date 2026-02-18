param(
  [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

# Скрипт приводит окончания строк к CRLF только для текстовых файлов
# в папках Core и Drivers. Бинарные файлы пропускаются (нулевой байт).

$includeDirs = @(
  "Core",
  "Drivers"
)

function Is-BinaryFile([byte[]]$bytes) {
  return ($bytes -contains 0)
}

function Get-TextEncoding([byte[]]$bytes) {
  if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
    return [Text.UTF8Encoding]::new($true)
  }
  return [Text.UTF8Encoding]::new($false)
}

function Normalize-ToCrlf([string]$text) {
  $text = $text -replace "`r`n", "`n"
  $text = $text -replace "`r", "`n"
  return ($text -replace "`n", "`r`n")
}

foreach ($dir in $includeDirs) {
  $path = Join-Path $Root $dir
  if (-not (Test-Path $path)) { continue }

  Get-ChildItem -Path $path -Recurse -File -Force | ForEach-Object {
    $bytes = [IO.File]::ReadAllBytes($_.FullName)
    if (Is-BinaryFile $bytes) { return }

    $enc = Get-TextEncoding $bytes
    $text = $enc.GetString($bytes)
    $normalized = Normalize-ToCrlf $text

    if ($normalized -ne $text) {
      [IO.File]::WriteAllBytes($_.FullName, $enc.GetBytes($normalized))
    }
  }
}
