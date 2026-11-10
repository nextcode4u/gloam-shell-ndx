@echo off
setlocal
if not exist "%~dp0download-addons.ps1" (
  echo Keep this launcher beside download-addons.ps1 and downloads.json.
  pause
  exit /b 1
)
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0download-addons.ps1" -Interactive
set "installer_result=%errorlevel%"
if not "%installer_result%"=="0" pause
exit /b %installer_result%
