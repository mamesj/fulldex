"""One registry (localisation/languages.json) names the languages; the catalogues and the generated headers
must agree with it, and no generated header may look a language up by a bare number."""
import json, re, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def registry():
    return [l['code'] for l in json.loads((ROOT / 'localisation/languages.json').read_text(encoding='utf-8'))['languages']]
class LanguageRegistryTests(unittest.TestCase):
    def test_english_first_and_unique(self):
        codes = registry()
        self.assertEqual(codes[0], 'en')
        self.assertEqual(len(set(codes)), len(codes))
    def test_catalogues_have_exactly_the_registry_languages(self):
        for name in ('translations', 'guide_translations'):
            tables = json.loads((ROOT / f'localisation/{name}.json').read_text(encoding='utf-8'))['tables']
            self.assertEqual(sorted(tables), sorted(registry()[1:]), name)
    def test_companion_tables_are_languages_of_the_registry(self):
        tables = json.loads((ROOT / 'localisation/companion_translations.json').read_text(encoding='utf-8'))['tables']
        self.assertTrue(set(tables) <= set(registry()[1:]))
        flagged = [l['code'] for l in json.loads((ROOT / 'localisation/languages.json').read_text(encoding='utf-8'))['languages'] if l.get('companion')]
        self.assertTrue(set(flagged) <= set(tables), 'a companion: true language has no companion table')
    def test_header_codes_follow_the_registry(self):
        source = (ROOT / 'frontend/ui_lang_codes.h').read_text(encoding='utf-8')
        codes = re.findall(r'"([a-z]+)"', re.search(r'Codes\[CodeCount\]\s*=\s*\{([^}]*)\}', source).group(1))
        self.assertEqual(codes, registry())
    def test_no_bare_language_numbers(self):
        for name in ('ui_translations.h', 'guide_translations.h', 'ui_lang_codes.h', 'companion_translations.h'):
            text = (ROOT / 'frontend' / name).read_text(encoding='utf-8')
            self.assertIsNone(re.search(r'\b(lang|language)\s*==\s*\d', text), name)
            self.assertIsNone(re.search(r'\bLang\(\)\s*\?|\bstatic int l\s*=\s*\d|\blang(uage)?\s*\?', text), name + ': English tested as a bare 0')
if __name__ == '__main__':
    unittest.main()
