from pathlib import Path
import os,shutil,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class LongKitSourceTests(unittest.TestCase):
    @unittest.skipUnless((sys.platform.startswith('linux') or os.name=='nt') and shutil.which('c++'),'Windows/Linux compiler required')
    def test_long_source_and_destination_copy_exact_files(self):
        include=Path(os.environ.get('SSR_KIT_LONG_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-kit-long-') as folder:
            binary=Path(folder)/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_runtime_kit_long_source.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],check=True,timeout=15)
