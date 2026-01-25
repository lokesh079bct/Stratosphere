#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    class Camera {
    public:
        Camera();

        void SetPosition(const glm::vec3& position);
        void SetRotation(float yaw, float pitch);

        const glm::vec3& GetPosition() const;
        float GetYaw() const { return m_Yaw; }
        float GetPitch() const { return m_Pitch; }
        float GetFOV() const { return m_FOV; }

        void SetPerspective(float fovRadians, float aspect, float nearPlane, float farPlane);
        void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);
        void SetProjectionType(ProjectionType type);
        void SetAspect(float aspect);

        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix() const;

    private:
        void UpdateVectors();

    private:
        glm::vec3 m_Position{0.0f, 0.0f, 3.0f};
        float m_Yaw{-90.0f};
        float m_Pitch{0.0f};

        glm::vec3 m_Forward{0.0f, 0.0f, -1.0f};
        glm::vec3 m_Right{1.0f, 0.0f, 0.0f};
        glm::vec3 m_Up{0.0f, 1.0f, 0.0f};

        ProjectionType m_ProjectionType{ProjectionType::Perspective};

        float m_FOV{glm::radians(60.0f)};
        float m_Aspect{16.0f / 9.0f};
        float m_Near{0.1f};
        float m_Far{100.0f};

        float m_Left{-1.0f};
        float m_RightOrtho{1.0f};
        float m_Bottom{-1.0f};
        float m_Top{1.0f};
    };
} // namespace Engine