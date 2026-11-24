from pathlib import Path
import os,shutil,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ShortcutRemovalTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and sys.platform in ('linux','darwin'),'C++ and POSIX filesystem required')
    def test_mac_branch_preserves_other_games_shortcuts(self):
        include=Path(os.environ.get('SSR_SHORTCUT_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);flags=[]
            if sys.platform=='linux':
                # Only the unused executable-path SDK declaration is supplied. The
                # actual macOS file-existence/removal code runs on private POSIX files.
                sdk=root/'sdk/mach-o';sdk.mkdir(parents=True)
                (sdk/'dyld.h').write_text('#pragma once\n#include <cstdint>\nextern "C" int _NSGetExecutablePath(char*,uint32_t*);\n')
                (root/'sdk/TargetConditionals.h').write_text('#pragma once\n#define TARGET_OS_IPHONE 0\n#define TARGET_OS_MAC 1\n')
                flags=['-D__APPLE__','-I'+str(root/'sdk')]
            exe=root/'test'
            subprocess.run(['c++','-std=c++17',*flags,'-I'+str(include),str(ROOT/'tests/shortcut_remove_mac.cpp'),'-o',str(exe)],check=True)
            data=root/'data';data.mkdir()
            subprocess.run([str(exe),str(data)],env=dict(os.environ,HOME=str(data)),check=True)

    @unittest.skipUnless(shutil.which('c++') and sys.platform=='win32','Native Windows C++ required')
    def test_windows_branch_preserves_other_games_shortcuts(self):
        include=Path(os.environ.get('SSR_SHORTCUT_INCLUDE',ROOT/'frontend'))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);exe=root/'test.exe';data=root/'data';data.mkdir()
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/shortcut_remove_windows.cpp'),'-o',str(exe),'-lole32','-luuid','-lshell32','-lpropsys'],check=True)
            subprocess.run([str(exe),str(data)],check=True)
