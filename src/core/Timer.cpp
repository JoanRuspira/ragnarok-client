//===========================================================================
// Timer.cpp  –  Frame-timing implementation
// Clean C++17 rewrite.
//===========================================================================
#include "Timer.h"
#include "Types.h"
#include <chrono>
#include <thread>

// ---------------------------------------------------------------------------
// Module-local state
// ---------------------------------------------------------------------------
using FrameClock = std::chrono::steady_clock;

static FrameClock::time_point s_frameTime{};
static FrameClock::time_point s_nextFrameTime{};
static FrameClock::duration s_frameInterval = std::chrono::ceil<FrameClock::duration>(
    std::chrono::duration<double>(1.0 / 60.0));

// ---------------------------------------------------------------------------

void InitTimer(unsigned int framesPerSecond)
{
    const unsigned int targetFps = framesPerSecond > 0 ? framesPerSecond : 60u;
    s_frameInterval = std::chrono::ceil<FrameClock::duration>(
        std::chrono::duration<double>(1.0 / targetFps));
    ResetTimer();
}

void ResetTimer()
{
    s_frameTime = FrameClock::now();
    s_nextFrameTime = s_frameTime;
}

void WaitForNextFrame()
{
    auto now = FrameClock::now();
    while (now < s_nextFrameTime) {
        const auto remaining = s_nextFrameTime - now;
        // Sleep for most of the wait, then yield to avoid rounding each
        // 60 FPS interval up to a whole 17 ms.
        if (remaining > std::chrono::milliseconds(1)) {
            std::this_thread::sleep_for(remaining - std::chrono::milliseconds(1));
        } else {
            std::this_thread::yield();
        }
        now = FrameClock::now();
    }

    // Keep an absolute cadence: small scheduler delays must not accumulate
    // into a lower frame rate. Rebase after a substantial delay so the next
    // frame does not rush to catch up with an almost-expired deadline.
    if (now - s_nextFrameTime >= s_frameInterval / 4) {
        s_nextFrameTime = now + s_frameInterval;
    } else {
        s_nextFrameTime += s_frameInterval;
    }
}

unsigned long GetSkipFrameCount()
{
    const auto now = FrameClock::now();
    const auto frames = (now - s_frameTime) / s_frameInterval;
    s_frameTime += frames * s_frameInterval;

    if (!g_frameskip)
        return 1;
    return static_cast<unsigned long>(frames);
}

int SkipSceneRendering()
{
    // Original implementation always returned 1 (do not skip rendering).
    return 1;
}
