from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'gui'))
from instance import GuiInstance


class InstanceTests(unittest.TestCase):
    def test_app_repeat_launch_does_not_create_another_window(self):
        import app
        with tempfile.TemporaryDirectory() as directory:
            owner = GuiInstance(directory)
            self.assertTrue(owner.acquire(1))
            try:
                with patch.object(app, 'RUNTIME', Path(directory)), patch.object(sys, 'argv', ['app.py', '--host', '2']), patch.object(app.webview, 'create_window') as create:
                    app.main()
                    create.assert_not_called()
                self.assertEqual(2, owner.poll()['host'])
            finally:
                owner.close()

    def test_repeated_processes_forward_to_owner_and_lock_releases(self):
        with tempfile.TemporaryDirectory() as directory:
            owner = GuiInstance(directory)
            self.assertTrue(owner.acquire(1))
            script = "from instance import GuiInstance; import sys; i=GuiInstance(sys.argv[1]); print(i.acquire(int(sys.argv[2]))); i.close()"
            try:
                for host in (1, 2, 2):
                    child = subprocess.run([sys.executable, '-c', script, directory, str(host)],
                                           cwd=Path(__file__).resolve().parents[1] / 'gui',
                                           capture_output=True, text=True, timeout=10, check=True)
                    self.assertEqual('False', child.stdout.strip())
                    self.assertEqual(host, owner.poll()['host'])
                    self.assertIsNone(owner.poll())
            finally:
                owner.close()
            successor = GuiInstance(directory)
            try:
                self.assertTrue(successor.acquire(3))
            finally:
                successor.close()

    def test_stale_activation_does_not_switch_reopened_window(self):
        with tempfile.TemporaryDirectory() as directory:
            first = GuiInstance(directory)
            first.acquire(1)
            second = GuiInstance(directory)
            self.assertFalse(second.acquire(2))
            first.close()
            successor = GuiInstance(directory)
            try:
                self.assertTrue(successor.acquire(3))
                self.assertIsNone(successor.poll())
            finally:
                successor.close()


if __name__ == '__main__':
    unittest.main()
