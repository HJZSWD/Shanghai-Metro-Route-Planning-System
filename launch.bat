@echo off
chcp 65001 >nul 2>nul

set ROOT=%~dp0
cd /d "%ROOT%"

title Shanghai Metro - Smart Route Planner

echo ============================================
echo    Shanghai Metro Route Planner
echo    ShangHai Metro Route Planning System
echo ============================================
echo.

rem ===== 1. Start HTTP Server =====
echo [1/3] Starting Web Server...
cd /d "%ROOT%frontend\web"

set PYTHON_CMD=
where pythonw.exe >nul 2>nul
if %errorlevel% equ 0 (
    set PYTHON_CMD=pythonw.exe
) else (
    where python.exe >nul 2>nul
    if %errorlevel% equ 0 (
        set PYTHON_CMD=python.exe
    )
)

if "%PYTHON_CMD%"=="" (
    echo [ERROR] Python not found. Please install Python 3 first.
    echo         Download: https://www.python.org/downloads/
    echo.
    echo You can manually start the frontend:
    echo   cd frontend\web
    echo   python -m http.server 8080
    echo.
    goto SKIP_SERVER
)

start /B "" %PYTHON_CMD% -m http.server 8080 >nul 2>nul
echo  [OK] HTTP Server started on port 8080

timeout /t 2 /nobreak >nul 2>nul

rem ===== 2. Open Browser =====
echo [2/3] Opening browser...
start http://localhost:8080
echo  [OK] Browser launched

:SKIP_SERVER
cd /d "%ROOT%"

rem ===== 3. Start backend service =====
echo [3/3] Starting backend service...

if exist "%ROOT%backend-service.exe" (
    start /B "" "%ROOT%backend-service.exe" --hidden >nul 2>nul
    echo  [OK] backend-service.exe running in background (hidden console)
) else (
    echo [WARN] backend-service.exe not found
    echo        Only frontend is available.
    echo        To compile: g++ backend/src/main.cpp ... -o backend-service.exe ...
)

echo.
echo ============================================
echo  [OK] All services started!
echo       Frontend: http://localhost:8080
echo       Close this window to stop all services.
echo ============================================
echo.

pause
