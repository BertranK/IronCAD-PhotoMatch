# IronCAD PhotoMatch

Align an IronCAD 3D model with a photo using matching points.
Built with a C++ add-in and Python GUI for IronCAD 2027.

Prototype: alignment accuracy is still being validated. Lens correction is not included.

## Features

- Open PNG, JPG, BMP, AVIF, WebP, TIFF, GIF, ICO, JPEG 2000, PNM, TGA, PCX, DDS, and QOI images.
- Match points and calculate the camera.
- See matching point numbers on the model; use each row's × button to delete a pair.
- Clear photo coordinates to remap from the first card; model points and their numbers stay saved.
- Preview and adjust the camera in IronCAD; control photo opacity with a slider.
- Save and load results as JSON.
- Korean/English and light/dark themes.

Animated and multipage images use the first frame or page. HEIC, camera RAW, and vector files are not supported.

## End-user package

Run **PhotoMatch-2027-Setup.exe** and choose your IronCAD 2027 folder.
Close IronCAD and PhotoMatch first; administrator access and Edge WebView2 are required.
The package includes Python. See [package instructions](packaging/README.txt).

## Developer setup

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

Install [Inno Setup 6](https://jrsoftware.org/isdl.php), then run `scripts/package.ps1 -Compiler "PATH\ISCC.exe"` to build the setup EXE. Output stays in `build/packages/`.

## Usage

1. Open a model and photo.
2. Pick at least six model points at different depths. Finish picking, then mark the photo from P1.
3. The camera recalculates as points change with **Automatic recalculate** enabled (default).
4. **Preview in IronCAD**; use **Adjust camera** if needed.
5. **Save** results or **Open** a saved file.
6. Click **Restore view** to return to the original camera.

## Tests

```powershell
& .\tests\RegistrationProbe.ps1
& .\scripts\test-gui.ps1
```

See [test guide](tests/README.md), [status](docs/PROTO0_STATUS.md), and [live acceptance](tests/ACCEPTANCE.md).

## Folders

`src/` add-in · `core/` projection math · `gui/` interface · `scripts/` setup and build · `tests/` checks · `docs/` documentation
