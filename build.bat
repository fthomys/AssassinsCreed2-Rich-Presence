@echo off
setlocal
cd /d "%~dp0"

if not exist third_party\discord-rpc\include\discord_rpc.h (
    git submodule update --init --recursive || exit /b 1
)

set "VSDIR=C:\Program Files\Microsoft Visual Studio\18\Community"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto build
for /f "usebackq delims=" %%i in (`call "%%VSWHERE%%" -latest -products * -property installationPath`) do set "VSDIR=%%i"

:build
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1 || exit /b 1

set RPC=third_party\discord-rpc\src
set SOURCES=src\*.cpp %RPC%\discord_rpc.cpp %RPC%\rpc_connection.cpp %RPC%\serialization.cpp %RPC%\connection_win.cpp %RPC%\discord_register_win.cpp
set INCLUDES=/I third_party\discord-rpc\include /I third_party\rapidjson\include
set DEFINES=/D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX

if not exist build\obj mkdir build\obj
cl /nologo /O2 /MT /utf-8 /EHsc /std:c++17 /W3 /LD %INCLUDES% %DEFINES% %SOURCES% /Fobuild\obj\ /Fe:build\DiscordPresenceAC2.asi /link /DLL advapi32.lib user32.lib || exit /b 1
copy /y dist\DiscordPresenceAC2.ini build\ >nul

if "%~1"=="install" (
    if not exist "%~2\scripts" (echo Game folder not found: %~2 & exit /b 1)
    copy /y build\DiscordPresenceAC2.asi "%~2\scripts\" >nul || (echo Could not install, is the game still running? & exit /b 1)
    if not exist "%~2\scripts\DiscordPresenceAC2.ini" copy /y dist\DiscordPresenceAC2.ini "%~2\scripts\" >nul
    echo Installed to %~2\scripts
)
echo Build finished: build\DiscordPresenceAC2.asi
