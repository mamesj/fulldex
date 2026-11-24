#include <filesystem>
#include <fstream>
#include <string>
#include <cassert>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "copy_function.h"
namespace fs=std::filesystem;
static void put(const fs::path& p,const char* data){std::ofstream(p)<<data;}
static std::string read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv)
{
    assert(argc==2);const fs::path root=argv[1];std::string said;
    const auto source=root/"saves",sibling=root/"saves-backup";
    fs::create_directories(source/"nested");put(source/"game.sav","current");put(source/"nested/other.sav","other");
    assert(CopySavesTo(source.string(),sibling.string(),said)==2);
    assert(read(sibling/"game.sav")=="current");
    put(sibling/"game.sav","destination");
    assert(CopySavesTo(source.string(),sibling.string(),said)==0);
    assert(read(sibling/"game.sav")=="destination" && read(source/"game.sav")=="current");
    assert(CopySavesTo(source.string(),(source/"child").string(),said)==-1);
    assert(CopySavesTo(source.string(),source.string(),said)==-1);
#ifdef _WIN32
    assert(CopySavesTo(source.string(),(root/"SAVES"/"child").string(),said)==-1);
#endif
    assert(CopySavesTo((root/"missing").string(),(root/"missing-target").string(),said)==-1);
    const auto blocked=root/"blocked";fs::create_directories(blocked);put(blocked/"nested","obstruction");
    assert(CopySavesTo(source.string(),blocked.string(),said)==-1);
    assert(read(blocked/"nested")=="obstruction");
    const auto fileBlocked=root/"file-blocked";fs::create_directories(fileBlocked/"game.sav");
    assert(CopySavesTo(source.string(),fileBlocked.string(),said)==-1);
    assert(fs::is_directory(fileBlocked/"game.sav")&&read(source/"game.sav")=="current");
#ifndef _WIN32
    // Source links must not escape into a sibling of the chosen destination.
    const auto links=root/"links",outside=root/"outside",destination=root/"destination";
    fs::create_directories(links);fs::create_directories(outside);
    put(outside/"private.sav","outside");
    fs::create_symlink(outside/"private.sav",links/"linked.sav");
    assert(CopySavesTo(links.string(),destination.string(),said)==-1);
    assert(read(outside/"private.sav")=="outside");
    assert(!fs::exists(destination/"linked.sav"));
    const auto otherDestination=root/"output/destination";
    fs::create_directories(root/"output/outside");
    assert(CopySavesTo(links.string(),otherDestination.string(),said)==-1);
    assert(!fs::exists(root/"output/outside/private.sav"));
    fs::remove(links/"linked.sav");
    fs::create_directory_symlink(outside,links/"linked-dir");
    assert(CopySavesTo(links.string(),destination.string(),said)==-1);
    fs::remove(links/"linked-dir");
    fs::create_symlink(outside/"absent.sav",links/"broken.sav");
    assert(CopySavesTo(links.string(),destination.string(),said)==-1);
    fs::remove(links/"broken.sav");
    // Neither an existing linked destination directory nor a dangling file link
    // may redirect writes outside the destination. Original files stay untouched.
    fs::create_directory_symlink(outside,destination/"nested");
    assert(CopySavesTo(source.string(),destination.string(),said)==-1);
    assert(!fs::exists(outside/"other.sav"));
    fs::remove(destination/"nested");
    fs::remove(destination/"game.sav");
    fs::create_symlink(outside/"absent.sav",destination/"game.sav");
    assert(CopySavesTo(source.string(),destination.string(),said)==-1);
    assert(!fs::exists(outside/"absent.sav"));
    // A linked folder deliberately selected as the root resolves normally.
    const auto alias=root/"chosen-alias",real=root/"chosen-real";
    fs::create_directories(real);fs::create_directory_symlink(real,alias);
    assert(CopySavesTo(source.string(),alias.string(),said)==2);
    assert(read(real/"nested/other.sav")=="other");
    if(geteuid()!=0)
    {
        fs::permissions(source,fs::perms::none);
        assert(CopySavesTo(source.string(),(root/"unreadable-target").string(),said)==-1);
        fs::permissions(source,fs::perms::owner_all);
        fs::permissions(source/"nested",fs::perms::none);
        assert(CopySavesTo(source.string(),(root/"nested-unreadable-target").string(),said)==-1);
        fs::permissions(source/"nested",fs::perms::owner_all);
        std::cout<<"Unreadable source and nested-directory cases exercised as non-root\n";
    }
#endif
    std::cout<<"Save-folder sibling copies, preservation, containment, missing/read errors and directory obstructions passed\n";
}
