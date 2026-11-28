#pragma once
#include "ui_lang_codes.h"
// The menu language for title case, a position in UiLang::Codes (English is UiLang::En). Outside
// English a lone Y, E, O or U between words is "and"/"or", so it is written small, not kept as the Y
// button. (Its own header so the font code and UiLang share one, with no #ifndef guard to keep in step.)
inline int& UiTitleLang() { static int l = UiLang::En; return l; }
