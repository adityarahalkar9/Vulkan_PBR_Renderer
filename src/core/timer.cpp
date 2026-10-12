#include "core/timer.h"

namespace vpbr {

    void Timer::start() {
        startTime = Clock::now();
        lastTime = startTime;
        deltaTime = 0.0f;
        totalTime = 0.0f;
    }

    void Timer::tick() {
        const auto currentTime = Clock::now();
        deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        totalTime = std::chrono::duration<float>(currentTime - startTime).count();
        lastTime = currentTime;
    }

} // namespace vpbr
