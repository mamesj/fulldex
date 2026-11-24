from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CloudFolderErrorTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_unreadable_remote_is_not_a_missing_save(self):
        source=Path(os.environ.get('SSR_CLOUD_ERRORS_SOURCE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory(prefix='fulldex-cloud-errors-') as folder:
            binary=Path(folder)/('test.exe' if os.name=='nt' else 'test')
            subprocess.run(['c++','-std=c++17','-pthread','-I'+str(source),str(ROOT/'tests/test_cloud_folder_errors.cpp'),str(source/'cloud_saves.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary),folder],check=True,timeout=15)
