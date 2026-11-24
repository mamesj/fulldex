from pathlib import Path
import os,shutil,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class LinuxDesktopDirectoryTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform=='linux' and shutil.which('c++'),'Linux C++ required')
    def test_creation_lookup_and_removal_share_xdg_policy(self):
        include=Path(os.environ.get('SSR_XDG_SHORTCUT_INCLUDE',ROOT/'frontend'))
        modes=os.environ.get('SSR_XDG_TEST_MODES','disabled,home-slash,empty,custom,relative-config,relative-data,default').split(',')
        with tempfile.TemporaryDirectory(prefix='FullDex XDG ') as folder:
            root=Path(folder);exe=root/'test'
            subprocess.run(['c++','-std=c++17','-I'+str(include),str(ROOT/'tests/test_linux_desktop_directory.cpp'),str(ROOT/'frontend/launcher_cjk.cpp'),'-o',str(exe)],check=True)
            for mode in modes:
                data=root/mode;data.mkdir();env=dict(os.environ,HOME=str(data),XDG_DATA_HOME=str(data/'data'))
                env.pop('APPIMAGE',None);env.pop('XDG_CONFIG_HOME',None)
                if mode=='custom':env['XDG_CONFIG_HOME']=str(data/'xdg-config')
                if mode=='relative-config':env['XDG_CONFIG_HOME']='relative-ignored'
                if mode=='relative-data':env['XDG_DATA_HOME']='relative-ignored'
                subprocess.run([str(exe),str(data),mode],env=env,check=True)
