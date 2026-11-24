from pathlib import Path
import os,shutil,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ShortcutRemovalErrorTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and sys.platform in ('linux','darwin'),'POSIX C++ required')
    def test_posix_removal_reports_permission_failures(self):
        include=Path(os.environ.get('SSR_SHORTCUT_ERRORS_INCLUDE',ROOT/'frontend'))
        for mode in (['linux','mac'] if sys.platform=='linux' else ['mac']):
            with self.subTest(mode=mode),tempfile.TemporaryDirectory() as folder:
                root=Path(folder);flags=[]
                if mode=='mac' and sys.platform=='linux':
                    sdk=root/'sdk/mach-o';sdk.mkdir(parents=True)
                    (sdk/'dyld.h').write_text('#pragma once\n#include <cstdint>\nextern "C" inline int _NSGetExecutablePath(char*,uint32_t*){return -1;}\n')
                    (root/'sdk/TargetConditionals.h').write_text('#pragma once\n#define TARGET_OS_IPHONE 0\n#define TARGET_OS_MAC 1\n')
                    flags=['-D__APPLE__','-I'+str(root/'sdk')]
                exe=root/'test';data=root/'data';data.mkdir()
                subprocess.run(['c++','-std=c++17',*flags,'-I'+str(include),str(ROOT/'tests/shortcut_remove_errors_posix.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(exe)],check=True)
                subprocess.run([str(exe),str(data)],env=dict(os.environ,HOME=str(data),XDG_DATA_HOME=str(data/'data'),XDG_CONFIG_HOME=str(data/'.config')),check=True)

    @unittest.skipUnless(shutil.which('c++') and sys.platform=='win32','Native Windows C++ required')
    def test_windows_removal_reports_readonly_failure(self):
        include=Path(os.environ.get('SSR_SHORTCUT_ERRORS_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);exe=root/'test.exe';data=root/'data';data.mkdir()
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/shortcut_remove_errors_windows.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(exe),'-lole32','-luuid','-lshell32','-lpropsys'],check=True)
            subprocess.run([str(exe),str(data)],check=True)
