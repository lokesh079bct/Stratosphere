#include "ScenarioSpawner.h"

#include "Structs/SpawnGroup.h"

#include "ECS/ECSContext.h"
#include "ECS/Prefab.h"
#include "ECS/PrefabSpawner.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <iostream>
#include <random>
#include <unordered_map>

namespace
{
    float prefabAutoSpacingMeters(const Engine::ECS::Prefab &prefab, Engine::ECS::ComponentRegistry &registry)
    {
        const uint32_t radId = registry.ensureId("Radius");
        const uint32_t sepId = registry.ensureId("Separation");

        float r = 0.0f;
        float s = 0.0f;
        if (auto it = prefab.defaults.find(radId);
            it != prefab.defaults.end() && std::holds_alternative<Engine::ECS::Radius>(it->second))
            r = std::get<Engine::ECS::Radius>(it->second).r;

        if (auto it = prefab.defaults.find(sepId);
            it != prefab.defaults.end() && std::holds_alternative<Engine::ECS::Separation>(it->second))
            s = std::get<Engine::ECS::Separation>(it->second).value;

        // For same-type units, desired center-to-center distance is:
        // (r1+r2) + (sep1+sep2) = 2r + 2sep.
        return 2.0f * (r + s);
    }

    std::unordered_map<std::string, std::pair<float, float>> parseAnchors(const nlohmann::json &j)
    {
        std::unordered_map<std::string, std::pair<float, float>> anchors;

        if (!j.contains("anchors") || !j["anchors"].is_object())
            return anchors;

        for (auto it = j["anchors"].begin(); it != j["anchors"].end(); ++it)
        {
            const std::string key = it.key();
            const auto &a = it.value();
            const float ax = a.value("x", 0.0f);
            const float az = a.value("z", 0.0f);
            anchors.emplace(key, std::make_pair(ax, az));
        }

        return anchors;
    }

    SpawnGroupResolved parseSpawnGroup(const nlohmann::json &g,
                                       const std::unordered_map<std::string, std::pair<float, float>> &anchors)
    {
        SpawnGroupResolved sg;
        sg.id = g.value("id", std::string("(no-id)"));
        sg.unitType = g.value("unitType", std::string(""));
        sg.count = g.value("count", 0);

        const std::string anchorName = g.value("anchor", std::string(""));
        const auto anchorIt = anchors.find(anchorName);
        const float anchorX = (anchorIt != anchors.end()) ? anchorIt->second.first : 0.0f;
        const float anchorZ = (anchorIt != anchors.end()) ? anchorIt->second.second : 0.0f;

        const float offX = g.contains("offset") ? g["offset"].value("x", 0.0f) : 0.0f;
        const float offZ = g.contains("offset") ? g["offset"].value("z", 0.0f) : 0.0f;
        sg.originX = anchorX + offX;
        sg.originZ = anchorZ + offZ;

        // Defaults
        sg.formationKind = "grid";
        sg.columns = 0;
        sg.circleRadiusM = 0.0f;
        sg.jitterM = 0.0f;
        sg.spacingAuto = true;
        sg.spacingM = 0.0f;

        if (g.contains("formation") && g["formation"].is_object())
        {
            const auto &f = g["formation"];
            sg.formationKind = f.value("kind", std::string("grid"));
            sg.columns = f.value("columns", 0);
            sg.circleRadiusM = f.value("radius_m", 0.0f);
            sg.jitterM = f.value("jitter_m", 0.0f);

            if (f.contains("spacing_m"))
            {
                if (f["spacing_m"].is_string() && f["spacing_m"].get<std::string>() == "auto")
                {
                    sg.spacingAuto = true;
                }
                else if (f["spacing_m"].is_number())
                {
                    sg.spacingAuto = false;
                    sg.spacingM = f["spacing_m"].get<float>();
                }
            }
        }

        return sg;
    }

    std::pair<float, float> computeFormationOffset(const SpawnGroupResolved &sg, int i, float spacingM)
    {
        const bool isCircle = (sg.formationKind == "circle");
        if (isCircle)
        {
            const float angle = (sg.count > 0)
                                    ? (static_cast<float>(i) * 6.28318530718f / static_cast<float>(sg.count))
                                    : 0.0f;
            return {std::cos(angle) * sg.circleRadiusM, std::sin(angle) * sg.circleRadiusM};
        }

        // Default: grid
        int columns = (sg.columns > 0)
                          ? sg.columns
                          : static_cast<int>(std::ceil(std::sqrt(static_cast<float>(sg.count))));
        const int rows = static_cast<int>(std::ceil(static_cast<float>(sg.count) / static_cast<float>(columns)));
        const float halfW = (static_cast<float>(columns) - 1.0f) * 0.5f;
        const float halfH = (static_cast<float>(rows) - 1.0f) * 0.5f;

        const int col = i % columns;
        const int row = i / columns;
        const float ox = (static_cast<float>(col) - halfW) * spacingM;
        const float oz = (static_cast<float>(row) - halfH) * spacingM;
        return {ox, oz};
    }
}

namespace Sample
{
    uint32_t SpawnFromScenarioFile(Engine::ECS::ECSContext &ecs, const std::string &scenarioPath, bool selectSpawned)
    {
        const std::string text = Engine::ECS::readFileText(scenarioPath);
        if (text.empty())
        {
            std::cerr << "[Scenario] Failed to read " << scenarioPath << " next to executable\n";
            return 0;
        }

        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(text);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Scenario] JSON parse error: " << e.what() << "\n";
            return 0;
        }

        const std::string scenarioName = j.value("name", std::string("(unnamed)"));
        std::cout << "[Scenario] Loading: " << scenarioName << "\n";

        if (!j.contains("spawnGroups") || !j["spawnGroups"].is_array())
        {
            std::cerr << "[Scenario] Missing spawnGroups[]\n";
            return 0;
        }

        const auto anchors = parseAnchors(j);
        const uint32_t selectedId = ecs.components.ensureId("Selected");

        uint32_t totalSpawned = 0;
        for (const auto &g : j["spawnGroups"])
        {
            const SpawnGroupResolved sg = parseSpawnGroup(g, anchors);

            if (sg.unitType.empty() || sg.count <= 0)
            {
                std::cerr << "[Scenario] Skipping group id=" << sg.id << " (missing unitType or count)\n";
                continue;
            }

            const Engine::ECS::Prefab *prefab = ecs.prefabs.get(sg.unitType);
            if (!prefab)
            {
                std::cerr << "[Scenario] Missing prefab for unitType=" << sg.unitType << " (group=" << sg.id << ")\n";
                continue;
            }

            const float spacingM = sg.spacingAuto ? prefabAutoSpacingMeters(*prefab, ecs.components) : sg.spacingM;

            std::mt19937 rng(static_cast<uint32_t>(std::hash<std::string>{}(sg.id)));
            std::uniform_real_distribution<float> jitter(-sg.jitterM, sg.jitterM);

            std::cout << "[Scenario] Spawn group id=" << sg.id
                      << " unitType=" << sg.unitType
                      << " count=" << sg.count
                      << " origin=(" << sg.originX << "," << sg.originZ << ")"
                      << " formation=" << sg.formationKind
                      << " spacingM=" << spacingM
                      << " jitterM=" << sg.jitterM << "\n";

            for (int i = 0; i < sg.count; ++i)
            {
                float x = sg.originX;
                float z = sg.originZ;

                const auto [ox, oz] = computeFormationOffset(sg, i, spacingM);
                x += ox;
                z += oz;

                x += jitter(rng);
                z += jitter(rng);

                Engine::ECS::SpawnResult res = Engine::ECS::spawnFromPrefab(*prefab, ecs.components, ecs.archetypes, ecs.stores, ecs.entities);
                Engine::ECS::ArchetypeStore *store = ecs.stores.get(res.archetypeId);
                if (!store || !store->hasPosition())
                    continue;

                auto &p = store->positions()[res.row];
                p.x = x;
                p.y = 0.0f;
                p.z = z;

                if (selectSpawned)
                {
                    store->rowMasks()[res.row].set(selectedId);
                }

                ++totalSpawned;
            }
        }

        std::cout << "[Scenario] Total units spawned: " << totalSpawned << "\n";
        return totalSpawned;
    }
}
