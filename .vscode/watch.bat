@echo off
echo Installing Watchdog Service...

REM Check if service exists
sc query Watchdog > nul
if %ERRORLEVEL% EQU 0 (
    echo Stopping existing service...
    sc stop Watchdog > nul
    timeout /t 2 > nul
    
    echo Removing existing service...
    sc delete Watchdog > nul
    timeout /t 2 > nul
)

echo Creating service...
sc create Watchdog binPath= "%~dp0watchdog.exe" type= own start= auto obj= "LocalSystem" DisplayName= "Health Monitor Watchdog"
if %ERRORLEVEL% NEQ 0 (
    echo Failed to create service - Error code: %ERRORLEVEL%
    goto :error
)

echo Setting privileges...
sc privs Watchdog SeAssignPrimaryTokenPrivilege/SeIncreaseQuotaPrivilege
if %ERRORLEVEL% NEQ 0 (
    echo Failed to set privileges - Error code: %ERRORLEVEL%
    goto :error
)

echo Setting service description...
sc description Watchdog "Monitors healthuse.exe process in user session"

echo Starting service...
sc start Watchdog
if %ERRORLEVEL% NEQ 0 (
    echo Failed to start service - Error code: %ERRORLEVEL%
    sc query Watchdog
    goto :error
)

echo Verifying service status...
sc query Watchdog
if %ERRORLEVEL% NEQ 0 (
    echo Failed to query service status
    goto :error
)

echo.
echo Installation completed.
goto :end

:error
echo.
echo ERROR: Service installation failed!
pause
exit /b 1

:end
pause
exit /b 0