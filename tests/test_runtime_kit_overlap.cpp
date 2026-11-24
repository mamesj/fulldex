#include "runtime_kit.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static std::string Read(const fs::path& path) {std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char**argv) {
    assert(argc==2);const auto root=fs::absolute(argv[1]);const auto source=root/"source";
    fs::create_directories(source);std::ofstream(source/"payload")<<"unchanged kit";
    std::error_code ec;
    if(RuntimeKit::CopyMissing(source,source,ec)) {std::cerr<<"Overlapping kit copy falsely accepted\n";return 1;}
    assert(ec);assert(Read(source/"payload")=="unchanged kit");
#ifdef _WIN32
    const auto upper=root/"SOURCE";
    if(fs::exists(upper)) {
        assert(!RuntimeKit::CopyMissing(source,upper/"case-nested",ec)&&ec);
        assert(!fs::exists(source/"case-nested"));
        std::cout<<"Windows case-variant alias refused before writing.\n";
    } else std::cout<<"Case-sensitive directory: case-variant alias unavailable.\n";
    if(fs::exists(root/"junction")) {
        assert(!RuntimeKit::CopyMissing(source,root/"junction"/"junction-nested",ec)&&ec);
        assert(!fs::exists(source/"inside"/"junction-nested"));
        std::cout<<"Windows junction into a source subdirectory refused before writing.\n";
    } else std::cout<<"Junction fixture not provided: junction case not verified in this run.\n";
#endif
    for(const auto target:{source/"new directory",root,source/".",root/"source-backup"/".."/"source"/"new directory"}) {
        assert(!RuntimeKit::CopyMissing(source,target,ec)&&ec);
        assert(!fs::exists(source/"new directory"));assert(!fs::exists(root/"payload"));
        assert(Read(source/"payload")=="unchanged kit");
    }
    const auto sibling=root/"source-backup";assert(RuntimeKit::CopyMissing(source,sibling,ec)&&!ec);
    assert(Read(sibling/"payload")=="unchanged kit");
    std::ofstream(sibling/"payload")<<"existing destination";
    assert(RuntimeKit::CopyMissing(source,sibling,ec));assert(Read(sibling/"payload")=="existing destination");
    fs::create_directory_symlink(source,root/"alias",ec);
    if(!ec) {assert(!RuntimeKit::CopyMissing(source,root/"alias"/"nested",ec)&&ec);assert(!fs::exists(source/"nested"));}
    else std::cout<<"Directory symlink creation unavailable: alias case not verified on this host.\n";
#ifndef _WIN32
    fs::create_directories(source/"inside");
    fs::create_directory_symlink(source/"inside",root/"nested-alias",ec);
    assert(!ec);assert(!RuntimeKit::CopyMissing(source,root/"nested-alias"/"output",ec)&&ec);
    assert(!fs::exists(source/"inside/output"));
#else
    auto longSource=root/"long-kit";
    for(int i=0;i<4;++i)longSource/=std::string(80,'x');
    auto nativeSource=RuntimeKit::NativePath(longSource,ec);assert(!ec);fs::create_directories(nativeSource);
    {std::ofstream file(nativeSource/"payload");file<<"long path kit";assert(file.good());}
    const auto resolved=RuntimeKit::GuardPath(longSource,ec);assert(!ec);
    const auto nativeResolved=RuntimeKit::NativePath(resolved,ec);assert(!ec);
    assert(fs::equivalent(nativeResolved,nativeSource,ec)&&!ec);
    const auto explicitResolved=RuntimeKit::GuardPath(nativeSource,ec);assert(!ec);
    assert(fs::equivalent(RuntimeKit::NativePath(explicitResolved,ec),nativeSource,ec)&&!ec);
    std::cout<<"Windows deep and explicit extended paths resolve correctly.\n";
#endif
    assert(!RuntimeKit::CopyMissing(root/"missing source",root/"must-not-create",ec)&&ec);
    assert(!fs::exists(root/"must-not-create"));
    std::cout<<"Kit overlap guards preserve source, refuse nested/enclosing copies and allow sibling copies.\n";
}
