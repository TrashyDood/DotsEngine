#include <Camera/Camera.h>

Camera::Camera()
{
    m_Position = glm::vec3(0.0f, 0.0f, 3.0f);
    m_EulerRotation = glm::vec3(0.0f);
    m_Rotation = glm::quat(1, 0, 0, 0);

    m_Fov = 45.0f;
    m_AspectRatio = 1280.0f / 720.0f;
    m_Near = 0.1f;
    m_Far = 1000.0f;
}

void Camera::SetPosition(const glm::vec3& position)
{
    m_Position = position;
}

void Camera::SetRotation(const glm::vec3& eulerAngles)
{
    m_EulerRotation = eulerAngles;

    glm::vec3 eulerRadians = glm::radians(eulerAngles);

    glm::quat qx = glm::angleAxis(eulerRadians.x, glm::vec3(1, 0, 0));
    glm::quat qy = glm::angleAxis(eulerRadians.y, glm::vec3(0, 1, 0));
    glm::quat qz = glm::angleAxis(eulerRadians.z, glm::vec3(0, 0, 1));

    m_Rotation = qz * qy * qx;
}

void Camera::LookAt(const glm::vec3& target, const glm::vec3& up)
{
    glm::vec3 forward = glm::normalize(target - m_Position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 trueUp = glm::normalize(glm::cross(right, forward));

    glm::mat3 frame;
    frame[0] = right;
    frame[1] = trueUp;
    frame[2] = -forward;

    m_Rotation = glm::normalize(glm::quat_cast(frame));
    m_EulerRotation = glm::eulerAngles(m_Rotation); 
}

glm::mat4 Camera::GetView() const
{
    glm::mat4 rotationMat = glm::mat4_cast(glm::conjugate(m_Rotation));
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), -m_Position);
    return rotationMat * translationMat;
}

glm::mat4 Camera::GetProjection() const
{
    return glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_Near, m_Far);
}

glm::mat4 Camera::GetViewProjection() const
{
    glm::mat4 rotationMat = glm::mat4_cast(glm::conjugate(m_Rotation));
    glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), -m_Position);

    return glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_Near, m_Far) * rotationMat * translationMat;
}

void Camera::SetAspectRatio(float aspect)
{
    m_AspectRatio = aspect;
}

void Camera::SetAspectRatio(float width, float height)
{
    m_AspectRatio = width / height;
}

void Camera::SetNearFar(float nearPlane, float farPlane)
{
    m_Near = nearPlane;
    m_Far = farPlane;
}

glm::vec3 Camera::GetForward() const
{
    // OpenGL convention: forward is -Z?
    return glm::normalize(m_Rotation * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::GetRight() const
{
    return glm::normalize(m_Rotation * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Camera::GetUp() const
{
    return glm::normalize(m_Rotation * glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::DrawCameraFrustrum()
{
    const float length = 5.0f;

    glm::vec3 forward = m_Rotation * glm::vec3(0, 0, -1);
    glm::vec3 right = m_Rotation * glm::vec3(1, 0, 0);
    glm::vec3 up = m_Rotation * glm::vec3(0, 1, 0);

    float tanFov = tan(glm::radians(m_Fov * 0.5f));
    float halfHeight = tanFov * length;
    float halfWidth = halfHeight * m_AspectRatio;

    glm::vec3 center = m_Position + forward * length;

    glm::vec3 tl = center + up * halfHeight - right * halfWidth;
    glm::vec3 tr = center + up * halfHeight + right * halfWidth;
    glm::vec3 bl = center - up * halfHeight - right * halfWidth;
    glm::vec3 br = center - up * halfHeight + right * halfWidth;

    glm::vec3 color(1, 1, 1);
}
