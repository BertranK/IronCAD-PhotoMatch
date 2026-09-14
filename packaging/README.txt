IronCAD PhotoMatch - Windows x64 / IronCAD 2027

1. Install IronCAD 2027.
2. Save your work and close IronCAD and PhotoMatch.
3. Run PhotoMatch-2027-Setup.exe, choose the IronCAD 2027 installation folder,
   and allow administrator access. Setup enables PhotoMatch in Add-in Applications
   for your Windows account. Then start IronCAD.
4. Open Add-Ins > IronCAD PhotoMatch, open a photo, and pick model vertices.
   Match at least six pairs spread across the model. You can add more.

Python is bundled. Node.js is not required.
Visual Studio, MFC/ATL build tools, and the Windows SDK are for developers only.
Setup installs Microsoft Edge WebView2 automatically when missing. That download
requires internet access; an existing runtime is reused. Setup stops if WebView2
installation fails. Removing PhotoMatch leaves this shared runtime installed.
The Microsoft Visual C++ x64 runtime and MFC runtime supplied with IronCAD
must be available. This prototype package is unsigned.

Finish model picking to select the first point without a photo location.
Existing photo matches stay saved when adding more model points.
Planar points are supported with Estimate optical center off. For ambiguous
views, add points at different depths.
Photo opacity controls the IronCAD overlay from 0% to 100%.
Automatic recalculate is enabled by default. Preview in IronCAD, adjust
the camera if needed, and save results as JSON. Restore view returns to
the original camera. Clear XY removes photo coordinates only; model points
stay. Clear all removes all point pairs and restarts numbering at P1.
Each row's X deletes that model/photo pair.
Moved scenes reconnect after saved vertex references are verified.
Opening a scene allows up to five minutes for IronCAD to respond.

Images: PNG, JPEG, BMP, AVIF, WebP, TIFF, GIF, ICO, JPEG 2000, PNM, TGA,
PCX, DDS, QOI. Only the first frame/page is used. HEIC/RAW/vector are not
supported. Alignment accuracy is under validation; no lens correction.

If Waiting IronCAD appears: open a scene and launch PhotoMatch from its
IronCAD Add-Ins button. Hover over the connection message for the error.
The first photo can open without IronCAD; matching requires a connection.
Packaged preferences and logs: %LOCALAPPDATA%\IronCADPhotoMatch.

Removal: close IronCAD and PhotoMatch, then remove IronCAD PhotoMatch
from Windows Settings > Apps > Installed apps. Saved projects, user
preferences, and host-config backups remain. Other add-ins are preserved.

Validation: native and GUI regression tests plus a frozen WebView2 launch
check are required for package creation. Installation and camera accuracy
still require acceptance checks on the recipient's IronCAD machine.
Third-party license texts are included in THIRD-PARTY-NOTICES.

Build: install Inno Setup 6 from https://jrsoftware.org/isdl.php, then run
scripts/package.ps1 -Compiler "PATH\ISCC.exe" from the source tree.
Build output, personal samples, and local evidence stay outside Git.
Installer source and regression tests stay in Git for repeatable builds.
