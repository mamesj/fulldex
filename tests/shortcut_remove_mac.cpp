#define STB_IMAGE_IMPLEMENTATION
#include "shortcuts.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char**argv){
    assert(argc==2);auto root=fs::absolute(fs::u8path(argv[1]));
    assert(std::string(getenv("HOME"))==root.string());
    Launcher::Game official;official.code="IRBO";official.known=true;official.rom="fictional.nds";
    const auto name=Shortcuts::Title(official)+".app";
    auto app=root/"Applications"/name,desktop=root/"Desktop"/name;
    fs::create_directories(app);fs::create_directories(desktop);
    std::ofstream(app/"dummy")<<"official";std::ofstream(desktop/"dummy")<<"official";
    Launcher::Game hack=official;hack.hack=true;hack.name="Example Hack";
    std::string where;bool found=Shortcuts::Exists(root.string(),hack);
    bool removed=Shortcuts::Remove(root.string(),hack,where);
    if(removed)std::cerr<<"unsupported hack removed official app="<<!fs::exists(app)<<std::endl;
    assert(!found&&!removed&&fs::exists(app/"dummy")&&fs::exists(desktop/"dummy"));
    auto unknown=official;unknown.known=false;
    assert(!Shortcuts::Exists(root.string(),unknown));assert(!Shortcuts::Remove(root.string(),unknown,where));
    assert(fs::exists(app)&&fs::exists(desktop));
    assert(Shortcuts::Exists(root.string(),official));assert(Shortcuts::Remove(root.string(),official,where));
    assert(!fs::exists(app)&&!fs::exists(desktop));
    std::cout<<"Actual macOS shortcut branch preserves official bundles for unsupported titles; intended supported removal works\n";
}
