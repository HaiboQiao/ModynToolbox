@echo off
setlocal
rem From PowerShell, run .\Run-Dev.cmd as a file.
echo [ModynToolbox] Building and running current source.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\dev.ps1" run
set "modynExitCode=%errorlevel%"
if not "%modynExitCode%"=="0" pause
exit /b %modynExitCode%
