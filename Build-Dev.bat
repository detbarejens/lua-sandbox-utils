@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MSBUILD="
if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" (
  set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
)
if not defined MSBUILD if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
  set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
)
if not defined MSBUILD (
  echo ERROR: Could not find MSBuild.exe
  exit /b 1
)

taskkill /F /IM HZ-Dev.exe >nul 2>&1
mkdir "%~dp0Client" 2>nul
mkdir "%~dp0Data" 2>nul

echo Building HZ-Dev...
"%MSBUILD%" "%~dp0HZ.sln" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:HzDev=true /m:1 /v:minimal /nologo
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)

copy /Y "%~dp0build\release\HZ-Dev.exe" "%~dp0Client\HZ-Dev.exe"
echo.
echo DEV BUILD SUCCESS
echo   %~dp0Client\HZ-Dev.exe
exit /b 0
