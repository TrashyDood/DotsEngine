#pragma once
#include "Camera.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class FlyingCamera
{
public:
    FlyingCamera(Camera* camera);
    void Update();

private:
    void OnMouseMove(float dx, float dy);

    Camera* m_Camera = nullptr;

    float m_Yaw;
    float m_Pitch;

    glm::vec3 m_Forward;
    glm::vec3 m_Right;
    glm::vec3 m_Up;

    float m_MoveSpeed = 30.0f;
    float m_MouseSensitivity = 0.5f;
};
