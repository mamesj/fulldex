from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class N3dsAddonPlayTests(unittest.TestCase):
    # (Linux: the add-on goes in with LD_PRELOAD; on Windows it is the game folder's SDL2.dll)
    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('c++'), 'Linux compiler required')
    def test_play_loads_the_addon_in_front_of_others(self):
        for name in ('fulldex-3ds-addon-', 'fulldex 3ds addon '):   # (a space in the folder's path too)
            with self.subTest(folder=name), tempfile.TemporaryDirectory(prefix=name) as folder:
                binary = Path(folder) / 'n3ds-addon-play'
                subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                                str(ROOT / 'tests/test_n3ds_addon_play.cpp'), '-o', str(binary)], check=True)
                env = dict(os.environ, HOME=folder, XDG_DATA_HOME=folder, XDG_CONFIG_HOME=folder)
                subprocess.run([str(binary), folder], check=True, timeout=60, env=env)


if __name__ == '__main__':
    unittest.main()
