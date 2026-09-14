# Tests

- `scripts/build.ps1`: build the add-in and run native projection tests.
- `scripts/test-gui.ps1`: run Python, named-pipe, and browser regression tests.
- `tests/PackagingTests.ps1`: check installer syntax and isolated host-config changes.
- `tests/TestWebView2Installer.ps1 -Compiler "PATH\ISCC.exe"`: test prerequisite skip, install, failure, and restart handling without changing the installed runtime.
- `scripts/package.ps1`: run checks, freeze the GUI, verify WebView2 startup, and create a setup EXE.
- `RegistrationProbe.ps1`: inspect the installed add-in; requires a configured host.

`fixture/` contains synthetic reference inputs used by tests. Keep these in Git.
Save personal project JSON, screenshots, and one-off diagnostics in ignored `evidence/`.
Local example models stay in ignored `testsample/`.

Automated tests do not establish live CAD accuracy. Follow [acceptance criteria](ACCEPTANCE.md).
