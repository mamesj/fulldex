"""Every launcher-xx.txt (frontend/lang) translates the same English lines, and keeps each {} place."""
import re, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def read(path):
    out = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#') or '\t' not in line: continue
        key, value = line.split('\t', 1)
        out.setdefault(key.lower(), (key, value))
    return out
def holes(s): return len(re.findall(r'\{#\}|\{\}', s))
class LauncherLanguageTests(unittest.TestCase):
    def test_same_lines_and_places_in_every_language(self):
        files = sorted((ROOT / 'frontend/lang').glob('launcher-*.txt'))
        self.assertTrue(files)
        tables = {f.stem[len('launcher-'):]: read(f) for f in files}
        english = set(tables['de'])
        for code, table in tables.items():
            self.assertEqual(set(table), english, code)
            for key, (original, value) in table.items():
                self.assertTrue(value.strip(), f'{code}: empty translation for {original!r}')
                self.assertEqual(holes(original), holes(value), f'{code}: places differ in {original!r}')
    def test_generated_header_has_every_language(self):
        header = (ROOT / 'frontend/launcher_lang_data.h').read_text(encoding='utf-8')
        for f in (ROOT / 'frontend/lang').glob('launcher-*.txt'):
            code = f.stem[len('launcher-'):]
            self.assertIn(f'kLauncherLang_{code}', header, code)
            self.assertIn(f'!strcmp(code, "{code}")', header, code)
if __name__ == '__main__':
    unittest.main()
