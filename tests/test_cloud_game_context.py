from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CloudContextTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_busy_old_game_cannot_remain_active_for_new_game(self):
        source=Path(os.environ.get('SSR_CLOUD_CONTEXT_SOURCE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-cloud-context-') as folder:
            binary=Path(folder)/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-pthread','-I'+str(source),str(ROOT/'tests/test_cloud_game_context.cpp'),str(source/'cloud_autosync.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],check=True,timeout=45)
