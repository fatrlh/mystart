@echo off
echo Installing Watchdog Service...

REM Remove existing service if exists
sc query Watchdog > nul && (
    sc stop Watchdog > nul
    timeout /t 2 > nul
    sc delete Watchdog > nul
    timeout /t 2 > nul
)

REM Create and start service
sc create Watchdog binPath= "%~dp0watchdog.exe" type= own start= auto obj= "LocalSystem" || goto :error
sc start Watchdog || goto :error
sc query Watchdog

echo Installation completed.
pause
exit /b 0

:error
echo ERROR: Service installation failed - Error code: %ERRORLEVEL%
pause
exit /b 1