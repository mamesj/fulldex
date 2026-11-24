#include "n3ds.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static void Put(const fs::path& path,const char* value) {fs::create_directories(path.parent_path());std::ofstream(path)<<value;}
static std::string Read(const fs::path& path) {std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char**argv) {
    assert(argc==2);const auto root=fs::absolute(argv[1]);
    const auto kit=fs::u8path(N3ds::ProgramDir(root.string()));
    Put(fs::u8path(N3ds::Exe(kit.string())),"synthetic executable");Put(kit/"tools/translator","synthetic translator");
    for(bool blockedExe:{true,false}) {
        Launcher::Game game;game.folder=(root/(blockedExe?"blocked-executable":"blocked-tool")).string();game.rom=(root/"synthetic.3ds").string();
        Put(fs::u8path(game.rom),"synthetic header");
        auto target=fs::u8path(game.folder);
        const auto obstruction=blockedExe?fs::u8path(N3ds::Exe(game.folder)):target/"tools/translator";
        fs::create_directories(obstruction);
        Put(target/"data/user/main","keep player progress");
        if(N3ds::MakeFolder(root.string(),game)) {std::cerr<<"Directory obstruction falsely reported as installed\n";return 1;}
        assert(fs::is_directory(obstruction));assert(!fs::exists(target/"rom.txt"));
        assert(Read(target/"data/user/main")=="keep player progress");assert(Read(fs::u8path(game.rom))=="synthetic header");
        assert(fs::remove(obstruction));assert(N3ds::MakeFolder(root.string(),game));
        assert(fs::is_regular_file(fs::u8path(N3ds::Exe(game.folder))));assert(Read(target/"tools/translator")=="synthetic translator");
        assert(Read(target/"data/user/main")=="keep player progress");
    }
#ifndef _WIN32
    const auto special=root/"special";fs::create_directories(special/"tools");
    assert(mkfifo((special/"tools/translator").c_str(),0600)==0);
    std::error_code ec;assert(!RuntimeKit::CopyMissing(kit,special,ec)&&ec);
    assert(fs::is_fifo(special/"tools/translator"));
#endif
    std::cout<<"Blocked executable/tool paths refuse setup, preserve progress and retry; ordinary existing files remain supported.\n";
}
