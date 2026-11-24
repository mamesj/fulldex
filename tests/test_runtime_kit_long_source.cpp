#include "runtime_kit.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::string Read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
    assert(argc==2);auto root=fs::absolute(argv[1]);auto source=root/"source",dest=root/"destination";
    for(int i=0;i<4;++i){source/=std::string(80,'s');dest/=std::string(80,'d');}
    std::error_code ec;auto nativeSource=RuntimeKit::NativePath(source,ec);assert(!ec);
    auto nativeDest=RuntimeKit::NativePath(dest,ec);assert(!ec);
    fs::create_directories(nativeSource/fs::u8path(u8"Pokémon_日本語"));
    {std::ofstream file(nativeSource/fs::u8path(u8"Pokémon_日本語")/"payload");file<<"synthetic kit content";assert(file.good());}
    {std::ofstream file(nativeSource/"existing");file<<"master settings";assert(file.good());}
    fs::create_directories(nativeDest);
    {std::ofstream file(nativeDest/"existing");file<<"keep destination settings";assert(file.good());}
    if(!RuntimeKit::CopyMissing(source,dest,ec)){std::cerr<<"Long source copy failed: "<<ec.message()<<'\n';return 1;}
    assert(Read(nativeDest/fs::u8path(u8"Pokémon_日本語")/"payload")=="synthetic kit content");
    assert(Read(nativeDest/"existing")=="keep destination settings");
    assert(RuntimeKit::CopyMissing(nativeSource,nativeDest,ec)&&!ec);
    assert(Read(nativeSource/"existing")=="master settings");
    std::cout<<"Long source/destination enumeration copies exact Unicode contents and preserves existing settings.\n";
}
