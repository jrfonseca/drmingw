@echo off

setlocal

if not "%VS90COMNTOOLS%"=="" set generator=Visual Studio 9 2008
if not "%VS100COMNTOOLS%"=="" set generator=Visual Studio 10
if not "%VS110COMNTOOLS%"=="" set generator=Visual Studio 11
if not "%VS120COMNTOOLS%"=="" set generator=Visual Studio 12

cmake -G "%generator%" -H. -Bbuild\x86
cmake --build build\x86 --config Debug -- /verbosity:minimal

cmake -G "%generator% Win64" -H. -Bbuild\x64
cmake --build build\x64 --config Debug -- /verbosity:minimal

endlocal
