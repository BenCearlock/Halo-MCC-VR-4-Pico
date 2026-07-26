#pragma once

#include <cstdint>

// OpenXR may vary predictedDisplayPeriod by a few nanoseconds without changing
// cadence. Treat only a relative change larger than five percent as a pacing
// transition. This is deliberately expressed as a ratio: no headset refresh
// rate is assumed.
inline constexpr bool IsMaterialFramePeriodTransition(
    uint64_t previousPeriodNs, uint64_t currentPeriodNs)
{
    if (!previousPeriodNs || !currentPeriodNs)
        return false;

    const uint64_t smaller =
        previousPeriodNs < currentPeriodNs
            ? previousPeriodNs
            : currentPeriodNs;
    const uint64_t larger =
        previousPeriodNs < currentPeriodNs
            ? currentPeriodNs
            : previousPeriodNs;
    return larger - smaller > smaller / 20;
}
