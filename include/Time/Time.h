#pragma once

#include <cmath>

class Time
{
    float deltaTime = 0.0f,
        lastFrameTime = 0.0f,
        totalTime = 0.0f,
        fps = 0.0f,
        minFrameTime = 0.0f;

public:
    const float& DeltaTime() const { return deltaTime; }
    const float& FPS() const { return fps; }
    const float& TotalTime() const { return totalTime; }

    void Tick(float currentTime)
    {
        deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
        totalTime += deltaTime;

        if (deltaTime > 0.0f)
            fps = (int)(1.0f / deltaTime);
    }
};

inline Time& StaticTime()
{
    static Time time;
    return time;
}