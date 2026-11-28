"""The menus' languages, read from localisation/languages.json (shared by the generators)."""
from pathlib import Path
import json
ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'localisation/languages.json'
def load():
    langs = json.loads(SRC.read_text(encoding='utf-8'))['languages']
    codes = [l['code'] for l in langs]
    if codes[0] != 'en': raise SystemExit('languages.json: English must come first (it is the language the text is written in)')
    if len(set(codes)) != len(codes): raise SystemExit('languages.json: a language code is repeated')
    return langs
def codes(): return [l['code'] for l in load()]
def flagged(key): return [l['code'] for l in load() if l.get(key)]  # (the languages with a property on in languages.json)
def others(): return codes()[1:]  # (every language but English: the ones with a catalogue)
def ident(code): return code[:1].upper() + code[1:]  # (the C++ name of a language: "pt" -> Pt)
