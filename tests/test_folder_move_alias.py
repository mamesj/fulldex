from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class FolderMoveAliasTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ required')
    def test_same_file_moves_and_legacy_recovery_keep_the_data(self):
        include=Path(os.environ.get('SSR_FOLDER_ALIAS_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);exe=root/'test';data=root/'data';data.mkdir()
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_folder_move_alias.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe),str(data)],check=True)
