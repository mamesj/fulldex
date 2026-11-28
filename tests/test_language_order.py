"""The languages' order is written in three places (ui_lang_codes.h Codes[], and the two catalogues'
"languages"); the generated headers look a language up by its code, so only these must agree."""
import json, re, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class LanguageOrderTests(unittest.TestCase):
    def test_same_order_everywhere(self):
        source = (ROOT / 'frontend/ui_lang_codes.h').read_text(encoding='utf-8')
        codes = re.search(r'Codes\[CodeCount\]\s*=\s*\{([^}]*)\}', source).group(1)
        codes = re.findall(r'"([a-z]+)"', codes)
        for name in ('translations', 'guide_translations'):
            languages = json.loads((ROOT / f'localisation/{name}.json').read_text(encoding='utf-8'))['languages']
            self.assertEqual(languages if languages[0] == 'en' else ['en'] + languages, codes, name)
    def test_no_bare_language_numbers(self):
        for name in ('ui_translations.h', 'guide_translations.h'):
            text = (ROOT / 'frontend' / name).read_text(encoding='utf-8')
            self.assertIsNone(re.search(r'\b(lang|language)\s*==\s*\d', text), name)
if __name__ == '__main__':
    unittest.main()
