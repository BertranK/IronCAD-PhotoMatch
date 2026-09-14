IronCAD PhotoMatch - Windows x64 / IronCAD 2027

1. Install IronCAD 2027 and Microsoft Edge WebView2 Runtime.
2. Extract the entire ZIP into a writable folder. Keep it for backups/removal.
3. Save your work and close IronCAD and PhotoMatch.
4. Double-click Install.cmd, choose the IronCAD 2027 installation folder,
   and allow administrator access. Then start IronCAD.
5. Open Add-Ins > IronCAD PhotoMatch, open a photo, and pick model vertices.
   Match at least six pairs at different depths. You can add more.

Python and Node.js are bundled or unnecessary; do not install them.
The Microsoft Visual C++ x64 runtime and MFC runtime supplied with IronCAD
must be available. This prototype package is unsigned.

Finish model picking to start photo matching at P1 (or the first remaining card).
Photo opacity controls the IronCAD overlay from 0% to 100%.
Automatic recalculate is enabled by default. Preview in IronCAD, adjust
the camera if needed, and save results as JSON. Restore view returns to
the original camera. Clear removes photo coordinates only; model points
stay. Each row's X deletes that model/photo pair. Deleting every model
point restarts numbering at P1.

Images: PNG, JPEG, BMP, AVIF, WebP, TIFF, GIF, ICO, JPEG 2000, PNM, TGA,
PCX, DDS, QOI. Only the first frame/page is used. HEIC/RAW/vector are not
supported. Alignment accuracy is under validation; no lens correction.

If Waiting IronCAD appears: open a scene and launch PhotoMatch from its
IronCAD Add-Ins button. Hover over the connection message for the error.
The first photo can open without IronCAD; matching requires a connection.
Packaged preferences and logs: %LOCALAPPDATA%\IronCADPhotoMatch.

Removal: close IronCAD and PhotoMatch, then run administrator PowerShell
from this extracted folder:
  .\scripts\register.ps1 -Unregister -IronRoot "YOUR IRONCAD 2027 FOLDER"
This removes the add-in registration. Installed files, saved projects,
and user preferences remain. Backups are retained in evidence.

Validation: native and GUI regression tests plus a frozen WebView2 launch
check are required for package creation. Installation and camera accuracy
still require acceptance checks on the recipient's IronCAD machine.
Third-party license texts are included in THIRD-PARTY-NOTICES.
