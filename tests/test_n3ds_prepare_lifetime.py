from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PrepareLifetimeTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('c++'), 'Linux compiler required')
    def test_worker_state_survives_exit_teardown(self):
        with tempfile.TemporaryDirectory(prefix='fulldex-prep-lifetime-') as folder:
            binary=Path(folder)/'prepare-lifetime'
            subprocess.run(['c++', '-std=c++17', '-pthread', '-fsanitize=address', '-fno-omit-frame-pointer',
                            '-I'+str(ROOT/'frontend'), str(ROOT/'tests/test_n3ds_prepare_lifetime.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], env=dict(os.environ, HOME=folder, XDG_DATA_HOME=folder,
                           XDG_CONFIG_HOME=folder, ASAN_OPTIONS='halt_on_error=1'), check=True, timeout=10)
