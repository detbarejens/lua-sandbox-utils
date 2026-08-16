@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MSBUILD="
if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" (
  set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
  for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -find VC\Auxiliary\Build\vcvars64.bat 2^>nul`) do (
    set "VCVARS=%%I"
  )
)
if not defined VCVARS (
  echo ERROR: Could not find vcvars64.bat
  exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 exit /b 1

cl /nologo /O2 /std:c++20 /EHsc /W3 /Fe:FriendsKeyGen.exe FriendsKeyGen.cpp bcrypt.lib
if errorlevel 1 exit /b 1
del /q FriendsKeyGen.obj >nul 2>&1
echo Built %~dp0FriendsKeyGen.exe
exit /b 0
