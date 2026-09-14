#ifndef PackageDir
  #error PackageDir must name a verified package directory.
#endif

[Setup]
AppId={{A268C9A5-5C19-44F9-A840-17F680CBEA82}
AppName=IronCAD PhotoMatch
AppVersion=2027 Preview
VersionInfoVersion=0.1.0.0
DefaultDirName={autopf}\IronCAD\2027
AppendDefaultDirName=no
DisableDirPage=no
DirExistsWarning=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
WizardStyle=modern
SetupIconFile=..\src\assets\photomatch.ico
UninstallDisplayIcon={app}\bin\PhotoMatch\PhotoMatch.exe
UninstallFilesDir={app}\bin\PhotoMatch\setup
OutputBaseFilename=PhotoMatch-2027-Setup
Compression=lzma2
SolidCompression=yes
CloseApplications=no
RestartApplications=no
AllowNoIcons=yes

[Messages]
SelectDirLabel3=Select your existing IronCAD 2027 installation folder, containing bin and Config.
FinishedLabel=PhotoMatch is installed. Start IronCAD and choose Add-Ins > IronCAD PhotoMatch.

[Dirs]
Name: "{app}\bin\PhotoMatch\setup\evidence"; Flags: uninsneveruninstall

[Files]
Source: "{#PackageDir}\PhotoMatch\*"; DestDir: "{app}\bin\PhotoMatch"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageDir}\build\v143\PhotoMatchProto.dll"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#PackageDir}\build\v143\PhotoMatchProto.dll"; DestDir: "{app}\bin\PhotoMatch\setup\build\v143"; Flags: ignoreversion
Source: "{#PackageDir}\scripts\*.ps1"; DestDir: "{app}\bin\PhotoMatch\setup\scripts"; Flags: ignoreversion
Source: "CheckHost.ps1"; DestDir: "{app}\bin\PhotoMatch\setup"; Flags: ignoreversion
Source: "CheckHost.ps1"; Flags: dontcopy
Source: "{#PackageDir}\THIRD-PARTY-NOTICES\*"; DestDir: "{app}\bin\PhotoMatch\THIRD-PARTY-NOTICES"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageDir}\README.txt"; DestDir: "{app}\bin\PhotoMatch"; Flags: ignoreversion

[Code]
function RunPowerShell(Script, Arguments: String): Integer;
var Code: Integer;
begin
  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    '-NoProfile -ExecutionPolicy Bypass -File "' + Script + '" ' + Arguments,
    '', SW_HIDE, ewWaitUntilTerminated, Code) then Code := -1;
  Result := Code;
end;

function CheckHost(Script: String): String;
var ErrorFile: String; ErrorText: AnsiString;
begin
  Result := '';
  ErrorFile := ExpandConstant('{tmp}\photomatch-check.txt');
  DeleteFile(ErrorFile);
  if RunPowerShell(Script, '-IronRoot "' + ExpandConstant('{app}') +
      '" -ErrorFile "' + ErrorFile + '"') <> 0 then begin
    if LoadStringFromFile(ErrorFile, ErrorText) then Result := String(ErrorText)
    else Result := 'IronCAD installation check failed. Save your work and close IronCAD and PhotoMatch.';
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  ExtractTemporaryFile('CheckHost.ps1');
  Result := CheckHost(ExpandConstant('{tmp}\CheckHost.ps1'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    if RunPowerShell(ExpandConstant('{app}\bin\PhotoMatch\setup\scripts\register.ps1'),
        '-IronRoot "' + ExpandConstant('{app}') + '"') <> 0 then
      RaiseException('PhotoMatch registration failed. Run setup again after checking IronCAD and its runtime prerequisites.');
end;

function InitializeUninstall: Boolean;
var ErrorText: String;
begin
  ErrorText := CheckHost(ExpandConstant('{app}\bin\PhotoMatch\setup\CheckHost.ps1'));
  Result := ErrorText = '';
  if not Result then MsgBox(ErrorText, mbError, MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    if RunPowerShell(ExpandConstant('{app}\bin\PhotoMatch\setup\scripts\register.ps1'),
        '-Unregister -IronRoot "' + ExpandConstant('{app}') + '"') <> 0 then
      RaiseException('PhotoMatch could not be unregistered. Its files have been retained.');
end;
