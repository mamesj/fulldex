#pragma once
#include <algorithm>

// A driver can accept vsync but return every picture immediately. Detect that
// before rate measurement can mistake the application's own clock for vsync.
struct PresentClock
{
    int quick = 0;
    bool unpaced = false;

    bool Observe(double blocked)
    {
        if (!unpaced)
        {
            quick = blocked < 0.0005 ? quick + 1 : 0;
            unpaced = quick >= 12;
        }
        return unpaced;
    }

    double Wait(double spent) const
    {
        return unpaced ? std::max(0.0, 1.0 / 60.0 - spent) : 0.0;
    }
};
