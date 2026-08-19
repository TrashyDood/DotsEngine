@echo off
setlocal
cd /d "%~dp0"

REM ============================================================
REM  Usage: compare_traces.bat trace_a.tracy trace_b.tracy
REM    Both filenames are relative to the traces\ folder.
REM    Opens trace_a directly in Tracy profiler; trace_b is left
REM    ready to select via Statistics -> Compare inside the GUI,
REM    since tracy-profiler.exe has no CLI flag for this.
REM ============================================================

if "%~1"=="" (
    echo Usage: compare_traces.bat trace_a.tracy trace_b.tracy
    pause
    exit /b 1
)
if "%~2"=="" (
    echo Usage: compare_traces.bat trace_a.tracy trace_b.tracy
    pause
    exit /b 1
)

set "TRACE_A=%~dp0traces\%~1"
set "TRACE_B=%~dp0traces\%~2"

if not exist "%TRACE_A%" (
    echo.
    echo Could not find: %TRACE_A%
    pause
    exit /b 1
)
if not exist "%TRACE_B%" (
    echo.
    echo Could not find: %TRACE_B%
    pause
    exit /b 1
)

echo.
echo Opening trace A in Tracy profiler:
echo   %TRACE_A%
echo.
echo Once it loads, go to Statistics -^> Compare and select trace B:
echo   %TRACE_B%
echo.

start "" "%~dp0Tracy 0.14.0\tracy-profiler.exe" "%TRACE_A%"

pause
endlocal
