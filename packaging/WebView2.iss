// Microsoft's documented Evergreen Runtime registration, not the Edge browser.
function HasWebView2Version(Version: String): Boolean;
begin
  Result := (Version <> '') and (Version <> '0.0.0.0');
end;

#ifndef WebView2Test
function WebView2Installed: Boolean;
var MachineVersion, UserVersion: String;
begin
  MachineVersion := ''; UserVersion := '';
  RegQueryStringValue(HKLM32, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', MachineVersion);
  RegQueryStringValue(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', UserVersion);
  Result := HasWebView2Version(MachineVersion) or HasWebView2Version(UserVersion);
end;

function LaunchWebView2(var ExitCode: Integer): Boolean;
begin
  WizardForm.StatusLabel.Caption := 'Installing Microsoft Edge WebView2 Runtime (internet required)...';
  ExtractTemporaryFile('MicrosoftEdgeWebview2Setup.exe');
  Result := Exec(ExpandConstant('{tmp}\MicrosoftEdgeWebview2Setup.exe'),
    '/silent /install', '', SW_HIDE, ewWaitUntilTerminated, ExitCode);
end;
#endif

function EnsureWebView2(var NeedsRestart: Boolean): String;
var ExitCode, Attempt: Integer;
begin
  Result := '';
  if WebView2Installed then exit;
  ExitCode := -1;
  if not LaunchWebView2(ExitCode) then begin
    Result := 'Could not start Microsoft WebView2 installation. Run setup again.';
    exit;
  end;
  if (ExitCode = 3010) or (ExitCode = 1641) then NeedsRestart := True;
  if ExitCode = 0 then
    for Attempt := 1 to 10 do begin
      if WebView2Installed then break;
      Sleep(1000);
    end;
  if WebView2Installed then exit;
  if NeedsRestart then
    Result := 'Restart Windows, then run PhotoMatch setup again to finish installing WebView2.'
  else
    Result := 'Microsoft WebView2 could not be installed (code ' + IntToStr(ExitCode) +
      '). Check your internet connection and system installation policy, then retry. PhotoMatch has not been installed.';
end;
