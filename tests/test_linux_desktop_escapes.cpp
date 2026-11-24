#define STB_IMAGE_IMPLEMENTATION
#include "shortcuts.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::string quoted(const std::string& value) {
    std::string text="\"";
    for(char c:value) { if(c=='"'||c=='\\'||c=='$'||c=='`')text+='\\'; text+=c; }
    return text+'"';
}
int main(int argc,char**argv) {
    assert(argc==2);const auto root=fs::absolute(argv[1]);
    fs::create_directories(root/".config");
    Launcher::Game game;game.code="IRBO";game.known=true;game.hasIcon=true;game.rom=(root/"synthetic.nds").string();
    std::fill(std::begin(game.icon),std::end(game.icon),0xff4080c0);
    for(const std::string name:{"quote\"desktop", "back\\slash", "cash$desktop", "tick`desktop", "literal$HOME", "Desktop ", "桌面é"}) {
        const auto desktop=root/name;fs::create_directories(desktop);
        {std::ofstream out(root/".config/user-dirs.dirs");out<<"XDG_DESKTOP_DIR="<<quoted(desktop.string())<<"\n";}
        if(Shortcuts::LinuxDesktopDir()!=desktop.string()) {std::cerr<<"Wrong decoded desktop path: "<<name<<'\n';return 1;}
        std::string where;assert(Shortcuts::Make(root.string(),{game},where)==1);
        const auto file="pokemon-ds-wide-"+Shortcuts::Lower(Shortcuts::LinuxShortcutId(game))+".desktop";
        assert(fs::is_regular_file(desktop/file));assert(Shortcuts::Exists(root.string(),game));
        assert(Shortcuts::Remove(root.string(),game,where));assert(!fs::exists(desktop/file));
    }
    // Unsupported shell expressions are not interpreted as directory names.
    {std::ofstream out(root/".config/user-dirs.dirs");out<<"  XDG_DESKTOP_DIR=\"$HOME/Good\" # configured desktop\nXDG_DESKTOP_DIR=\"$HOMEevil/Bad\"\nXDG_DESKTOP_DIR=\"$(touch never-run)\"\nXDG_DESKTOP_DIR=\"unterminated\n";}
    assert(Shortcuts::LinuxDesktopDir()==(root/"Good").string());assert(!fs::exists(root/"never-run"));
    // Do not truncate a valid setting at an arbitrary 1024-byte input buffer.
    std::string longpath=root.string();for(int i=0;i<15;++i)longpath+="/"+std::string(80,'a');
    {std::ofstream out(root/".config/user-dirs.dirs");out<<"XDG_DESKTOP_DIR="<<quoted(longpath)<<'\n';}
    assert(Shortcuts::LinuxDesktopDir()==longpath);
    std::cout<<"Escaped desktop paths: actual create/find/remove, malformed assignments and long values passed.\n";
}
