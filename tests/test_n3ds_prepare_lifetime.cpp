#include "n3ds.h"
#include <cassert>
#include <cstdlib>

static std::atomic<bool> exiting{false}, finished{false};
static void FinishDuringExit()
{
    exiting.store(true);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (!finished.load() && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(finished.load());
}

int main()
{
    // Registered first, so this runs after the old static Prep destructor.
    assert(std::atexit(FinishDuringExit)==0);
    auto& state=N3ds::P();
    state.name=std::string(10000, 'A');
    state.th=std::thread([&state] {
        while (!exiting.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // Preparation normally keeps using its mutex/strings after detach.
        std::lock_guard<std::mutex> lock(state.m);
        state.name[0]='B';
        finished.store(true);
    });
}
