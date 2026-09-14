from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'gui'))
from preferences import Preferences, initial_window_size


class PreferenceTests(unittest.TestCase):
    def test_initial_window_fits_1080p_work_area_at_100_150_200_percent(self):
        def work_area(action, unused, rect, flags):
            rect._obj.left=0;rect._obj.top=0;rect._obj.right=1920;rect._obj.bottom=1000
            return True
        for dpi in (96,144,192):
            user32=SimpleNamespace(SystemParametersInfoW=work_area,GetDpiForSystem=lambda:dpi)
            with patch('preferences.ct.WinDLL',return_value=user32):
                width,height=initial_window_size()
            self.assertLessEqual((width+32)*dpi/96,1920)
            self.assertLessEqual((height+48)*dpi/96,1000)

    def test_system_language_theme_changes_and_manual_override_persist(self):
        with tempfile.TemporaryDirectory() as directory:
            prefs = Preferences(directory)
            with patch('preferences.windows_language', return_value='ko-KR'), patch('preferences.windows_theme', return_value='dark'):
                self.assertEqual('en', prefs.snapshot()['language'])
                self.assertEqual('dark', prefs.snapshot()['theme'])
                prefs.set_language('en')
                self.assertEqual('en', Preferences(directory).snapshot()['language'])
            with patch('preferences.windows_language', return_value='de-DE'), patch('preferences.windows_theme', return_value='light'):
                self.assertEqual('light', prefs.snapshot()['theme'])
                self.assertEqual('en', prefs.set_language('system')['language'])
            with patch('preferences.windows_language', return_value='ko-KR'):
                self.assertEqual('ko', prefs.snapshot()['language'])

    def test_invalid_selection_does_not_replace_setting(self):
        with tempfile.TemporaryDirectory() as directory:
            prefs = Preferences(directory)
            prefs.set_language('ko')
            with self.assertRaises(ValueError):
                prefs.set_language('../other')
            self.assertEqual('ko', Preferences(directory).selection)
            prefs.path.write_text('{broken', encoding='utf-8')
            self.assertEqual('en', Preferences(directory).selection)
