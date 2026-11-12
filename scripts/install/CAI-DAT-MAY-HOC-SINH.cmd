@echo off
rem ======================================================================
rem  ITLabControl - cai dat may HOC SINH
rem
rem  Cach dung:
rem    - Chuot phai file nay -> "Run as administrator", script se hoi ten phong.
rem    - Cai hang loat (khong hoi): CAI-DAT-MAY-HOC-SINH.cmd "Phong 201"
rem
rem  Thu muc nay can co: itlabcontrol-*-setup.exe va it nhat mot file khoa
rem  *_public_key.pem: khoa cua phong (vd. phong-201_public_key.pem, tao tren
rem  may giao vien trong Configurator - Phong may), co the them khoa quan tri
rem  cua bo phan IT. Chi may giao vien co cac khoa nay dieu khien duoc may HS.
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

set SETUP=
for %%f in (itlabcontrol-*-setup.exe) do set SETUP=%%f
if "%SETUP%"=="" (
  echo [LOI] Khong tim thay file itlabcontrol-*-setup.exe trong thu muc nay.
  pause
  exit /b 1
)
set KEYCOUNT=0
for %%k in (*_public_key.pem) do set /a KEYCOUNT+=1
if "%KEYCOUNT%"=="0" (
  echo [LOI] Khong tim thay file khoa *_public_key.pem trong thu muc nay.
  echo       Tao khoa tren may giao vien: Configurator - Phong may - "Tao khoa cho phong nay".
  pause
  exit /b 1
)

rem ----- ten phong: lay tu tham so, neu khong co thi hoi -----
set "ROOM=%~1"
if "%ROOM%"=="" (
  echo.
  echo Nhap TEN PHONG cua may nay, vi du: Phong 201
  echo   - Khong phan biet hoa/thuong, co dau hay khong dau.
  echo   - Bam Enter de bo qua, co the dat sau trong Configurator hoac tu may giao vien.
  set /p "ROOM=Ten phong: "
)

echo.
echo [1/5] Dang cai %SETUP% (chi phan hoc sinh, co driver khoa ban phim/chuot)...
start /wait "" "%SETUP%" /S /NoMaster
if not exist "C:\Program Files\ITLabControl\veyon-cli.exe" (
  echo [LOI] Cai dat that bai.
  pause
  exit /b 1
)

cd /d "C:\Program Files\ITLabControl"
echo [2/5] Nhap khoa xac thuc cua giao vien (chi cac khoa co trong thu muc nay)...
rem Go cac khoa cong khai cu de chi may giao vien co khoa trong thu muc nay dieu khien duoc may nay
for /d %%d in ("%ProgramData%\ITLabControl\keys\public\*") do veyon-cli.exe authkeys delete "%%~nxd/public" >nul 2>&1
setlocal enabledelayedexpansion
for %%k in ("%~dp0*_public_key.pem") do (
  set "KEYNAME=%%~nk"
  set "KEYNAME=!KEYNAME:_public_key=!"
  echo        - khoa !KEYNAME!
  veyon-cli.exe authkeys import "!KEYNAME!/public" "%%~fk"
)
endlocal

echo [3/5] Bat xac thuc bang khoa...
veyon-cli.exe config set Authentication/Method 1

echo [4/5] Dat ten phong...
if not "%ROOM%"=="" (
  veyon-cli.exe config set ITLab/Room "%ROOM%"
) else (
  echo        Bo qua - chua dat ten phong.
)

if exist "%~dp0man-hinh-khoa.png" (
  echo        Anh man hinh khoa: man-hinh-khoa.png
  copy /y "%~dp0man-hinh-khoa.png" "%ProgramData%\ITLabControl\lock-screen.png" >nul
)

echo [5/5] Khoi dong lai dich vu...
veyon-cli.exe service restart
veyon-cli.exe service status

echo.
echo ======================================================================
echo  XONG. Thong tin may nay:
echo    Ten may : %COMPUTERNAME%
echo    Phong   : %ROOM%
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr /c:"IPv4"') do echo    Dia chi IP:%%a
echo.
echo  Tren may giao vien: mo "Quan ly phong" -^> "Tu do may" de them may nay.
echo  Nen KHOI DONG LAI may de driver khoa ban phim/chuot co hieu luc.
echo ======================================================================
if "%~1"=="" pause
