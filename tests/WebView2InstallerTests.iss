// Run the production prerequisite decision logic with an isolated fake installer.
#define WebView2Test
[Setup]
AppName=PhotoMatch prerequisite tests
AppVersion=1
CreateAppDir=no
Uninstallable=no
PrivilegesRequired=lowest
OutputBaseFilename=WebView2InstallerTests
[Code]
var Present, LaunchOK, InstallSucceeds: Boolean;
    InstallCode, LaunchCount: Integer;
function WebView2Installed: Boolean;
begin Result := Present; end;
function LaunchWebView2(var ExitCode: Integer): Boolean;
begin
  LaunchCount := LaunchCount + 1; ExitCode := InstallCode;
  Present := InstallSucceeds; Result := LaunchOK;
end;
#include "..\packaging\WebView2.iss"
procedure Check(Value: Boolean; Message: String);
begin if not Value then RaiseException(Message); end;
function InitializeSetup: Boolean;
var Restart: Boolean; ErrorText: String;
begin
  Check(not HasWebView2Version(''), 'Empty version accepted');
  Check(not HasWebView2Version('0.0.0.0'), 'Uninstalled version accepted');
  Check(HasWebView2Version('152.0.4191.66'), 'Installed version rejected');
  Restart := False; LaunchCount := 0; Present := True;
  Check(EnsureWebView2(Restart) = '', 'Existing runtime rejected');
  Check(LaunchCount = 0, 'Existing runtime reinstalled');
  Present := False; LaunchOK := True; InstallSucceeds := True; InstallCode := 0;
  Check(EnsureWebView2(Restart) = '', 'Successful installation rejected');
  Check(LaunchCount = 1, 'Missing runtime not installed exactly once');
  Present := False; InstallSucceeds := False; LaunchOK := False;
  Check(EnsureWebView2(Restart) <> '', 'Launch failure ignored');
  LaunchOK := True; InstallCode := 1;
  Check(EnsureWebView2(Restart) <> '', 'Download/install failure ignored');
  InstallCode := 0;
  Check(EnsureWebView2(Restart) <> '', 'Success exit without runtime accepted');
  InstallCode := 3010;
  ErrorText := EnsureWebView2(Restart);
  Check((ErrorText <> '') and Restart, 'Restart requirement lost');
  SaveStringToFile(ExpandConstant('{param:REPORT}'), 'passed', False);
  Result := False;
end;
