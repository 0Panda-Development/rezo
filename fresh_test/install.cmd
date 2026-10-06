@echo off
setlocal enabledelayedexpansion
title Rezo Installer
cd /d "%~dp0"

echo ========================================
echo Rezo Installer
echo ========================================
echo Working dir: %CD%
echo.

>nul 2>&1 net session
if %errorlevel% neq 0 (
    echo [INFO] Requesting administrator rights...
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    echo [INFO] Launched elevated instance. This window can close.
    pause
    exit /b
)

echo [1/3] Trusting Rezo publisher certificate...
echo Checking for Rezo-selfsign.cer...
if not exist Rezo-selfsign.cer (
    echo [ERROR] Rezo-selfsign.cer NOT FOUND in %CD%
    echo Files in folder:
    dir /b
    pause
    exit /b 1
)

echo Found certificate at: %CD%\Rezo-selfsign.cer
echo Running certutil...
certutil -addstore -f Root Rezo-selfsign.cer
echo After certutil, errorlevel=%errorlevel%
if %errorlevel% equ 0 (
    echo [OK] Certificate installed/trusted.
) else (
    echo [WARN] Certificate install returned error - may already be trusted.
)
echo After certificate check

echo.
echo [2/3] Enabling app sideloading...
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock" /t REG_DWORD /f /v AllowAllTrustedApps /d 1 >nul
echo [OK] Sideloading enabled.

echo.
echo [3/3] Installing Rezo...

rem Find latest MSIX
set "MSIX="
for /f "delims=" %%f in ('dir /b /o:-n Rezo-*.msix 2^>nul') do (
    set "MSIX=%%f"
    goto :found_msix
)
:found_msix
if not defined MSIX (
    echo [ERROR] No Rezo-*.msix found in %CD%
    echo Files in folder:
    dir /b
    pause
    exit /b 1
)
echo Found MSIX: %MSIX%

rem Extract version from MSIX filename (Rezo-1.5.3.msix -> 1.5.3)
for /f "tokens=2 delims=-" %%a in ("%MSIX%") do set "MSIX_VER=%%a"
for /f "tokens=1-3 delims=." %%x in ("%MSIX_VER%") do set "MSIX_VER=%%x.%%y.%%z"
echo MSIX version: %MSIX_VER%

rem Check installed version
set "INSTALLED_VER="
for /f "tokens=2 delims=_" %%v in ('powershell -NoProfile -Command "Get-AppxPackage -Name 'Pandajupiter.Rezo' | Select-Object -ExpandProperty Version" 2^>nul') do (
    set "INSTALLED_VER=%%v"
)
if defined INSTALLED_VER (
    echo Installed version: %INSTALLED_VER%
    
    powershell -NoProfile -Command "if ([version]'%INSTALLED_VER%' -ge [version]'%MSIX_VER%') { exit 0 } else { exit 1 }"
    if errorlevel 1 (
        echo Updating from %INSTALLED_VER% to %MSIX_VER%...
        powershell -NoProfile -Command "Remove-AppxPackage -Package (Get-AppxPackage -Name 'Pandajupiter.Rezo').PackageFullName"
        echo [OK] Old version removed.
    ) else (
        echo [OK] Already have version %INSTALLED_VER% or newer. Nothing to do.
        pause
        exit /b 0
    )
) else (
    echo No existing installation found.
)

echo Installing %MSIX% ...
powershell -NoProfile -Command "Add-AppxPackage -Path '%CD%\%MSIX%' -ForceApplicationShutdown"
if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] Rezo installed successfully!
) else (
    echo.
    echo [FAILED] Installation failed. Error code: %errorlevel%
    echo Try running manually: Add-AppxPackage -Path '%CD%\%MSIX%' -ForceApplicationShutdown
)
echo.
echo Press any key to exit...
pause >nul