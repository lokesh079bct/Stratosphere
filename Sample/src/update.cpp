#include "update.h"

namespace Sample
{
    void SystemRunner::Initialize(Engine::ECS::ComponentRegistry &registry)
    {
        if (m_initialized)
            return;

        // Ensure common IDs exist up-front (also used by scenario spawner selection).
        (void)registry.ensureId("Selected");

        m_command.buildMasks(registry);
        m_steering.buildMasks(registry);
        m_spatial.buildMasks(registry);
        m_avoidance.buildMasks(registry);
        m_movement.buildMasks(registry);
        m_renderModel.buildMasks(registry);

        // Neighbor radius (meters). Tune later; matches SpatialIndexSystem doc.
        m_spatial.setCellSize(m_spatial.getCellSize());

        m_initialized = true;
    }

    void SystemRunner::Update(Engine::ECS::ECSContext &ecs, float dtSeconds)
    {
        if (!m_initialized)
            Initialize(ecs.components);

        if (dtSeconds <= 0.0f)
            return;

        // Suggested order per LocalAvoidanceSystem.h
        m_command.update(ecs.stores, dtSeconds);
        m_steering.update(ecs.stores, dtSeconds);
        m_spatial.update(ecs.stores, dtSeconds);
        m_avoidance.update(ecs.stores, dtSeconds);
        m_movement.update(ecs.stores, dtSeconds);
        m_renderModel.update(ecs.stores, dtSeconds);
    }

    void SystemRunner::SetAssetManager(Engine::AssetManager *assets)
    {
        m_renderModel.setAssetManager(assets);
    }

    void SystemRunner::SetRenderer(Engine::Renderer *renderer)
    {
        m_renderModel.setRenderer(renderer);
    }

    void SystemRunner::SetCamera(Engine::Camera *camera)
    {
        m_renderModel.setCamera(camera);
    }

    void SystemRunner::SetGlobalMoveTarget(float x, float y, float z)
    {
        m_command.SetGlobalMoveTarget(x, y, z);
    }
}
