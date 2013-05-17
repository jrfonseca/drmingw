@echo off

setlocal

set KeyName=HKLM\Software\Microsoft\Windows NT\CurrentVersion\AeDebug
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" set KeyName=HKLM\Software\Wow6432Node\Microsoft\Windows NT\CurrentVersion\AeDebug

goto option_%1
goto option_status

:option_install
reg add "%KeyName%" /v "Auto" /t REG_SZ /d "1" /f
reg add "%KeyName%" /v "Debugger" /t REG_SZ /d "%CD%\drmingw.exe -p %%ld -e %%ld" /f
goto option_status

:option_uninstall
:option_reset
reg add "%KeyName%" /v "Auto" /t REG_SZ /d "0" /f
reg delete "%KeyName%" /v "Debugger" /f
goto option_status

:option_
:option_status
reg query "%KeyName%" /v "Auto"
reg query "%KeyName%" /v "Debugger"

endlocal
