@echo off
setlocal

rem Запуск PowerShell-скрипта нормализации EOL (CRLF) для Core/Drivers.
set "SCRIPT=%~dp0fix_eol_crlf.ps1"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%"
if errorlevel 1 (
  echo [fix_eol_crlf] ERROR: PowerShell script failed with code %errorlevel%.
  endlocal
  exit /b 1
)

endlocal
exit /b 0
