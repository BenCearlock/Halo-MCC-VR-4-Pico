#pragma once

#include <cstdint>

// Preserve a cursor's normalized client position when a window is resized.
// Both rectangles use half-open pixel bounds: [0,width) x [0,height).
inline bool MapDesktopFitCursorPoint(
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
