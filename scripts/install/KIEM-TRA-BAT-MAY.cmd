@echo off
rem ======================================================================
rem  ITLabControl - kiem tra va bat tinh nang "Bat may" (Wake-on-LAN)
rem
rem  Chay tren MAY HOC SINH khi may dang bat:
rem    Chuot phai file nay -> "Run as administrator".
rem  Ket qua duoc ghi vao file KIEM-TRA-BAT-MAY-<ten may>.txt canh file nay.
rem ======================================================================
setlocal
chcp 65001 >nul
cd /d "%~dp0"

net session >nul 2>&1
if errorlevel 1 (
  echo [LOI] Hay chuot phai file nay va chon "Run as administrator".
  pause
  exit /b 1
)

set "OUT=%~dp0KIEM-TRA-BAT-MAY-%COMPUTERNAME%.txt"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0kiem-tra-bat-may.ps1" -OutFile "%OUT%"

echo.
echo Ket qua da ghi vao: %OUT%
echo.
set /p "FIX=Bat Wake-on-LAN cho card mang va tat Fast startup ngay bay gio? (C/K): "
if /i "%FIX%"=="C" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0kiem-tra-bat-may.ps1" -Fix -OutFile "%OUT%"
  echo.
  echo Da bat. Hay KHOI DONG LAI may mot lan, sau do Shut down va thu "Bat may" tu may giao vien.
)
pause
