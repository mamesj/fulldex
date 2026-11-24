from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]

def functions():
    source=Path(os.environ.get('SSR_OWN_FOLDER_SOURCE',ROOT/'frontend/pc_launcher.cpp')).read_text(encoding='utf-8')
    start=source.index('std::string UseOwnFolder(Folders& fo,')
    start=source.index('std::string UseOwnFolder(Folders& fo,',start+1) # after its declaration
    def body(at):
        end=source.index('{',at)+1;depth=1
        while depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        return source[at:end]
    helper=source.find('static int CopyOwnFolder(')
    return (body(helper)+'\n' if helper>=0 else '')+body(start)

class OwnFolderCopyTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'),'C++ required')
    def test_folder_setting_changes_only_after_successful_copy(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'own_folder_functions.h').write_text(functions())
            exe=root/'test'
            subprocess.run(['c++','-std=c++17','-I'+str(ROOT/'frontend'),'-I'+folder,
                            str(ROOT/'tests/test_own_folder_copy.cpp'),'-o',str(exe)],check=True)
            data=root/'data';data.mkdir()
            subprocess.run([str(exe),str(data)],check=True)
