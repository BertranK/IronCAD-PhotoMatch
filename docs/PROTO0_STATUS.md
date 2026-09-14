# Current status

## Implemented

- IronCAD 2027 x64 add-in with a separate Python/WebView2 interface.
- Fourteen image formats; first frame/page only. Source coordinates remain unchanged.
- Six or more point pairs, stable numbered model markers, and per-pair deletion.
- Closing PhotoMatch hides model markers and retains model points, including after Restore view.
- Clear XY retains model points; Clear all removes pairs and restarts numbering at P1.
- New model points select their cards; finishing picking selects the first unmatched photo card.
- Step 03 is Calibration; Reconnect and delete buttons share a 30 px height.
- Automatic camera recalculation, optional optical-center estimation, and preview.
- Photo opacity from 0% to 100%, independent of camera and point coordinates.
- Manual camera adjustment, save/cancel, JSON results, and original-view restoration.
- Same-model references persist when verifiable; ambiguous/deleted vertices need reconnection.
- Document changes preserve photo matches for review. Reconnection never guesses a nearest vertex.
- Relocated scenes reconnect through verified vertex references. Scene opening allows five minutes.
- First-photo opening works without a host connection. An active preview still requires successful restoration before replacement.
- Windows package scripts bundle the GUI and install beside IronCAD; runtime data uses Local AppData.

## Verification

2026-09-15: 72 Python and 12 browser/JavaScript tests passed, along with native
projection, packaging, WebView2, and frozen-GUI checks. In live IronCAD, a scene
saved out of a temporary folder was closed, reopened, and reconnected with all
16 fixture points. Changed geometry was rejected without replacing references.
Clear XY retained model points; Clear all removed pairs and kept the photo.
Window-close regressions cover active/restored views, failed cleanup, disconnected
hosts, and document switches without deleting correspondences.

Native projection, Python/pipe, browser, and package-config checks run locally.
Package creation also checks the frozen WebView2 window and image/scientific-library imports.
The running IronCAD host responded during the reported waiting-state investigation;
the exact trigger for that intermittent state was not reproduced. Connection errors
are now available by hovering over the connection message.

## Remaining acceptance

The setup EXE still requires installation and removal checks on a recipient machine.
Live opacity changes, marker visibility, camera restoration, and the complete
100%/150% DPI matrix require [host acceptance](../tests/ACCEPTANCE.md).
A low fitted residual is not independent camera or lens calibration. Lens correction
and fixed-intrinsic four-point solving are not implemented.

Personal results, historical diagnostics, and previous status notes stay in ignored
`evidence/`; they are not regression fixtures or package contents.
