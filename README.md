# IronCAD PhotoMatch

Align an IronCAD 3D model with a photo using matching points.
Built with a C++ add-in and Python GUI for IronCAD 2027.

Prototype: alignment accuracy is still being validated. Lens correction is not included.

## Features

- Open PNG, JPG, BMP, and AVIF photos.
- Match points and calculate the camera.
- Preview and adjust the camera in IronCAD.
- Save and load results as JSON.
- Korean/English and light/dark themes.

## Setup

Requires Windows, IronCAD 2027, Python 3.13, Node.js, and Edge WebView2.
Build tools: MSVC v143 x64 with MFC/ATL and Windows SDK 10.0.19041.0.

Place the project under IronCAD's `ICAPI/PhotoMatchProto`. Run from that folder:

```powershell
& .\scripts\setup-gui.ps1
& .\scripts\build.ps1
& .\scripts\register.ps1
& .\scripts\start-gui.ps1
```

Registration needs administrator access. Close IronCAD before updating the add-in.
No end-user installer is available yet.

## Usage

1. Open a model and photo.
2. Match at least six points at different depths.
3. Click **Calculate camera**.
4. **Preview in IronCAD**; use **Adjust camera** if needed.
5. **Save** results or **Open** a saved file.
6. Click **Restore view** to return to the original camera.

## Tests

```powershell
& .\tests\RegistrationProbe.ps1
& .\scripts\test-gui.ps1
```

Live IronCAD checks are also required. See [status](docs/PROTO0_STATUS.md) and [acceptance criteria](tests/ACCEPTANCE.md).

## Folders

`src/` add-in · `core/` projection math · `gui/` interface · `scripts/` setup and build · `tests/` checks · `docs/` documentation · `evidence/` local results
