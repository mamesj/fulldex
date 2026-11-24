#include "../gba/frontend/present_clock.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    PresentClock clock;
    for (int frame = 0; frame < 11; ++frame) assert(!clock.Observe(0.0001));
    assert(clock.Observe(0.0001));
    // A driver returning at once must not produce 500 presentations/second.
    double elapsed = 0;
    for (int frame = 0; frame < 600; ++frame)
    {
        const double work = frame % 2 ? 0.002 : 0.003;
        elapsed += work + clock.Wait(work);
    }
    assert(std::abs(elapsed - 10.0) < 0.0001);
    assert(clock.Wait(0.040) == 0); // no extra wait when work already ran late
    PresentClock blocking;
    for (int frame = 0; frame < 600; ++frame)
    {
        assert(!blocking.Observe(1.0 / 240));
        assert(blocking.Wait(0.002) == 0); // working high-refresh vsync keeps its rate
    }
    PresentClock mixed;
    for (int frame = 0; frame < 600; ++frame)
        assert(!mixed.Observe(frame % 8 ? 0.0001 : 0.004));
    std::cout << "Ignored vsync paced to 60 presentations; working vsync and slow frames preserved\n";
}
