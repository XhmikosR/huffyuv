@ECHO OFF
rem ***************************************************************************
rem *
rem *  build_installer.bat
rem *    Batch file "wrapper" to create the installer
rem *
rem *  Copyright (C) 2011 XhmikosR, http://code.google.com/p/huffyuv/
rem *
rem *  This program is free software: you can redistribute it and/or modify
rem *  it under the terms of the GNU General Public License as published by
rem *  the Free Software Foundation, either version 3 of the License, or
rem *  (at your option) any later version.
rem *
rem *  This program is distributed in the hope that it will be useful,
rem *  but WITHOUT ANY WARRANTY; without even the implied warranty of
rem *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem *  GNU General Public License for more details.
rem *
rem *  You should have received a copy of the GNU General Public License
rem *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
rem *
rem ***************************************************************************


SETLOCAL ENABLEEXTENSIONS
CD /D %~dp0

REM You can set here the Inno Setup path if for example you have Inno Setup Unicode
REM installed and you want to use the ANSI Inno Setup which is in another location
IF NOT DEFINED InnoSetupPath SET "InnoSetupPath=H:\progs\thirdparty\isetup"

REM Check the building environment
CALL :SubDetectInnoSetup

CALL :SubInno %1


:END
ECHO. & ECHO.
ENDLOCAL
EXIT /B


:SubInno
ECHO.
TITLE Building huffyuv installer...
"%InnoSetupPath%\ISCC.exe" /Q "huffyuv_setup.iss" /D%1
IF %ERRORLEVEL% NEQ 0 (GOTO EndWithError) ELSE (ECHO Installer compiled successfully!)
EXIT /B


:SubDetectInnoSetup
REM Detect if we are running on 64bit Windows and use Wow6432Node since Inno Setup is
REM a 32-bit application, and set the registry key of Inno Setup accordingly
IF DEFINED PROGRAMFILES(x86) (
  SET "U_=HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall"
) ELSE (
  SET "U_=HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
)

IF DEFINED InnoSetupPath IF NOT EXIST "%InnoSetupPath%" (
  ECHO "%InnoSetupPath%" wasn't found on this machine! I will try to detect Inno Setup's path from the registry...
)

IF NOT EXIST "%InnoSetupPath%" (
  FOR /F "delims=" %%a IN (
    'REG QUERY "%U_%\Inno Setup 5_is1" /v "Inno Setup: App Path"2^>Nul^|FIND "REG_"') DO (
    SET "InnoSetupPath=%%a" & CALL :SubInnoSetupPath %%InnoSetupPath:*Z=%%)
)

IF NOT EXIST "%InnoSetupPath%" ECHO Inno Setup wasn't found! & GOTO EndWithError
EXIT /B


:SubInnoSetupPath
SET "InnoSetupPath=%*"
EXIT /B


:EndWithError
Title Compiling PerfmonBar [ERROR]
ECHO. & ECHO.
ECHO **ERROR: Build failed and aborted!**
PAUSE
ENDLOCAL
EXIT
