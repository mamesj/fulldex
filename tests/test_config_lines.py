from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CompleteConfigLinesTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('sdl2-config'),'C++ and SDL headers required')
    def test_real_helpers_preserve_long_lines_and_read_failures(self):
        source=Path(os.environ.get('FULLDEX_CONFIG_SOURCE',ROOT/'frontend'))
        text=(source/'launcher.h').read_text(encoding='utf-8')
        functions=[]
        for signature in ['inline std::string ConfigValue(', 'inline bool SetConfigLine(', 'inline void SetConfigValues(']:
            start=text.index(signature);end=text.index('\n}\n',start)+3
            functions.append(text[start:end])
        with tempfile.TemporaryDirectory(prefix='fulldex-config-lines-') as folder:
            (Path(folder)/'config_functions.h').write_text('namespace Launcher {\n'+functions[0]+functions[1]+'}\n'+functions[2],encoding='utf-8')
            exe=Path(folder)/'config-lines'
            flags=subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True).split()
            subprocess.run(['c++','-O2','-std=c++17','-I'+str(source),'-I'+folder,str(ROOT/'tests/test_config_lines.cpp'),'-o',str(exe)]+flags,check=True)
            subprocess.run([str(exe),folder],check=True,timeout=20)
