# Interface languages

The launcher and Fulldex menus support English, Spanish, Italian, French, German, Japanese, Korean and Portuguese (Brazil). AUTO follows a supported system language, otherwise English. This setting does not translate or modify a ROM. Embedded GBA menus inherit the launcher language. GBA game translations, regional compatibility and widescreen patches are unchanged.

`translations.json` is the canonical UTF-8 catalogue. Existing Spanish and Italian wording, including AlessioGiacobbe’s PR9 improvements, was retained. Additional translations, including Portuguese, began as machine drafts, with common menu terminology and dynamic actions reviewed. Native-speaker wording feedback is welcome. Every non-English catalogue has the same 1375 keys; the Guide catalogue (`guide_translations.json`) has 445. Portuguese uses Brazilian vocabulary and conventions throughout; the Guide's own `CompanionTranslation` pages (PokeDaisy) are not yet translated into Portuguese and fall back to English, as they do for Japanese and Korean.

## Adding or moving a language

`languages.json` is the one list of languages: each one's code, the name the menu shows, and what it needs (`tall` letters, `companion` pages with their own text, a menu in the `omega_nx` game). The order there is the order the app indexes them and the settings menu shows. To add a language: add it there, add its table to `translations.json` and `guide_translations.json`, then run `generate_languages.py`, `generate_translations.py` and `generate_guide_translations.py` (each takes `--check`). Code refers to a language as `UiLang::Pt` (never a bare number or `"pt"`), so nothing else changes when one is added or moved. `tests/test_language_order.py` checks the catalogues and generated headers agree with `languages.json`.

Run `python3 tools/localisation/generate_translations.py` after editing; `--check` verifies the generated header. UI labels, control names and runtime user/account names must remain distinct. Templates preserve names rather than translating user data.

Fonts are embedded monochrome derivatives of Noto Sans CJK JP/KR, licensed under SIL OFL1.1. See `LICENSE-Noto.txt`. The generator pins original font SHA256 values and requires Pillow; runtime needs no font installation or extra library. To regenerate: `python3 tools/localisation/generate_font.py --japanese NotoSansCJKjp-Regular.otf --korean NotoSansCJKkr-Regular.otf`. Official font source: https://github.com/notofonts/noto-cjk/tree/main/Sans/OTF.

The GBA overlay is prepared in the build directory by `prepare_gba.py`; original GBA sources are read-only. UI text and editor geometry are shared; CPU, emulator and game data are unchanged. All five platform packaging scripts include the font licence.
