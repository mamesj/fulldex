from pathlib import Path
import os, shutil, subprocess, sys, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class RuntimeKitTypeTests(unittest.TestCase):
    @unittest.skipUnless((sys.platform.startswith('linux') or os.name=='nt') and shutil.which('c++'),'Windows/Linux compiler required')
    def test_obstructed_files_are_not_treated_as_installed(self):
        include=Path(os.environ.get('SSR_KIT_TYPES_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-kit-types-') as folder:
            root=Path(folder);binary=root/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-pthread','-I'+str(include),str(ROOT/'tests/test_runtime_kit_types.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],check=True,timeout=15,env=dict(os.environ,HOME=folder,XDG_DATA_HOME=folder,XDG_CONFIG_HOME=folder))
