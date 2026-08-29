#include <Camera/FlyingCamera.h>
#include <Time/Time.h>
#include <GLFW/glfw3.h>
#include <Input/Input.h>

FlyingCamera::FlyingCamera() : Camera()
{
	m_Yaw = -90.0f;
	m_Pitch = 0.0f;

	m_Forward = glm::vec3(0, 0, -1);
	m_Right = glm::vec3(1, 0, 0);
	m_Up = glm::vec3(0, 1, 0);

	LookAt(GetPosition() + m_Forward, m_Up);
}

void FlyingCamera::Update()
{
	if (!Input::MouseDown(GLFW_MOUSE_BUTTON_RIGHT)) return;

    OnMouseMove((float)Input::MouseDeltaX(), (float)Input::MouseDeltaY());

    glm::vec3 position = GetPosition();

    if (Input::KeyDown(GLFW_KEY_W)) position += m_Forward * m_MoveSpeed * StaticTime().DeltaTime();
    if (Input::KeyDown(GLFW_KEY_S)) position -= m_Forward * m_MoveSpeed * StaticTime().DeltaTime();
    if (Input::KeyDown(GLFW_KEY_A)) position -= m_Right * m_MoveSpeed * StaticTime().DeltaTime();
    if (Input::KeyDown(GLFW_KEY_D)) position += m_Right * m_MoveSpeed * StaticTime().DeltaTime();
    if (Input::KeyDown(GLFW_KEY_SPACE)) position += m_Up * m_MoveSpeed * StaticTime().DeltaTime();
    if (Input::KeyDown(GLFW_KEY_LEFT_SHIFT)) position -= m_Up * m_MoveSpeed * StaticTime().DeltaTime();

    SetPosition(position);
    LookAt(position + m_Forward, m_Up);
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

