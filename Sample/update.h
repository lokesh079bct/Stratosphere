#pragma once

#include "ECS/ECSContext.h"

#include "systems/CommandSystem.h"
#include "systems/SteeringSystem.h"
#include "systems/SpatialIndexSystem.h"
#include "systems/LocalAvoidanceSystem.h"
#include "systems/MovementSystem.h"
#include "systems/RenderSystem.h"

namespace Engine
{
    class AssetManager;
    class Renderer;
    class Camera;
}

namespace Sample
{
    // Owns and runs Sample gameplay systems in a consistent order.
    class SystemRunner
    {
    public:
        void Initialize(Engine::ECS::ComponentRegistry &registry);
        void Update(Engine::ECS::ECSContext &ecs, float dtSeconds);

        void SetAssetManager(Engine::AssetManager *assets);
        void SetRenderer(Engine::Renderer *renderer);
        void SetCamera(Engine::Camera *camera);
        void SetGlobalMoveTarget(float x, float y, float z);

    private:
        bool m_initialized = false;

        CommandSystem m_command;
        SteeringSystem m_steering;
        SpatialIndexSystem m_spatial{2.0f};
        LocalAvoidanceSystem m_avoidance{&m_spatial};
        MovementSystem m_movement;

        RenderSystem m_renderModel;
    };
}
