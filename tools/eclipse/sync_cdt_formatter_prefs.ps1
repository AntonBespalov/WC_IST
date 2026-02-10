param(
  [string]$ProfileXml = "tools/eclipse/EclipseCodeStyle.xml",
  [string]$OutputPrefs = ".settings/org.eclipse.cdt.core.prefs"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ProfileXml)) {
  throw "Не найден файл профиля форматтера: $ProfileXml"
}

[xml]$xml = Get-Content -Raw $ProfileXml

$lines = @("eclipse.preferences.version=1")
foreach ($setting in $xml.profiles.profile.setting) {
  if ($setting.id) {
    $lines += ("{0}={1}" -f $setting.id, $setting.value)
  }
}

Set-Content -Path $OutputPrefs -Value $lines -Encoding utf8
Write-Output "Синхронизировано: $OutputPrefs (строк: $($lines.Count))"
