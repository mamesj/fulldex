#include <windows.h>
#include <shlobj.h>
#include <string>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
static std::wstring testDesktop;
static HRESULT TestDesktop(REFKNOWNFOLDERID,DWORD,HANDLE,PWSTR* out){
    assert(!testDesktop.empty());auto bytes=(testDesktop.size()+1)*sizeof(wchar_t);
    *out=static_cast<PWSTR>(CoTaskMemAlloc(bytes));if(!*out)return E_OUTOFMEMORY;
    memcpy(*out,testDesktop.c_str(),bytes);return S_OK;
}
#define SHGetKnownFolderPath TestDesktop
#define STB_IMAGE_IMPLEMENTATION
#include "steam_shortcuts.h"
#undef SHGetKnownFolderPath
namespace fs=std::filesystem;
static void put(const fs::path& p){std::ofstream(p)<<"dummy shortcut";}
int main(int argc,char**argv){
    assert(argc==2);auto root=fs::absolute(fs::u8path(argv[1]));fs::create_directories(root/"Desktop");fs::create_directories(root/"shortcut-icons");
    testDesktop=(root/"Desktop").wstring();Launcher::Game g;g.code="IRBO";g.known=true;g.rom="fictional.nds";
    auto link=root/"Desktop"/fs::u8path(Shortcuts::Title(g)+".lnk"),icon=root/"shortcut-icons/IRBO.ico";
    put(link);put(icon);assert(SetFileAttributesW(link.c_str(),FILE_ATTRIBUTE_READONLY));
    SteamShortcuts::ActivePath().clear(); // No Steam discovery or account access.
    std::string where;const auto message=SteamShortcuts::PlatformEntryDo(root.u8string(),g);
    bool removed=message.rfind("REMOVED",0)==0;
    if(removed)std::cerr<<"readonly shortcut reported removed; link exists="<<fs::exists(link)<<"; icon exists="<<fs::exists(icon)<<std::endl;
    assert(SetFileAttributesW(link.c_str(),FILE_ATTRIBUTE_NORMAL));
    assert(!removed&&message.rfind("COULD NOT",0)==0&&fs::exists(link)&&fs::exists(icon));
    assert(Shortcuts::Remove(root.u8string(),g,where));assert(!fs::exists(link)&&!fs::exists(icon));
    assert(Shortcuts::Remove(root.u8string(),g,where)); // Missing is already removed.
    fs::create_directory(link);put(icon);
    assert(!Shortcuts::Remove(root.u8string(),g,where));assert(fs::is_directory(link)&&fs::exists(icon));
    std::cout<<"Actual Windows removal reports readonly/obstructed errors and preserves the icon; retry and missing entries work\n";
}
