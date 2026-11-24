from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
def copy_function():
    source=Path(os.environ.get('SSR_SAVE_COPY_SOURCE',ROOT/'frontend/saves_page.h')).read_text()
    start=source.index('inline int CopySavesTo('); opening=source.index('{',start)
    end=opening+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
class SaveFolderCopyTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ compiler required')
    def test_actual_save_folder_copy(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'copy_function.h').write_text(copy_function())
            exe=root/'test-copy'
            subprocess.run(['c++','-std=c++17','-I'+folder,str(ROOT/'tests/test_save_folder_copy.cpp'),'-o',str(exe)],check=True)
            fixtures=root/'fixtures';fixtures.mkdir()
            subprocess.run([str(exe),str(fixtures)],check=True)
if __name__=='__main__':unittest.main()
