#pragma once

#include <cmath>

class Time
{
public:
    static float main_DeltaTime, main_LastFrameTime, main_TotalTime, main_FPS;
    static float sim_DeltaTime, sim_LastFrameTime, sim_TotalTime, sim_FPS;

    static void UpdateMain(float currentTime)
    {
        main_DeltaTime = currentTime - main_LastFrameTime;
        main_LastFrameTime = currentTime;

        main_TotalTime += main_DeltaTime;

        if (main_DeltaTime > 0.0f) 
            main_FPS = (int)(1.0f / main_DeltaTime);
    }

    static void UpdateSimulation(float currentTime)
    {
        sim_DeltaTime = currentTime - sim_LastFrameTime;
        sim_LastFrameTime = currentTime;

        sim_TotalTime += sim_DeltaTime;

        if (sim_DeltaTime > 0.0f)
            sim_FPS = (int)(1.0f / sim_DeltaTime);
    }
};