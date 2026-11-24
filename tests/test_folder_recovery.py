from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class FolderRecoveryTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ required')
    def test_interrupted_move_preserves_existing_copies(self):
        include=Path(os.environ.get('SSR_FOLDER_RECOVERY_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);exe=root/'test';data=root/'data';data.mkdir()
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_folder_recovery.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe),str(data)],check=True)
