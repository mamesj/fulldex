from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
def function():
    source=Path(os.environ.get('SSR_ENUM_SOURCE',ROOT/'frontend/pc_launcher.cpp')).read_text(encoding='utf-8')
    signature='std::string SwitchFolders(Folders& fo,'
    start=source.index(signature);start=source.index(signature,start+1)
    end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
class FolderEnumerationTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ required')
    def test_layout_and_undo_require_complete_listing(self):
        include=Path(os.environ.get('SSR_ENUM_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'switch_folders_function.h').write_text(function())
            exe=root/'test';data=root/'data';data.mkdir()
            subprocess.run(['c++','-std=c++17','-I'+str(include),'-I'+folder,str(ROOT/'tests/test_folder_enumeration.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe),str(data)],check=True)
