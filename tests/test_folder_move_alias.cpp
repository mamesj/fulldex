#include "folders.h"
#include <cassert>
#include <iostream>
namespace fs=std::filesystem;
static void put(const fs::path& p,const char* text){std::ofstream(p)<<text;}
static std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
    assert(argc==2);fs::path root=argv[1],source=root/"only.sav",journal=root/"move.txt";
    put(source,"only copy");std::string why;std::vector<std::string> keep{root.string()};
    bool moved=FolderSet::Move({{source.string(),source.string()}},journal.string(),keep,why);
    if(moved)std::cerr<<"Move accepted a same-file alias; source exists="<<fs::exists(source)<<std::endl;
    assert(!moved&&read(source)=="only copy"&&!fs::exists(journal));
    auto hard=root/"hard.sav";std::error_code linkError;fs::create_hard_link(source,hard,linkError);
#ifndef _WIN32
    assert(!linkError);
#endif
    if(!linkError){
        assert(!FolderSet::Move({{source.string(),hard.string()}},journal.string(),keep,why));
        assert(read(source)=="only copy"&&read(hard)=="only copy");
    }else std::cout<<"Hard-link case unavailable on this filesystem: "<<linkError.message()<<"\n";
#ifndef _WIN32
    auto real=root/"real",alias=root/"alias";fs::create_directory(real);fs::create_directory_symlink(real,alias);
    put(real/"game.sav","aliased only copy");
    assert(!FolderSet::Move({{(real/"game.sav").string(),(alias/"game.sav").string()}},journal.string(),keep,why));
    assert(read(real/"game.sav")=="aliased only copy"&&read(alias/"game.sav")=="aliased only copy");
#endif
    // An old journal can already contain this plan, before the new move guard.
    assert(FolderSet::WriteJournal(journal.string(),"removing",{{source.string(),source.string()}}));
    assert(FolderSet::FinishCutShort(journal.string(),keep));assert(read(source)=="only copy");
    // Distinct equal files still move normally, and removal recovery still completes.
    auto destination=root/"separate.sav";put(destination,"only copy");
    assert(FolderSet::Move({{source.string(),destination.string()}},journal.string(),keep,why));
    assert(!fs::exists(source)&&read(destination)=="only copy");
    put(source,"only copy");assert(FolderSet::WriteJournal(journal.string(),"removing",{{source.string(),destination.string()}}));
    assert(FolderSet::FinishCutShort(journal.string(),keep));assert(!fs::exists(source)&&read(destination)=="only copy");
    std::cout<<"Move and legacy recovery preserve same-file, hard-link and directory-alias data; distinct-copy moves still work\n";
}
