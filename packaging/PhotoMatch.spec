from pathlib import Path

root = Path(SPECPATH).parent
web = root / 'gui' / 'web'
assets = [(str(web/name), 'web') for name in
          ('index.html', 'app.js', 'coordinates.js', 'localization.js', 'tailwind.css')]
a = Analysis([str(root/'gui/app.py')], pathex=[str(root/'gui')], datas=assets,
             excludes=['pytest', 'tkinter', 'PyQt5', 'PyQt6', 'PySide2', 'PySide6'])
pyz = PYZ(a.pure)
exe = EXE(pyz, a.scripts, [], exclude_binaries=True, name='PhotoMatch',
          console=False, icon=str(root/'src/assets/photomatch.ico'))
coll = COLLECT(exe, a.binaries, a.datas, name='PhotoMatch')
