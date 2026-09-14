# Camera workflow

1. Open a photo and select model vertices. Keep at least six matching pairs at different depths.
2. Finish model picking, then mark the photo from the first point card.
3. Calculate automatically as points change, or use Calculate camera.
4. Preview in IronCAD; use Photo opacity to compare the model with the reference.
5. Adjust the camera against the fixed photo, then save or cancel the adjustment.
6. Save results as JSON. Restore view returns to the original camera.

Clear removes only photo coordinates. Delete removes the selected model/photo pair.
Remaining point numbers stay stable; deleting every model point restarts at P1.
Closing a photo restores its owned preview before discarding image-specific input.
A failed restore preserves the input.

Model edits keep verifiable persistent references. Only ambiguous or deleted vertices
need reconnection. Changing documents preserves the old matches for review; it never
applies an old camera to a different document. A replacement model must be explicitly
connected to the saved rows.

The default solver estimates focal length with a centered optical axis and no lens
distortion. Optical-center estimation is optional. Manual camera settings remain
separate from both the fitted camera and the original-view snapshot.

Deferred: fixed-intrinsic pose solving, planar four-point ambiguity handling, radial
lens calibration, and independent calibration using distributed points or multiple views.
The 0.1 source-pixel fit goal and 1 physical-pixel host-projection check are separate.
