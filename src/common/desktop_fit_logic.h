#pragma once

#include <cstdint>

inline bool MapDesktopFitPoint(
    int x, int y,
    int sourceWidth, int sourceHeight,
    int destinationWidth, int destinationHeight,
    int& mappedX, int& mappedY) noexcept
{
    if (sourceWidth <= 0 || sourceHeight <= 0 ||
        destinationWidth <= 0 || destinationHeight <= 0 ||
        x < 0 || x >= sourceWidth || y < 0 || y >= sourceHeight)
    {
        return false;
    }

    mappedX = static_cast<int>(
        static_cast<int64_t>(x) * destinationWidth / sourceWidth);
    mappedY = static_cast<int>(
        static_cast<int64_t>(y) * destinationHeight / sourceHeight);
    return true;
}
