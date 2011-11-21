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
SET "InnoSetupPath=H:\progs\thirdparty\isetup"

rem Check the building environment
CALL :SubDetectInnoSetup

IF NOT EXIST %InnoSetupPath% (
  ECHO. & ECHO.
  ECHO Inno Setup not found!
  GOTO END
)

CALL :SubInno %1


:END
ECHO. & ECHO.
ENDLOCAL
EXIT /B


:SubInno
ECHO.
TITLE Building huffyuv installer...
"%InnoSetupPath%\iscc.exe" /Q "huffyuv_setup.iss" /D%1
IF %ERRORLEVEL% NEQ 0 (ECHO Build failed! & GOTO END) ELSE (ECHO Installer compiled successfully!)
EXIT /B


:SubDetectInnoSetup
REM Detect if we are running on 64bit WIN and use Wow6432Node, and set the path
REM of Inno Setup accordingly
IF DEFINED PROGRAMFILES(x86) (
  SET "U_=HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall"
) ELSE (
  SET "U_=HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
)

IF NOT DEFINED InnoSetupPath (
  FOR /F "delims=" %%a IN (
    'REG QUERY "%U_%\Inno Setup 5_is1" /v "Inno Setup: App Path"2^>Nul^|FIND "REG_"') DO (
    SET "InnoSetupPath=%%a" & CALL :SubInnoSetupPath %%InnoSetupPath:*Z=%%)
)

IF NOT EXIST "%InnoSetupPath%" ECHO Inno Setup wasn't found! & GOTO END
EXIT /B


:SubInnoSetupPath
SET InnoSetupPath=%*
EXIT /B
