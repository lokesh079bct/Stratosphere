#pragma once

#include <cstdint>
#include <string>

namespace Engine::ECS
{
    struct ECSContext;
}

namespace Sample
{
    // Spawns entities described in the scenario JSON.
    // Returns total number of spawned entities.
    uint32_t SpawnFromScenarioFile(Engine::ECS::ECSContext &ecs, const std::string &scenarioPath, bool selectSpawned = true);
}
