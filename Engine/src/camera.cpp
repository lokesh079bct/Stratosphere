#include "Engine/Camera.h"
#include <glm/common.hpp>

namespace Engine {

Camera::Camera() {
    UpdateVectors();
}

void Camera::SetPosition(const glm::vec3& position) {
    m_Position = position;
}

void Camera::SetRotation(float yaw, float pitch) {
    m_Yaw = yaw;
    m_Pitch = glm::clamp(pitch, -89.0f, 89.0f);
    UpdateVectors();
}

const glm::vec3& Camera::GetPosition() const {
    return m_Position;
}

void Camera::SetPerspective(float fovRadians, float aspect, float nearPlane, float farPlane) {
    m_ProjectionType = ProjectionType::Perspective;
    m_FOV = fovRadians;
    m_Aspect = aspect;
    m_Near = nearPlane;
    m_Far = farPlane;
}

void Camera::SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    m_ProjectionType = ProjectionType::Orthographic;
    m_Left = left;
    m_RightOrtho = right;
    m_Bottom = bottom;
    m_Top = top;
    m_Near = nearPlane;
    m_Far = farPlane;
}

void Camera::SetAspect(float aspect) {
    m_Aspect = aspect;
}

void Camera::SetProjectionType(ProjectionType type) {
    m_ProjectionType = type;
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(
        m_Position,
        m_Position + m_Forward,
        m_Up
    );
}

glm::mat4 Camera::GetProjectionMatrix() const {
    glm::mat4 projection{1.0f};

    if (m_ProjectionType == ProjectionType::Perspective) {
        projection = glm::perspective(m_FOV, m_Aspect, m_Near, m_Far);
    } else {
        projection = glm::ortho(
            m_Left,
            m_RightOrtho,
            m_Bottom,
            m_Top,
            m_Near,
            m_Far
        );
    }

    projection[1][1] *= -1.0f;
    return projection;
}

void Camera::UpdateVectors() {
    glm::vec3 forward;
    forward.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    forward.y = sin(glm::radians(m_Pitch));
    forward.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

    m_Forward = glm::normalize(forward);
    m_Right = glm::normalize(glm::cross(m_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_Up = glm::normalize(glm::cross(m_Right, m_Forward));
}

} // namespace Engine