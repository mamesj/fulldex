from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PortraitControlsTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'), 'C++ and SDL required')
    def test_portrait_controls_and_stacked_touch_mapping(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'portrait-controls'
            flags = shlex.split(subprocess.check_output(['sdl2-config', '--cflags', '--libs'], text=True))
            subprocess.run(['c++', '-std=c++17', '-O1', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_portrait_controls.cpp'), '-o', str(exe), *flags], check=True)
            subprocess.run([str(exe)], check=True)
