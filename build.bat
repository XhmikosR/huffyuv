@ECHO OFF
rem ***************************************************************************
rem *
rem * huffyuv
rem *
rem * build.bat
rem *   Batch file used to build huffyuv with MSVC2010
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

rem Check the building environment
IF NOT DEFINED VS100COMNTOOLS CALL :SUBMSG "ERROR" "Visual Studio 2010 NOT FOUND!"


rem Check for the help switches
IF /I "%~1"=="help"   GOTO SHOWHELP
IF /I "%~1"=="/help"  GOTO SHOWHELP
IF /I "%~1"=="-help"  GOTO SHOWHELP
IF /I "%~1"=="--help" GOTO SHOWHELP
IF /I "%~1"=="/?"     GOTO SHOWHELP


rem Check for the first switch
IF "%~1" == "" (
  SET "BUILDTYPE=Build"
) ELSE (
  IF /I "%~1" == "Build"     SET "BUILDTYPE=Build"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "/Build"    SET "BUILDTYPE=Build"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "-Build"    SET "BUILDTYPE=Build"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "--Build"   SET "BUILDTYPE=Build"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "Clean"     SET "BUILDTYPE=Clean"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "/Clean"    SET "BUILDTYPE=Clean"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "-Clean"    SET "BUILDTYPE=Clean"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "--Clean"   SET "BUILDTYPE=Clean"   & GOTO CHECKSECONDARG
  IF /I "%~1" == "Rebuild"   SET "BUILDTYPE=Rebuild" & GOTO CHECKSECONDARG
  IF /I "%~1" == "/Rebuild"  SET "BUILDTYPE=Rebuild" & GOTO CHECKSECONDARG
  IF /I "%~1" == "-Rebuild"  SET "BUILDTYPE=Rebuild" & GOTO CHECKSECONDARG
  IF /I "%~1" == "--Rebuild" SET "BUILDTYPE=Rebuild" & GOTO CHECKSECONDARG

  ECHO.
  ECHO Unsupported commandline switch!
  ECHO Run "%~nx0 help" for details about the commandline switches.
  CALL :SUBMSG "ERROR" "Compilation failed!"
)


:CHECKSECONDARG
rem Check for the second switch
IF "%~2" == "" (
  SET "ARCH=x86"
) ELSE (
  IF /I "%~2" == "x86"   SET "ARCH=x86" & GOTO START
  IF /I "%~2" == "/x86"  SET "ARCH=x86" & GOTO START
  IF /I "%~2" == "-x86"  SET "ARCH=x86" & GOTO START
  IF /I "%~2" == "--x86" SET "ARCH=x86" & GOTO START

  ECHO.
  ECHO Unsupported commandline switch!
  ECHO Run "%~nx0 help" for details about the commandline switches.
  CALL :SUBMSG "ERROR" "Compilation failed!"
)


:START
CALL "%VS100COMNTOOLS%vsvars32.bat"

CALL :SUBMSVC %BUILDTYPE% Release Win32

CALL "installer\build_installer.bat"


 :END
ECHO. & ECHO.
ENDLOCAL
PAUSE
EXIT /B


:SUBMSVC
ECHO.
TITLE Building huffyuv - %~1 "%~2|%~3"...
devenv /nologo huffyuv.sln /%~1 "%~2|%~3"
IF %ERRORLEVEL% NEQ 0 CALL :SUBMSG "ERROR" "Compilation failed!"
EXIT /B


:SHOWHELP
TITLE "%~nx0 %1"
ECHO. & ECHO.
ECHO Usage:  %~nx0 [Clean^|Build^|Rebuild] [x86]
ECHO.
ECHO Notes:  You can also prefix the commands with "-", "--" or "/".
ECHO         The arguments are not case sensitive.
ECHO. & ECHO.
ECHO Executing "%~nx0" will use the defaults: "%~nx0 build all"
ECHO.
ECHO If you skip the second argument the default one will be used.
ECHO Examples:
ECHO "%~nx0 rebuild" is the same as "%~nx0 rebuild all"
ECHO.
ECHO WARNING: "%~nx0 x86" or "%~nx0 x64" won't work.
ECHO.
ENDLOCAL
EXIT /B


:SUBMSG
ECHO. & ECHO ______________________________
ECHO [%~1] %~2
ECHO ______________________________ & ECHO.
IF /I "%~1" == "ERROR" (
  PAUSE
  EXIT
) ELSE (
  EXIT /B
)
