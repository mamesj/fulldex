
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cassert>
#include "controls.h"
#if __has_include("config_lines.h")
#include "config_lines.h"
#endif
#include "config_functions.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace fs=std::filesystem;
static std::string Read(const fs::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static std::string Normalized(std::string s){size_t p=0;while((p=s.find("\r\n",p))!=std::string::npos)s.erase(p,1);return s;}
static void Write(const fs::path&p,const std::string&s){std::ofstream f(p,std::ios::binary);f<<s;assert(f.good());}
int main(int argc,char**argv){assert(argc==2);const fs::path root=fs::absolute(argv[1]);fs::create_directories(root);int failures=0;
auto check=[&](bool ok,const std::string&name){if(!ok){std::cerr<<"Failed settings preservation: "<<name<<'\n';++failures;}};
const std::string value="private/"+std::string(2400,'x')+u8"/日本語/"+std::string(1300,'y');const std::string payload="roms_folder="+value+"\r\nlanguage=ja\r\nmap_key_a=4\r\n# "+std::string(2800,'z')+"\r\ntrailing=value";
Controls::Bindings bindings;bindings.key[0]=SDL_SCANCODE_Z;
for(const std::string writer:{"controls","single","multiple"}){const fs::path path=root/(writer+".cfg");Write(path,payload);check(Launcher::ConfigValue(path.string(),"roms_folder")==value,writer+" complete lookup before save");if(writer=="controls")check(bindings.SaveTo(path.string()),writer+" save");else if(writer=="single")check(Launcher::SetConfigLine(path.string(),"show_fps","1"),writer+" save");else SetConfigValues(path.string(),{{"show_fps","1"}});if(writer=="controls")check(Launcher::ConfigValue(path.string(),(std::string("map_key_")+Controls::ActionKey(0)).c_str())==std::to_string(SDL_SCANCODE_Z),writer+" mapping updated");else check(Launcher::ConfigValue(path.string(),"show_fps")=="1",writer+" value updated");const auto result=Normalized(Read(path));check(result.find("roms_folder="+value+"\n")!=std::string::npos,writer+" long folder value");check(result.find("# "+std::string(2800,'z')+"\n")!=std::string::npos,writer+" long comment");check(result.find("trailing=value\n")!=std::string::npos,writer+" final line without newline");check(Launcher::ConfigValue(path.string(),"roms_folder")==value,writer+" complete lookup after save");}
for(const std::string writer:{"controls","single","multiple"}){const fs::path path=root/(writer+"-denied.cfg");Write(path,payload);
#ifdef _WIN32
HANDLE lock=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(lock!=INVALID_HANDLE_VALUE);
#else
if(geteuid()==0){std::cout<<"Root cannot exercise read-denied fixture.\n";continue;}fs::permissions(path,fs::perms::owner_write);
#endif
bool saved=false;if(writer=="controls")saved=bindings.SaveTo(path.string());else if(writer=="single")saved=Launcher::SetConfigLine(path.string(),"show_fps","1");else SetConfigValues(path.string(),{{"show_fps","1"}});
#ifdef _WIN32
CloseHandle(lock);
#else
fs::permissions(path,fs::perms::owner_read|fs::perms::owner_write);
#endif
if(writer!="multiple")check(!saved,writer+" reports read refusal");check(Read(path)==payload,writer+" preserves unreadable existing config");}
const fs::path fragment=root/"fragment.cfg";std::string tricky="roms_folder=";tricky.append(511-tricky.size(),'x');tricky+=std::string("map_key_")+Controls::ActionKey(0)+"="+std::to_string(SDL_SCANCODE_Z)+"\n";Write(fragment,tricky);Controls::Bindings loaded;loaded.Load(fragment.string());check(loaded.key[0]==-1,"long unrelated value cannot inject a control binding");
const fs::path fresh=root/"fresh.cfg";check(bindings.SaveTo(fresh.string()),"new config can be created");check(Read(fresh).find("map_key_")!=std::string::npos,"new config has mappings");loaded.Load(fresh.string());check(loaded.key[0]==SDL_SCANCODE_Z,"saved controls reload normally");
const fs::path unicode=root/fs::u8path(u8"設定.cfg");Write(unicode,payload);check(bindings.SaveTo(unicode.u8string()),"Unicode-named config controls save");check(Launcher::SetConfigLine(unicode.u8string(),"show_fps","1"),"Unicode-named config single save");SetConfigValues(unicode.u8string(),{{"volume","70"}});check(Launcher::ConfigValue(unicode.u8string(),"roms_folder")==value&&Launcher::ConfigValue(unicode.u8string(),"volume")=="70","Unicode-named config preserved through all helpers");
#if __has_include("config_lines.h")
std::vector<std::string>lines={"sentinel"};const fs::path binary=root/"binary.cfg";const std::string bytes="before=1\nbinary="+std::string(1,'\0')+"bad\n";Write(binary,bytes);check(!ConfigLines::Read(binary.string(),lines)&&lines==std::vector<std::string>{"sentinel"},"binary read leaves output untouched");check(!bindings.SaveTo(binary.string())&&Read(binary)==bytes,"binary settings not rewritten");check(!ConfigLines::Read(root.string(),lines)&&lines==std::vector<std::string>{"sentinel"},"failed directory read leaves output untouched");check(ConfigLines::Read((root/"missing.cfg").string(),lines)&&lines.empty(),"missing settings produce empty successful read");
const fs::path duplicate=root/"duplicate.cfg";Write(duplicate,"roms_folder=first\r\nroms_folder="+value);check(Launcher::ConfigValue(duplicate.string(),"roms_folder")==value,"last complete duplicate wins");
#endif
if(failures)return 1;std::cout<<"Complete settings values and Unicode files survive all helpers; read failures preserve originals; control fragments are ignored; normal controls reload.\n";return 0;}
