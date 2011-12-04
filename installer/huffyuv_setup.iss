;*  Huffyuv - Installer script
;*
;*  Copyright (C) 2011 XhmikosR, http://code.google.com/p/huffyuv/
;*
;*  This program is free software: you can redistribute it and/or modify
;*  it under the terms of the GNU General Public License as published by
;*  the Free Software Foundation, either version 3 of the License, or
;*  (at your option) any later version.
;*
;*  This program is distributed in the hope that it will be useful,
;*  but WITHOUT ANY WARRANTY; without even the implied warranty of
;*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;*  GNU General Public License for more details.
;*
;*  You should have received a copy of the GNU General Public License
;*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
;*
;**************************************************************************

; Requirements:
; Inno Setup v5.4.2(+): http://www.jrsoftware.org/isdl.php

; $Id$


#define HUFFYUV_VERSION  "2.1.1"
#define CCESP_VERSION    "0.2.5"
#define PUBLISHER        "Ben Rudiak-Gould et all"
#define WEBPAGE          "http://code.google.com/p/huffyuv/"

;#define WDK_BUILD

#if VER < 0x05040200
  #error Update your Inno Setup version
#endif


#ifdef WDK_BUILD
  #define bindir "..\src\WDK"
#else
  #define bindir "..\src\Release"
#endif


[Setup]
AppID=Huffyuv
AppName=Huffyuv
AppVerName=Huffyuv {#HUFFYUV_VERSION}
AppVersion={#HUFFYUV_VERSION}
AppContact={#WEBPAGE}
AppCopyright={#PUBLISHER}
AppPublisher={#PUBLISHER}
AppPublisherURL={#WEBPAGE}
AppSupportURL={#WEBPAGE}
AppUpdatesURL={#WEBPAGE}
VersionInfoCompany={#PUBLISHER}
VersionInfoCopyright={#PUBLISHER}
VersionInfoDescription=Huffyuv {#HUFFYUV_VERSION} Setup
VersionInfoTextVersion={#HUFFYUV_VERSION}
VersionInfoVersion={#HUFFYUV_VERSION}
VersionInfoProductName=Huffyuv
VersionInfoProductVersion={#HUFFYUV_VERSION}
VersionInfoProductTextVersion={#HUFFYUV_VERSION}
DefaultDirName={pf}\Huffyuv
AppReadmeFile={app}\readme.txt
InfoBeforeFile=..\copying.txt
OutputDir=.
#ifdef WDK_BUILD
OutputBaseFilename=HuffyuvSetup_{#HUFFYUV_VERSION}_WDK
MinVersion=0,5.0
#else
OutputBaseFilename=HuffyuvSetup_{#HUFFYUV_VERSION}
MinVersion=0,5.01SP3
#endif
#ifdef CCESP_VERSION
UninstallDisplayName=Huffyuv [{#HUFFYUV_VERSION}/CCESP {#CCESP_VERSION}]
#else
UninstallDisplayName=Huffyuv [{#HUFFYUV_VERSION}]
#endif
SolidCompression=yes
Compression=lzma/ultra64
InternalCompressLevel=max
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes


[Files]
Source: {#bindir}\huffyuv.dll; DestDir: {sys}; Flags: sharedfile ignoreversion uninsnosharedfileprompt restartreplace
Source: ..\copying.txt;        DestDir: {app}; Flags: ignoreversion restartreplace
Source: ..\readme.txt;         DestDir: {app}; Flags: ignoreversion restartreplace


[Registry]
Root: HKLM;   Subkey: SYSTEM\CurrentControlSet\Control\MediaResources\icm\VIDC.HFYU; ValueType: string; ValueName: Description;  ValueData: Huffyuv lossless codec [HFYU]; Flags: uninsdeletevalue
Root: HKLM;   Subkey: SYSTEM\CurrentControlSet\Control\MediaResources\icm\VIDC.HFYU; ValueType: string; ValueName: Driver;       ValueData: huffyuv.dll;                   Flags: uninsdeletevalue
Root: HKLM;   Subkey: SYSTEM\CurrentControlSet\Control\MediaResources\icm\VIDC.HFYU; ValueType: string; ValueName: FriendlyName; ValueData: Huffyuv lossless codec [HFYU]; Flags: uninsdeletevalue
Root: HKLM;   Subkey: SOFTWARE\Microsoft\Windows NT\CurrentVersion\drivers.desc;     ValueType: string; ValueName: huffyuv.dll;  ValueData: Huffyuv lossless codec [HFYU]; Flags: uninsdeletevalue
Root: HKLM;   Subkey: SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32;        ValueType: string; ValueName: VIDC.HFYU;    ValueData: huffyuv.dll;                   Flags: uninsdeletevalue


[INI]
FileName: {win}\system.ini; Section: drivers32; Key: VIDC.HFYU; String: huffyuv.dll; Flags: uninsdeleteentry


[Run]
Filename: {sys}\rundll32.exe; Description: Configure Huffyuv; Parameters: """{sys}\huffyuv.dll"",Configure"; WorkingDir: {sys}; Flags: postinstall nowait skipifsilent unchecked
Filename: {#WEBPAGE};         Description: Visit Webpage;                                                                       Flags: postinstall nowait skipifsilent shellexec unchecked


[Code]
function IsUpgrade(): Boolean;
var
  sPrevPath: String;
begin
  sPrevPath := WizardForm.PrevAppDir;
  Result := (sPrevPath <> '');
end;


function IsOldBuildInstalled(): Boolean;
var
  rootkey: Integer;
begin
  if IsWin64 then
    rootkey := HKLM64
  else
    rootkey := HKLM;

  if RegKeyExists(rootkey, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Huffyuv') and
  FileExists(ExpandConstant('{win}\inf\huffyuv.inf')) then begin
    Log('Custom Code: The old build is installed');
    Result := True;
  end
  else begin
    Log('Custom Code: The old build is NOT installed');
    Result := False;
  end;

end;


function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // Hide the InfoBefore page
  if IsUpgrade() and (PageID = wpInfoBefore) then
    Result := True
  else
    Result := False
end;


function UninstallOldVersion(): Integer;
var
  iResultCode: Integer;
  bOldState: Boolean;
begin
  // Return Values:
  // 0 - no idea
  // 1 - error executing the command
  // 2 - successfully executed the command

  // default return value
  Log('Custom Code: Will try to uninstall the old build');
  Result := 0;
  if IsWin64 then
    bOldState := EnableFsRedirection(False);

  if Exec('rundll32.exe', ExpandConstant('setupapi.dll,InstallHinfSection DefaultUninstall 132 {win}\inf\huffyuv.inf'), '', SW_HIDE, ewWaitUntilTerminated, iResultCode) then begin
    Result := 2;
    Sleep(500);
    Log('Custom Code: The old build was successfully uninstalled');
    if IsWin64 then
      EnableFsRedirection(bOldState);
  end
  else begin
    Result := 1;
    Log('Custom Code: Something went wrong when uninstalling the old build');
  end;
end;


procedure CurPageChanged(CurPageID: Integer);
begin
  if IsUpgrade() and (CurPageID = wpWelcome) then
    WizardForm.NextButton.Caption := SetupMessage(msgButtonInstall)
  else if CurPageID = wpInfoBefore then
    WizardForm.NextButton.Caption := SetupMessage(msgButtonInstall)
  else if CurPageID = wpFinished then
    WizardForm.NextButton.Caption := SetupMessage(msgButtonFinish)
  else if IsUpgrade() then
    WizardForm.NextButton.Caption := SetupMessage(msgButtonNext);
end;


procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssInstall) and IsOldBuildInstalled() then
    UninstallOldVersion();
end;


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  // When uninstalling ask user to delete huffyuv settings
  if CurUninstallStep = usUninstall then begin
    if FileExists(ExpandConstant('{win}\huffyuv.ini')) then begin
      if SuppressibleMsgBox('Do you also want to delete Huffyuv settings? If you plan on reinstalling Huffyuv you do not have to delete them.',
        mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO) = IDYES then begin
         DeleteFile(ExpandConstant('{win}\huffyuv.ini'));
      end;
    end;
  end;
end;
