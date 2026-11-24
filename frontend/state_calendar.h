#pragma once
#include <ctime>
#ifdef __SWITCH__
#include "switch_platform.h"
#endif

namespace StateCalendar
{
inline bool Local(std::time_t value, std::tm& result)
{
#ifdef __SWITCH__
    if (SwitchPlatform::LocalCalendar(static_cast<int64_t>(value), result)) return true;
    // Keep state loading available if the console time service cannot be read.
#endif
    if (const std::tm* local = std::localtime(&value)) { result = *local; return true; }
    return false;
}
}
