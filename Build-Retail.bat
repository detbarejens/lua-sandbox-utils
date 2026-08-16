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

set "VER=1"
if exist "%~dp0tools\KeyGen\friends-version.txt" set /p VER=<"%~dp0tools\KeyGen\friends-version.txt"
if "%VER%"=="" set "VER=1"

taskkill /F /IM HZ-Retail.exe >nul 2>&1
mkdir "%~dp0Client" 2>nul

echo Building HZ-Retail (version %VER%)...
"%MSBUILD%" "%~dp0HZ.sln" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:HzDev=false /p:FriendsBuildVersion=%VER% /m:1 /v:minimal /nologo
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)

copy /Y "%~dp0build\release\HZ-Retail.exe" "%~dp0Client\HZ-Retail.exe"
echo.
echo RETAIL BUILD SUCCESS
echo   %~dp0Client\HZ-Retail.exe
echo Issue keys with: tools\KeyGen\FriendsKeyGen.bat key --note "name"
exit /b 0
