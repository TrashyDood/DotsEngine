#include <Camera/Camera.h>

Camera::Camera()
{
    position = glm::vec3(0.0f, 0.0f, 3.0f);
    eulerRotation = glm::vec3(0.0f);
    rotation = glm::quat(1, 0, 0, 0);

    fov = 45.0f;
    aspectRatio = 1280.0f / 720.0f;
    near = 0.1f;
    far = 1000.0f;
}

void Camera::SetRotation(const glm::vec3& eulerAngles)
{
    Camera::eulerRotation = eulerAngles;

    glm::vec3 eulerRadians = glm::radians(eulerAngles);

    glm::quat qx = glm::angleAxis(eulerRadians.x, glm::vec3(1, 0, 0));
    glm::quat qy = glm::angleAxis(eulerRadians.y, glm::vec3(0, 1, 0));
    glm::quat qz = glm::angleAxis(eulerRadians.z, glm::vec3(0, 0, 1));

    rotation = qz * qy * qx;
}

void Camera::LookAt(const glm::vec3& target, const glm::vec3& up)
{
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 trueUp = glm::normalize(glm::cross(right, forward));

    glm::mat3 frame;
    frame[0] = right;
    frame[1] = trueUp;
    frame[2] = -forward;

    rotation = glm::normalize(glm::quat_cast(frame));
    eulerRotation = glm::eulerAngles(rotation); 
}

glm::mat4 Camera::GetView() const
{
    glm::mat4 rotationMat = glm::mat4_cast(glm::conjugate(rotation));
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), -position);
    return rotationMat * translationMat;
}

glm::mat4 Camera::GetViewProjection() const
{
    glm::mat4 rotationMat = glm::mat4_cast(glm::conjugate(rotation));
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), -position);

    return glm::perspective(glm::radians(fov), aspectRatio, near, far) * rotationMat * translationMat;
}

void Camera::DrawCameraFrustrum()
{
    const float length = 5.0f;

    glm::vec3 forward = rotation * glm::vec3(0, 0, -1);
    glm::vec3 right = rotation * glm::vec3(1, 0, 0);
    glm::vec3 up = rotation * glm::vec3(0, 1, 0);

    float tanFov = tan(glm::radians(fov * 0.5f));
    float halfHeight = tanFov * length;
    float halfWidth = halfHeight * aspectRatio;

    glm::vec3 center = position + forward * length;

    glm::vec3 tl = center + up * halfHeight - right * halfWidth;
    glm::vec3 tr = center + up * halfHeight + right * halfWidth;
    glm::vec3 bl = center - up * halfHeight - right * halfWidth;
    glm::vec3 br = center - up * halfHeight + right * halfWidth;

    glm::vec3 color(1, 1, 1);
}
