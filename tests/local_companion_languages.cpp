#include "ui_lang.h"
#include <cassert>
#include <iostream>
int main(){
 UiLang::SetSystem("fr");UiLang::Set("auto");assert(UiLang::lang==UiLang::Fr);
 for(const auto& code:{"en","es","it","fr","de","pt"}){
  UiLang::Set(code);assert(UiLang::choice==code);
  assert(UiLang::Tr("FULLDEX_UNKNOWN_FALLBACK")=="FULLDEX_UNKNOWN_FALLBACK");
  if(!UiLang::IsEnglish(UiLang::lang))assert(UiLang::Tr("PARTY")!="PARTY");
 }
 UiLang::Set("unsupported");assert(UiLang::choice=="auto"&&UiLang::lang==UiLang::Fr);
 std::cout<<"PASS: existing languages, French/German/Portuguese selection, automatic locale and English fallback\n";
}
