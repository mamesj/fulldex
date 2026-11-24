#include "n3ds.h"
#include <cassert>
#include <csignal>
#include <fstream>
#include <iostream>

static void Signal(int) {}
static bool WaitStep()
{
    for (int i=0; i<500; ++i) {
        { std::lock_guard<std::mutex> lock(N3ds::P().m);
          if (N3ds::P().step == "TRANSLATING THE GAME'S CODE") return true; }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    namespace fs=std::filesystem;
    const fs::path root=fs::absolute(argv[1]);
    // The test binary lives beside its own synthetic master kit. No real ROM,
    // installed kit, user data, or external game process is used.
    const auto program=root/"3ds/program/pokemon-oras";
    fs::create_directories(program.parent_path());
    { std::ofstream out(program); out << "#!/bin/sh\nprintf 'step translate\\n'\nsleep 0.5\nprintf 'error synthetic translation failed\\n'\nexit 7\n"; }
    fs::permissions(program, fs::perms::owner_all);
    const auto rom=root/"synthetic.3ds";
    { std::ofstream out(rom, std::ios::binary); out << std::string(1024, '\0'); }
    struct sigaction action{}, previous{};
    action.sa_handler=Signal; sigemptyset(&action.sa_mask); action.sa_flags=0;
    assert(sigaction(SIGUSR1, &action, &previous)==0);
    Launcher::Game game;
    game.code="ECRA"; game.name="SYNTHETIC"; game.folder=(root/"game").string(); game.rom=rom.string();
    N3ds::StartPrepare(root.string(), game);
    assert(WaitStep());
    // Interrupt pipe reading, then waiting on the old implementation. The
    // child remains alive long enough to expose a zero-initialized wait status.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    assert(pthread_kill(N3ds::P().th.native_handle(), SIGUSR1)==0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(pthread_kill(N3ds::P().th.native_handle(), SIGUSR1)==0);
    N3ds::P().th.join();
    assert(sigaction(SIGUSR1, &previous, nullptr)==0);
    const auto result=N3ds::Peek();
    if (!result.failed || result.error!="synthetic translation failed") {
        std::cerr << "Lost preparation failure: failed=" << result.failed << " error=" << result.error << '\n';
        return 1;
    }
    assert(fs::is_regular_file(rom));
    std::cout << "Interrupted preparation retained child failure output and waited for its result.\n";
}
