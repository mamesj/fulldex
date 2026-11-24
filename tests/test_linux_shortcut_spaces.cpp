#define STB_IMAGE_IMPLEMENTATION
#include "shortcuts.h"
#include <cassert>
#include <fstream>
int main(int argc,char**argv) {
    if(argc>1) {
        std::ofstream out(getenv("FULLDEX_SPACE_RESULT"));
        out<<std::filesystem::current_path().u8string()<<'\n';
        for(int i=1;i<argc;i++)out<<argv[i]<<'\n';
        return 0;
    }
    const std::string root=getenv("HOME");
    Launcher::Game game;game.code="IRBO";game.known=true;game.hasIcon=true;
    game.customName="  Custom Name  ";game.rom=root+"/synthetic.nds";
    std::fill(std::begin(game.icon),std::end(game.icon),0xff4080c0);
    std::string where;assert(Shortcuts::Make(root,{game},where)==1);
}
