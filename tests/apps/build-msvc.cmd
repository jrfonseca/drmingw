@echo off

setlocal

if not "%VS90COMNTOOLS%"=="" set generator=Visual Studio 9 2008
if not "%VS100COMNTOOLS%"=="" set generator=Visual Studio 10
if not "%VS110COMNTOOLS%"=="" set generator=Visual Studio 11
if not "%VS120COMNTOOLS%"=="" set generator=Visual Studio 12

cmake -G "%generator%" -H%~dp0 -Bbuild\msvc32
if errorlevel 1 exit /b %ERRORLEVEL%
cmake --build build\msvc32 --config Debug -- /verbosity:minimal
if errorlevel 1 exit /b %ERRORLEVEL%

cmake -G "%generator% Win64" -H%~dp0 -Bbuild\msvc64
if errorlevel 1 exit /b %ERRORLEVEL%
cmake --build build\msvc64 --config Debug -- /verbosity:minimal
if errorlevel 1 exit /b %ERRORLEVEL%

endlocal
