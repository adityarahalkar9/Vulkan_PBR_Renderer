#include "core/timer.h"

namespace vpbr {

    void Timer::start() {
        startTime = Clock::now();
        lastTime = startTime;
        deltaTime = 0.0f;
        totalTime = 0.0f;
    }

    void Timer::tick() {
        const auto now = Clock::now();
        deltaTime = std::chrono::duration<float>(now - lastTime).count();
        totalTime = std::chrono::duration<float>(now - startTime).count();
        lastTime = now;
    }

} // namespace vpbr