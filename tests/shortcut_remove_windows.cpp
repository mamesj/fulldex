#include <windows.h>
#include <shlobj.h>
#include <string>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
static std::wstring testDesktop;
static HRESULT TestDesktop(REFKNOWNFOLDERID,DWORD,HANDLE,PWSTR* out){
    assert(!testDesktop.empty());const auto bytes=(testDesktop.size()+1)*sizeof(wchar_t);
    *out=static_cast<PWSTR>(CoTaskMemAlloc(bytes));if(!*out)return E_OUTOFMEMORY;
    memcpy(*out,testDesktop.c_str(),bytes);return S_OK;
}
#define SHGetKnownFolderPath TestDesktop
#define STB_IMAGE_IMPLEMENTATION
#include "shortcuts.h"
#undef SHGetKnownFolderPath
namespace fs=std::filesystem;
static void put(const fs::path& p){std::ofstream(p)<<"dummy official shortcut";}
int main(int argc,char**argv){
    assert(argc==2);auto root=fs::absolute(fs::u8path(argv[1]));
    fs::create_directories(root/"Desktop");fs::create_directories(root/"shortcut-icons");testDesktop=(root/"Desktop").wstring();
    Launcher::Game official;official.code="IRBO";official.known=true;official.rom="fictional.nds";
    const auto link=root/"Desktop"/fs::u8path(Shortcuts::Title(official)+".lnk");
    const auto icon=root/"shortcut-icons/IRBO.ico";put(link);put(icon);
    Launcher::Game hack=official;hack.hack=true;hack.name="Example Hack";
    std::string where;const bool found=Shortcuts::Exists(root.u8string(),hack);
    const bool removed=Shortcuts::Remove(root.u8string(),hack,where);
    if(removed)std::cerr<<"unsupported hack removed official shortcut="<<!fs::exists(link)<<std::endl;
    assert(!found&&!removed&&fs::exists(link)&&fs::exists(icon));
    auto unknown=official;unknown.known=false;
    assert(!Shortcuts::Exists(root.u8string(),unknown));assert(!Shortcuts::Remove(root.u8string(),unknown,where));
    assert(fs::exists(link)&&fs::exists(icon));
    assert(Shortcuts::Exists(root.u8string(),official));assert(Shortcuts::Remove(root.u8string(),official,where));
    assert(!fs::exists(link)&&!fs::exists(icon));
    std::cout<<"Actual Windows shortcut removal preserved the official link/icon for unsupported titles and removed the intended supported shortcut\n";
}
