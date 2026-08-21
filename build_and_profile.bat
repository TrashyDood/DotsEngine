@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

REM ============================================================
REM  Usage: build_and_profile.bat [name] [duration] [scene]
REM    name     - base trace name (optional, prompted if omitted)
REM    duration - capture duration in seconds (optional, prompted if omitted)
REM    scene    - optional arg passed straight through to DotsEngine.exe
REM  All three can also be supplied purely via CLI for scripted/batch runs.
REM ============================================================

set "OUTPUT=%~1"
set "DURATION=%~2"
set "SCENE=%~3"

if "%OUTPUT%"=="" set /p OUTPUT=Enter trace filename:
if "%DURATION%"=="" set /p DURATION=Enter duration in seconds:

if "%OUTPUT%"=="" (
    echo.
    echo Trace filename cannot be empty.
    pause
    exit /b 1
)

echo %DURATION%|findstr /r "^[1-9][0-9]*$" >nul
if errorlevel 1 (
    echo.
    echo Duration must be a positive whole number of seconds.
    pause
    exit /b 1
)

REM --- Set up traces archive folder ---
if not exist "%~dp0traces" mkdir "%~dp0traces"

REM --- Git short commit hash, with -dirty suffix if there are uncommitted changes ---
set "GITHASH="
where git >nul 2>&1
if not errorlevel 1 (
    for /f %%h in ('git rev-parse --short HEAD 2^>nul') do set "GITHASH=%%h"
    if defined GITHASH (
        git diff --quiet 2>nul
        if errorlevel 1 set "GITHASH=!GITHASH!-dirty"
        git diff --cached --quiet 2>nul
        if errorlevel 1 set "GITHASH=!GITHASH!-dirty"
    )
)
set "HASHTAG="
if defined GITHASH set "HASHTAG=_!GITHASH!"

REM --- Auto-increment: count existing traces with this base name (ignoring timestamp/hash) ---
set /a N=0
for %%f in ("%~dp0traces\%OUTPUT%_*.tracy") do (
    set /a N+=1
)
set /a N+=1

set "COUNTER=%N%"
if %N% lss 100 set "COUNTER=0%N%"
if %N% lss 10 set "COUNTER=00%N%"

set "BASENAME=%OUTPUT%_%COUNTER%%HASHTAG%"

set "LOGFILE=%~dp0traces\%BASENAME%_build.log"

REM --- Locate MSBuild via vswhere instead of a hardcoded path ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo.
    echo Could not find vswhere.exe - is Visual Studio installed?
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe`) do (
    set "MSBUILD=%%i"
)

if not defined MSBUILD (
    echo.
    echo Could not locate MSBuild.exe via vswhere.
    pause
    exit /b 1
)

echo.
echo Using MSBuild: !MSBUILD!
echo.
echo Building DotsEngine...
echo Build log: %LOGFILE%
"!MSBUILD!" DotsEngine.sln -noautoresponse /p:Configuration=Release /p:Platform=x64 > "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo.
    echo Build failed. See log: %LOGFILE%
    type "%LOGFILE%"
    pause
    exit /b 1
)

echo.
echo Build successful.

REM --- Start Tracy capture right before launching the app, not before the build ---
set "TRACEFILE=%~dp0traces\%BASENAME%.tracy"
echo.
echo Starting Tracy capture (%DURATION%s, output "%TRACEFILE%")...
start "" /b "%~dp0Tracy 0.14.0\tracy-capture.exe" -a 127.0.0.1 -o "%TRACEFILE%" -s %DURATION%

echo Waiting 2 seconds for capture to arm...
timeout /t 2 /nobreak >nul

echo.
if "%SCENE%"=="" (
    echo Running DotsEngine...
) else (
    echo Running DotsEngine with scene arg: %SCENE%
)
cd /d "%~dp0x64\Release"
start "" /b DotsEngine.exe %SCENE%

echo.
echo Waiting for Tracy capture to finish...
:waitloop
tasklist /fi "imagename eq tracy-capture.exe" 2>nul | find /i "tracy-capture.exe" >nul
if not errorlevel 1 (
    timeout /t 1 /nobreak >nul
    goto waitloop
)

echo.
echo Capture finished, closing DotsEngine...
taskkill /im DotsEngine.exe /f >nul 2>&1

echo.
echo Opening trace in Tracy profiler...
start "" "%~dp0Tracy 0.14.0\tracy-profiler.exe" "%TRACEFILE%"

echo.
echo ============================================================
echo  Trace saved: traces\%BASENAME%.tracy
echo ============================================================

pause
endlocal
