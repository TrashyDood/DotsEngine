#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera
{
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 eulerRotation = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1, 0, 0, 0);

    float fov = 45.0f;
    float aspectRatio = 1280.0f / 720.0f;
    float near = 0.1f;
    float far = 100.0f;

public:
    Camera();

    glm::mat4 GetView() const;
    glm::mat4 GetProjection() const { return glm::perspective(glm::radians(fov), aspectRatio, near, far); }
    glm::mat4 GetViewProjection() const;

    const glm::vec3& GetPosition() const { return position; }
    const glm::vec3& GetEulerRotation() const { return eulerRotation; }
    const float& GetAspectRatio() const { return aspectRatio; }
    const float& GetFov() const { return fov; }

    void SetPosition(const glm::vec3& position) { Camera::position = position; }
    void SetRotation(const glm::vec3& eulerAngles);
    void LookAt(const glm::vec3& target, const glm::vec3& up);

    void SetFov(float fov) { Camera::fov = fov; }
    void SetAspectRatio(float aspect) { aspectRatio = aspect; }
    void SetAspectRatio(float width, float height) { SetAspectRatio(width / height); }
    void SetNearFar(float nearPlane, float farPlane) { near = nearPlane; far = farPlane; }

    void DrawCameraFrustrum();

    glm::vec3 GetForward() const { return glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f)); }
    glm::vec3 GetRight() const { return glm::normalize(rotation * glm::vec3(1.0f, 0.0f, 0.0f)); }
    glm::vec3 GetUp() const { return glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f)); }
};
