#include <Camera/FlyingCamera.h>
#include <Time/Time.h>
#include <GLFW/glfw3.h>
#include <Input/Input.h>
#include <iostream>

FlyingCamera::FlyingCamera(Camera* camera) : m_Camera(camera)
{
	m_Yaw = -90.0f;
	m_Pitch = 0.0f;

	m_Forward = glm::vec3(0, 0, -1);
	m_Right = glm::vec3(1, 0, 0);
	m_Up = glm::vec3(0, 1, 0);

	if (m_Camera) m_Camera->LookAt(m_Camera->GetPosition() + m_Forward, m_Up);
}

void FlyingCamera::Update()
{
    float dt = Time::deltaTime;

    if (!m_Camera) return;

    bool mouseCaptured = Input::MouseDown(GLFW_MOUSE_BUTTON_RIGHT);

    if (!mouseCaptured) 
    {
        return;
    }

    float dx = (float)Input::MouseDeltaX();
    float dy = (float)Input::MouseDeltaY();
    OnMouseMove(dx, dy);

    glm::vec3 pos = m_Camera->GetPosition();

    if (Input::KeyDown(GLFW_KEY_W)) pos += m_Forward * m_MoveSpeed * dt;
    if (Input::KeyDown(GLFW_KEY_S)) pos -= m_Forward * m_MoveSpeed * dt;
    if (Input::KeyDown(GLFW_KEY_A)) pos -= m_Right * m_MoveSpeed * dt;
    if (Input::KeyDown(GLFW_KEY_D)) pos += m_Right * m_MoveSpeed * dt;
    if (Input::KeyDown(GLFW_KEY_SPACE)) pos += m_Up * m_MoveSpeed * dt;
    if (Input::KeyDown(GLFW_KEY_LEFT_SHIFT)) pos -= m_Up * m_MoveSpeed * dt;

    m_Camera->SetPosition(pos);
    m_Camera->LookAt(pos + m_Forward, m_Up);
}

void FlyingCamera::OnMouseMove(float dx, float dy)
{
	m_Yaw += dx * m_MouseSensitivity;
	m_Pitch -= dy * m_MouseSensitivity;
	m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);

	glm::vec3 fwd;
	fwd.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
	fwd.y = sin(glm::radians(m_Pitch));
	fwd.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

	m_Forward = glm::normalize(fwd);
	m_Right = glm::normalize(glm::cross(m_Forward, glm::vec3(0, 1, 0)));
	m_Up = glm::normalize(glm::cross(m_Right, m_Forward));
}

