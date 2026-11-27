from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class N3dsHackTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('c++'), 'Linux compiler required')
    def test_hack_with_its_own_title_id_plays_from_a_retitled_copy(self):
        with tempfile.TemporaryDirectory(prefix='fulldex-3ds-hacks-') as folder:
            binary = Path(folder) / 'n3ds-hacks'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-I' + str(ROOT / 'frontend'),
                            str(ROOT / 'tests/test_n3ds_hacks.cpp'), '-o', str(binary)], check=True)
            env = dict(os.environ, HOME=folder, XDG_DATA_HOME=folder, XDG_CONFIG_HOME=folder)
            subprocess.run([str(binary), folder], check=True, timeout=60, env=env)


if __name__ == '__main__':
    unittest.main()
