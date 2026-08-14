#pragma once

class Time
{
public:
    static float deltaTime;
    static float lastFrameTime;
    static float totalTime;

    static float fps;

    static void Update(float currentTime)
    {
        deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        totalTime += deltaTime;

        if (deltaTime > 0.0f) 
        {
            fps = 1.0f / deltaTime;
        }
    }
};