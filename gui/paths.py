import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RUNTIME = (Path(os.environ['LOCALAPPDATA']) / 'IronCADPhotoMatch'
           if getattr(sys, 'frozen', False) else ROOT / '.runtime')
