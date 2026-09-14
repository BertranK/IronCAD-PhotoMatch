"""Retain installed distributions' license files in the end-user archive."""
import importlib.metadata
from pathlib import Path
import shutil
import sys

target=Path(sys.argv[1]);target.mkdir(parents=True,exist_ok=True)
for dist in importlib.metadata.distributions():
    name=dist.metadata['Name']
    for file in dist.files or []:
        if any(word in file.name.lower() for word in ('license','copying','notice')):
            source=Path(dist.locate_file(file))
            if source.is_file():
                destination=target/name/str(file).replace('../','').replace('..\\','')
                destination.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(source,destination)
    folder=target/name;folder.mkdir(parents=True,exist_ok=True)
    (folder/'metadata.txt').write_text(dist.read_text('METADATA') or dist.read_text('PKG-INFO') or name,encoding='utf-8')
python_license=Path(sys.base_prefix)/'LICENSE.txt'
if not python_license.exists():
    raise RuntimeError('Python license not found')
shutil.copy2(python_license,target/'PYTHON-LICENSE.txt')
