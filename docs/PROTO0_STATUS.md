# Current status

## Implemented

- IronCAD 2027 x64 add-in with a separate Python/WebView2 interface.
- Fourteen image formats; first frame/page only. Source coordinates remain unchanged.
- Six or more point pairs, stable numbered model markers, and per-pair deletion.
- Clear removes photo coordinates only and retains model points and numbers.
- New model points select their cards; finishing picking selects the first card for photo matching.
- Automatic camera recalculation, optional optical-center estimation, and preview.
- Photo opacity from 0% to 100%, independent of camera and point coordinates.
- Manual camera adjustment, save/cancel, JSON results, and original-view restoration.
- Same-model references persist when verifiable; ambiguous/deleted vertices need reconnection.
- Document changes preserve photo matches for review. Reconnection never guesses a nearest vertex.
- First-photo opening works without a host connection. An active preview still requires successful restoration before replacement.
- Windows package scripts bundle the GUI and install beside IronCAD; runtime data uses Local AppData.

## Verification

Native projection, Python/pipe, browser, and package-config checks run locally.
Package creation also checks the frozen WebView2 window and image/scientific-library imports.
The running IronCAD host responded during the reported waiting-state investigation;
the exact trigger for that intermittent state was not reproduced. Connection errors
are now available by hovering over the connection message.

## Remaining acceptance

The latest package has not been installed into the user's open IronCAD session.
Live opacity changes, marker visibility, camera restoration, and the complete
100%/150% DPI matrix require [host acceptance](../tests/ACCEPTANCE.md).
A low fitted residual is not independent camera or lens calibration. Lens correction
and fixed-intrinsic four-point solving are not implemented.

Personal results, historical diagnostics, and previous status notes stay in ignored
`evidence/`; they are not regression fixtures or package contents.
