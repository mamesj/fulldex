#include "n3ds.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>

// (Not assert: a failed assert in a -mwindows program opens a message box and waits for a click, so on GitHub's
// Windows machines a failure showed only as the two-minute timeout. A failure here is written to failure.txt beside
// the program, the test prints it, and the program ends at once with 1.)
#define CHECK(c) do { if (!(c)) Fail(#c, __LINE__); } while (0)
[[noreturn]] static void Fail(const char* what, int line)
{
    wchar_t self[32768];
    std::filesystem::path out = GetModuleFileNameW(nullptr, self, 32768) ? std::filesystem::path(self).parent_path() : std::filesystem::path(".");
    std::ofstream(out / "failure.txt", std::ios::app) << "line " << line << ": " << what << "\n";
    std::exit(1);
}

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    if (argc == 1)
    {
        // A fake windowed game. More than a pipe buffer, including no final newline.
        const bool restarted = fs::exists("restarted.txt");
        fs::create_directories("data");
        std::ofstream("data/log.txt") << "renderer session " << (restarted ? "second" : "first");
        std::fprintf(stderr, "runtime stderr %s\n", restarted ? "second" : "first");
        std::fwrite("runtime stdout\n", 1, 15, stdout);
        const std::string bulk(128 * 1024, 'x');
        CHECK(std::fwrite(bulk.data(), 1, bulk.size(), stdout) == bulk.size());
        std::fprintf(stderr, "runtime tail without newline");
        if (!restarted) { std::ofstream("restarted.txt") << "yes"; return 3; }
        return 0;
    }
    CHECK(argc == 2);
    const fs::path root = fs::absolute(fs::u8path(argv[1]));
    fs::create_directories(root);
    Launcher::Game game;
    game.folder = (root / fs::u8path("title with spaces \xc3\xa9")).u8string();
    game.name = "Synthetic runtime";
    game.rom = (root / "synthetic.3ds").u8string();
    fs::create_directories(fs::u8path(game.folder));
    std::ofstream(fs::u8path(game.rom)) << "synthetic fixture, no game data";
    wchar_t self[32768];
    CHECK(GetModuleFileNameW(nullptr, self, 32768));
    fs::copy_file(self, fs::u8path(N3ds::Exe(game.folder)));
    const fs::path log = root / "runtime.log";
    CHECK(_wfreopen(log.c_str(), L"wb", stderr));
    CHECK(N3ds::Play(root.u8string(), game) == 0);
    std::fflush(stderr);
    auto text = Read(log);
    CHECK(text.find("runtime stderr first") != std::string::npos);
    CHECK(text.find("runtime stderr second") != std::string::npos);
    CHECK(text.find("runtime stdout") != std::string::npos);
    CHECK(text.find("renderer session first") != std::string::npos);
    CHECK(text.find("renderer session second") != std::string::npos);
    CHECK(text.find(std::string(128 * 1024, 'x')) != std::string::npos);
    CHECK(text.find("runtime tail without newline") != std::string::npos);
    CHECK(text.find("ended (3)") != std::string::npos);
    CHECK(text.find("ended (0)") != std::string::npos);
    CHECK(fs::exists(fs::u8path(game.folder) / "restarted.txt"));

    DWORD code = 999;
    CHECK(!WindowsRuntimeLog::Run((root / L"missing.exe").wstring(), root.wstring(), code));
    std::fflush(stderr);
    text = Read(log);
    CHECK(text.find("runtime start failed (Windows error 2)") != std::string::npos);
    CHECK(code == 999);
    std::puts("Windows runtime output, full pipes, restart, Unicode paths and launch error checks passed.");
}
