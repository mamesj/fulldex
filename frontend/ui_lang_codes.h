#pragma once
// The menus' languages. This is the only place a language's position is written down: the enum
// below names each one, Codes[] and Names[] follow its order, and static_asserts stop the build if
// they ever disagree. Everything else says UiLang::Ja, never a bare number or a loose "ja".
// (The generated tables look a language up by its code; tests/test_language_order.py checks they agree.)
namespace UiLang
{
// The position of each language: UiLang::lang is one of these (an int, as it indexes tables).
enum Id : int { En, Es, It, Fr, De, Pt, Ja, Ko, CodeCount };
inline constexpr const char* Codes[CodeCount] = {"en","es","it","fr","de","pt","ja","ko"};   // (the locale codes, as saved)
inline constexpr const char* Names[CodeCount] = {"ENGLISH","ESPAÑOL","ITALIANO","FRANÇAIS","DEUTSCH","PORTUGUÊS","日本語","한국어"}; // (as the menu shows them)
constexpr bool SameCode(const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return *a == *b; }
static_assert(SameCode(Codes[En],"en") && SameCode(Codes[Es],"es") && SameCode(Codes[It],"it") && SameCode(Codes[Fr],"fr") &&
              SameCode(Codes[De],"de") && SameCode(Codes[Pt],"pt") && SameCode(Codes[Ja],"ja") && SameCode(Codes[Ko],"ko"),
              "Codes[] is out of step with enum UiLang::Id");
static_assert(SameCode(Names[Pt],"PORTUGUÊS") && SameCode(Names[Ja],"日本語"), "Names[] is out of step with enum UiLang::Id");
// English is the language the text is written in: it needs no table, so it is never translated.
inline constexpr bool IsEnglish(int l) { return l == En; }
// The languages whose companion pages have their own text (es/it/fr/de); the rest fall back to English.
inline constexpr bool HasCompanionText(int l) { return l == Es || l == It || l == Fr || l == De; }
}
