#pragma once
#include <chrono>

namespace vpbr{
    class Timer{
    public:
        void start();
        void tick();
        float getDeltaTime() const{ 
            return deltaTime; 
        }
        float getTotalTime() const{
            return totalTime; 
        }
        float getFPS() const{ 
            return deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f; 
        }
    private:
        using Clock = std::chrono::high_resolution_clock;
        Clock::time_point startTime;
        Clock::time_point lastTime;
        float deltaTime = 0.0f;
        float totalTime = 0.0f;
    };
} 