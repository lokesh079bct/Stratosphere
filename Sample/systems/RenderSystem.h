#pragma once

#include "ECS/SystemFormat.h"

#include "assets/AssetManager.h"

#include "Engine/Camera.h"
#include "Engine/Renderer.h"
#include "Engine/SModelRenderPassModule.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class RenderSystem : public Engine::ECS::SystemBase
{
public:
    explicit RenderSystem(Engine::AssetManager *assets = nullptr)
        : m_assets(assets)
    {
        // We need Position to build a model matrix later.
        setRequiredNames({"RenderModel", "Position"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "RenderModelSystem"; }
    void setAssetManager(Engine::AssetManager *assets) { m_assets = assets; }

    void setRenderer(Engine::Renderer *renderer) { m_renderer = renderer; }
    void setCamera(Engine::Camera *camera) { m_camera = camera; }

    void update(Engine::ECS::ArchetypeStoreManager &mgr, float /*dt*/) override
    {
        if (!m_assets || !m_renderer || !m_camera)
            return;

        auto keyFromHandle = [](const Engine::ModelHandle &h) -> uint64_t
        {
            return (static_cast<uint64_t>(h.generation) << 32) | static_cast<uint64_t>(h.id);
        };

        std::unordered_map<uint64_t, std::vector<glm::mat4>> instancesByModel;
        std::unordered_map<uint64_t, Engine::ModelHandle> handleByKey;

        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;

            auto &store = *ptr;
            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;
            if (!store.hasRenderModel())
                continue;
            if (!store.hasPosition())
                continue;

            auto &renderModels = const_cast<std::vector<Engine::ECS::RenderModel> &>(store.renderModels());
            auto &positions = const_cast<std::vector<Engine::ECS::Position> &>(store.positions());
            const auto &masks = store.rowMasks();
            const uint32_t n = store.size();

            for (uint32_t row = 0; row < n; ++row)
            {
                if (!masks[row].matches(required(), excluded()))
                    continue;

                const Engine::ModelHandle handle = renderModels[row].handle;
                Engine::ModelAsset *asset = m_assets->getModel(handle);
                if (!asset)
                    continue;

                const auto &pos = positions[row];

                const uint64_t key = keyFromHandle(handle);
                handleByKey[key] = handle;

                auto &vec = instancesByModel[key];
                vec.emplace_back(glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z)));

                (void)asset;
            }
        }

        // Create/update passes for models that have instances this frame.
        for (auto &kv : instancesByModel)
        {
            const uint64_t key = kv.first;
            auto &worlds = kv.second;
            if (worlds.empty())
                continue;

            const Engine::ModelHandle handle = handleByKey[key];

            auto it = m_passes.find(key);
            if (it == m_passes.end())
            {
                auto pass = std::make_shared<Engine::SModelRenderPassModule>();
                pass->setAssets(m_assets);
                pass->setModel(handle);
                pass->setCamera(m_camera);
                pass->setEnabled(true);
                m_renderer->registerPass(pass);
                it = m_passes.emplace(key, std::move(pass)).first;
            }

            it->second->setCamera(m_camera);
            it->second->setEnabled(true);
            it->second->setInstances(worlds.data(), static_cast<uint32_t>(worlds.size()));
        }

        // Disable passes that have no instances this frame.
        for (auto &kv : m_passes)
        {
            if (instancesByModel.find(kv.first) == instancesByModel.end())
            {
                kv.second->setEnabled(false);
            }
        }
    }

private:
    Engine::AssetManager *m_assets = nullptr; // not owned
    Engine::Renderer *m_renderer = nullptr;   // not owned
    Engine::Camera *m_camera = nullptr;       // not owned

    std::unordered_map<uint64_t, std::shared_ptr<Engine::SModelRenderPassModule>> m_passes;
};
