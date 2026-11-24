#include "folders.h"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
    assert(argc==3);const std::string path=argv[1];const bool expect=std::string(argv[2])=="success";
    FolderSet::Pairs pairs{{"source/game.sav","destination/game.sav"}};
    const bool result=FolderSet::WriteJournal(path,"copying",pairs);
    if(result!=expect)std::cerr<<"journal write unexpectedly "<<(result?"succeeded":"failed")<<std::endl;
    assert(result==expect);
    if(expect){std::string stage;FolderSet::Pairs read;assert(FolderSet::ReadJournal(path,stage,read));assert(stage=="copying"&&read==pairs);}
    else{std::ifstream f(path);std::string old;std::getline(f,old);assert(old=="existing-good-journal");}
}
