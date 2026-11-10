@echo off
setlocal
rem From PowerShell, run .\Open-ModynToolbox.cmd as a file.
echo [ModynToolbox] Opening latest portable build. Source development uses Run-Dev.cmd.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\open-release.ps1"
set "modynExitCode=%errorlevel%"
if not "%modynExitCode%"=="0" pause
exit /b %modynExitCode%
