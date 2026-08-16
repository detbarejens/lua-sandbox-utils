@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not exist "%~dp0FriendsKeyGen.exe" (
  call "%~dp0Build-FriendsKeyGen.bat"
  if errorlevel 1 exit /b 1
)

"%~dp0FriendsKeyGen.exe" %*
exit /b %ERRORLEVEL%
