#pragma once

#include <cmath>

class Time
{
public:
    static float main_DeltaTime, main_LastFrameTime, main_TotalTime, main_FPS;

    static void UpdateMain(float currentTime)
    {
        main_DeltaTime = currentTime - main_LastFrameTime;
        main_LastFrameTime = currentTime;

        main_TotalTime += main_DeltaTime;

        if (main_DeltaTime > 0.0f) 
            main_FPS = (int)(1.0f / main_DeltaTime);
    }
};