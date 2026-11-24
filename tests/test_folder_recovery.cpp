#include "folders.h"
#include <cassert>
#include <iostream>
namespace fs=std::filesystem;
static void put(const fs::path& p,const char* text){std::ofstream(p)<<text;}
static std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){
    assert(argc==2);fs::path root=argv[1];fs::create_directories(root/"source");fs::create_directories(root/"destination");
    auto a=root/"source/a.sav",b=root/"source/b.sav",x=root/"destination/a.sav",y=root/"destination/b.sav",journal=root/"move.txt";
    put(a,"A");put(b,"B");put(x,"A"); // A predated the move; B has not been copied.
    FolderSet::Pairs pairs{{a.string(),x.string()},{b.string(),y.string()}};
    std::vector<std::string> keep{(root/"source").string(),(root/"destination").string()};
    assert(FolderSet::WriteJournal(journal.string(),"copying",pairs));
    assert(FolderSet::FinishCutShort(journal.string(),keep));
    assert(read(x)=="A"&&read(a)=="A"&&read(b)=="B"&&!fs::exists(y));
    assert(!fs::exists(journal));
    // An equal copy made just before interruption is also retained, then reused.
    put(y,"B");assert(FolderSet::WriteJournal(journal.string(),"copying",pairs));
    assert(FolderSet::FinishCutShort(journal.string(),keep));assert(read(y)=="B");
    std::string why;assert(FolderSet::Move(pairs,journal.string(),keep,why));
    assert(!fs::exists(a)&&!fs::exists(b)&&read(x)=="A"&&read(y)=="B");
    // Recovery once source removal started keeps the destination and completes removal.
    put(a,"A");put(b,"B");assert(FolderSet::WriteJournal(journal.string(),"removing",pairs));
    assert(FolderSet::FinishCutShort(journal.string(),keep));
    assert(!fs::exists(a)&&!fs::exists(b)&&read(x)=="A"&&read(y)=="B");
    // An incomplete, differing destination is never overwritten or removed by recovery.
    put(a,"A");put(b,"B");put(y,"partial");assert(FolderSet::WriteJournal(journal.string(),"copying",pairs));
    assert(FolderSet::FinishCutShort(journal.string(),keep));
    assert(read(a)=="A"&&read(b)=="B"&&read(x)=="A"&&read(y)=="partial");
    assert(!FolderSet::Move(pairs,journal.string(),keep,why));
    assert(read(a)=="A"&&read(b)=="B"&&read(x)=="A"&&read(y)=="partial");
    std::cout<<"Interrupted folder recovery preserves pre-existing, complete and partial destination files; safe retry and removal stage pass\n";
}
