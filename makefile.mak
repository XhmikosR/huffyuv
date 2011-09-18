#******************************************************************************
#*
#*  makefile.mak
#*    makefile for building with WDK
#*
#*  Copyright (C) 2011 XhmikosR
#*
#*  This program is free software: you can redistribute it and/or modify
#*  it under the terms of the GNU General Public License as published by
#*  the Free Software Foundation, either version 3 of the License, or
#*  (at your option) any later version.
#*
#*  This program is distributed in the hope that it will be useful,
#*  but WITHOUT ANY WARRANTY; without even the implied warranty of
#*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*  GNU General Public License for more details.
#*
#*  You should have received a copy of the GNU General Public License
#*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#*
#*
#*  Use build_wdk.bat and set there your WDK directory.
#*
#******************************************************************************


# Remove the .SILENT directive in order to display all the commands
.SILENT:


CC = cl.exe
LD = link.exe
ML = ml.exe
RC = rc.exe


SRC       = .
BINDIR    = WDK
OBJDIR    = $(BINDIR)\obj
EXE       = $(BINDIR)\huffyuv.dll


DEFINES   = /D "_WINDOWS" /D "NDEBUG" /D "WIN32" /D "_WIN32_WINNT=0x0500"
CPPFLAGS  = /nologo /c /Fo"$(OBJDIR)/" /W3 /EHsc /MD /O2 /GL /MP /arch:SSE $(DEFINES)
LIBS      = kernel32.lib user32.lib shell32.lib winmm.lib msvcrt_win2000.obj
LDFLAGS   = /NOLOGO /WX /INCREMENTAL:NO /RELEASE /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS,5.0 \
            /LARGEADDRESSAWARE /DLL /MACHINE:X86 /LTCG /DEF:huffyuv.def
MLFLAGS   = /c /nologo /Zi /Fo"$(OBJDIR)/" /W3
RFLAGS    = /l 0x0409 /d "WIN32" /d "NDEBUG"


# Targets
BUILD:	CHECKDIRS $(EXE)

CHECKDIRS:
	IF NOT EXIST "$(OBJDIR)" MD "$(OBJDIR)"

CLEAN:
	ECHO Cleaning... & ECHO.
	IF EXIST "$(EXE)"                DEL "$(EXE)"
	IF EXIST "$(OBJDIR)\*.obj"       DEL "$(OBJDIR)\*.obj"
	IF EXIST "$(BINDIR)\huffyuv.lib" DEL "$(BINDIR)\huffyuv.lib"
	IF EXIST "$(BINDIR)\huffyuv.exp" DEL "$(BINDIR)\huffyuv.exp"
	IF EXIST "$(BINDIR)\huffyuv.pdb" DEL "$(BINDIR)\huffyuv.pdb"
	IF EXIST "$(OBJDIR)\huffyuv.res" DEL "$(OBJDIR)\huffyuv.res"
	-IF EXIST "$(OBJDIR)"            RD /Q "$(OBJDIR)"
	-IF EXIST "$(BINDIR)"            RD /Q "$(BINDIR)"

REBUILD:	CLEAN BUILD


# Object files
OBJECTS = \
    $(OBJDIR)\drvproc.obj \
    $(OBJDIR)\huffyuv.obj \
    $(OBJDIR)\huffyuv.res \
    $(OBJDIR)\huffyuv_a.obj \
    $(OBJDIR)\tables.obj


# Batch rules
{$(SRC)\}.asm{$(OBJDIR)}.obj:
    $(ML) $(MLFLAGS) /Ta $<

{$(SRC)\}.cpp{$(OBJDIR)}.obj::
    $(CC) $(CPPFLAGS) /Tp $<


# Commands
$(EXE): $(OBJECTS)
	$(RC) $(RFLAGS) /fo"$(OBJDIR)\huffyuv.res" "$(SRC)\huffyuv.rc" >NUL
	$(LD) $(LDFLAGS) /OUT:"$(EXE)" $(OBJECTS) $(LIBS)


# Dependencies
$(OBJDIR)\drvproc.obj: \
    $(SRC)\drvproc.cpp \
    $(SRC)\huffyuv.h

$(OBJDIR)\huffyuv.obj: \
    $(SRC)\huffyuv.cpp \
    $(SRC)\huffyuv.h \
    $(SRC)\huffyuv_a.h \
    $(SRC)\resource.h

$(OBJDIR)\huffyuv_a.obj: \
    $(SRC)\huffyuv_a.asm

$(OBJDIR)\tables.obj: \
    $(SRC)\tables.cpp \
    $(SRC)\huffyuv.h

$(OBJDIR)\huffyuv.res: \
    $(SRC)\huffyuv.rc \
    $(SRC)\resource.h
