#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera
{
public:
    Camera();

    void SetPosition(const glm::vec3& position);

    void SetRotation(const glm::vec3& eulerAngles);

    void LookAt(const glm::vec3& target, const glm::vec3& up);

    glm::mat4 GetView() const;
    glm::mat4 GetProjection() const;
    glm::mat4 GetViewProjection() const;

    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::vec3& GetEulerRotation() const { return m_EulerRotation; }
    const float& GetAspectRatio() const { return m_AspectRatio; }
    const float& GetFov() const { return m_Fov; }

    void SetFov(float fov) { m_Fov = fov; }
    void SetAspectRatio(float aspect);
    void SetAspectRatio(float width, float height);
    void SetNearFar(float nearPlane, float farPlane);

    void DrawCameraFrustrum();

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;

private:
    glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 m_EulerRotation = glm::vec3(0.0f);  
    glm::quat m_Rotation = glm::quat(1, 0, 0, 0); 

    float m_Fov = 45.0f;
    float m_AspectRatio = 1280.0f / 720.0f;
    float m_Near = 0.1f;
    float m_Far = 100.0f;
};
