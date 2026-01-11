#include "Engine/Application.h"
#include "Engine/TrianglesRenderPassModule.h"
#include "Engine/MeshRenderPassModule.h"
#include "assets/AssetManager.h"
#include "assets/MeshAsset.h"
#include "assets/MeshFormats.h"
#include "Engine/VulkanContext.h"
#include "Engine/Window.h"

#include "ECS/Prefab.h"
#include "ECS/PrefabSpawner.h"
#include "ECS/ECSContext.h"

#include "utils/BufferUtils.h" // CreateOrUpdateVertexBuffer + DestroyVertexBuffer
#include <iostream>
#include <filesystem>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

class MySampleApp : public Engine::Application
{
public:
    MySampleApp() : Engine::Application()
    { // Create the AssetManager (uses Vulkan device & physical device)

        m_assets = std::make_unique<Engine::AssetManager>(
            GetVulkanContext().GetDevice(),
            GetVulkanContext().GetPhysicalDevice(),
            GetVulkanContext().GetGraphicsQueue(),
            GetVulkanContext().GetGraphicsQueueFamilyIndex());

        // Keep render setup; camera/world-to-screen mapping comes later.
        setupTriangleRenderer();
        setupMeshFromAssets();
        setupECSFromPrefabs();

        // Hook engine window events into our handler
        SetEventCallback([this](const std::string &e)
                         { this->OnEvent(e); });
    }

    ~MySampleApp() {}

    void Close() override
    {
        vkDeviceWaitIdle(GetVulkanContext().GetDevice());

        // Release mesh handle and collect unused assets
        m_assets->release(m_bugattiHandle);
        m_assets->garbageCollect();

        // Destroy triangle vertex buffer
        Engine::DestroyVertexBuffer(GetVulkanContext().GetDevice(), m_triangleVB);

        // Destroy triangle instance buffer
        Engine::DestroyVertexBuffer(GetVulkanContext().GetDevice(), m_triangleInstancesVB);

        // Release passes
        m_meshPass.reset();
        m_trianglesPass.reset();

        Engine::Application::Close();
    }

    void OnUpdate(Engine::TimeStep ts) override
    {
        (void)ts;
        // Intentionally not running ECS updates yet.
    }

    void OnRender() override
    {
        // Rendering handled by Renderer/Engine; no manual draw calls here
    }

private:
    static float pxToNdcX(double px, int w) { return static_cast<float>((px / double(w)) * 2.0 - 1.0); }
    static float pxToNdcY(double py, int h) { return static_cast<float>(((py / double(h)) * 2.0 - 1.0)); }

    void setupTriangleRenderer()
    {
        // Interleaved vertex data: vec2 position, vec3 color (matches your triangle pipeline)
        const float vertices[] = {
            // x,     y,     r, g, b
            0.0f,
            -0.01f,
            1.0f,
            1.0f,
            1.0f,
            0.01f,
            0.01f,
            1.0f,
            1.0f,
            1.0f,
            -0.01f,
            0.01f,
            1.0f,
            1.0f,
            1.0f,
        };

        VkDevice device = GetVulkanContext().GetDevice();
        VkPhysicalDevice phys = GetVulkanContext().GetPhysicalDevice();

        // Create/upload triangle vertex buffer
        VkDeviceSize dataSize = sizeof(vertices);
        VkResult r = Engine::CreateOrUpdateVertexBuffer(device, phys, vertices, dataSize, m_triangleVB);
        if (r != VK_SUCCESS)
        {
            std::cerr << "Failed to create triangle vertex buffer" << std::endl;
            return;
        }

        // Create triangles pass and bind vertex buffer
        m_trianglesPass = std::make_shared<Engine::TrianglesRenderPassModule>();
        m_triangleBinding.vertexBuffer = m_triangleVB.buffer;
        m_triangleBinding.offset = 0;
        m_triangleBinding.vertexCount = 3; // base triangle (instanced)
        m_trianglesPass->setVertexBinding(m_triangleBinding);

        // Create a placeholder instance buffer; we'll stream real ECS instances every frame.
        const float oneInstance[5] = {0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
        VkDeviceSize instSize = sizeof(oneInstance);
        r = Engine::CreateOrUpdateVertexBuffer(device, phys, oneInstance, instSize, m_triangleInstancesVB);
        if (r != VK_SUCCESS)
            std::cerr << "Failed to create triangle instance buffer" << std::endl;

        Engine::TrianglesRenderPassModule::InstanceBinding inst{};
        inst.instanceBuffer = m_triangleInstancesVB.buffer;
        inst.offset = 0;
        inst.instanceCount = 1;
        m_trianglesPass->setInstanceBinding(inst);

        // Register pass to renderer
        GetRenderer().registerPass(m_trianglesPass);

        // Initial offset (push constants)
        m_trianglesPass->setOffset(0.0f, 0.0f);
    }

    void setupECSFromPrefabs()
    {
        auto &ecs = GetECS();

        // Load all prefab definitions from JSON copied next to executable.
        // (CMake copies Sample/entities/*.json -> <build>/Sample/entities/)
        size_t prefabCount = 0;
        try
        {
            for (const auto &entry : std::filesystem::directory_iterator("entities"))
            {
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().extension() != ".json")
                    continue;
                const std::string path = entry.path().generic_string();
                const std::string jsonText = Engine::ECS::readFileText(path);
                if (jsonText.empty())
                {
                    std::cerr << "[Prefab] Failed to read: " << path << "\n";
                    continue;
                }
                Engine::ECS::Prefab p = Engine::ECS::loadPrefabFromJson(jsonText, ecs.components, ecs.archetypes, *m_assets);
                if (p.name.empty())
                {
                    std::cerr << "[Prefab] Missing name in: " << path << "\n";
                    continue;
                }
                ecs.prefabs.add(p);
                ++prefabCount;
                std::cout << "[Prefab] Loaded " << p.name << " from " << path << "\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Prefab] Failed to enumerate entities/: " << e.what() << "\n";
            return;
        }

        if (prefabCount == 0)
        {
            std::cerr << "[Prefab] No prefabs loaded from entities/*.json\n";
            return;
        }

        spawnFromScenario();
    }

    struct SpawnGroupResolved
    {
        std::string id;
        std::string unitType;
        int count = 0;
        float originX = 0.0f;
        float originZ = 0.0f;
        float jitterM = 0.0f;
        std::string formationKind;
        int columns = 0;
        float circleRadiusM = 0.0f;
        bool spacingAuto = true;
        float spacingM = 0.0f;
    };

    static float prefabAutoSpacingMeters(const Engine::ECS::Prefab &p, Engine::ECS::ComponentRegistry &registry)
    {
        const uint32_t radId = registry.ensureId("Radius");
        const uint32_t sepId = registry.ensureId("Separation");

        float r = 0.0f;
        float s = 0.0f;
        if (auto it = p.defaults.find(radId); it != p.defaults.end() && std::holds_alternative<Engine::ECS::Radius>(it->second))
            r = std::get<Engine::ECS::Radius>(it->second).r;
        if (auto it = p.defaults.find(sepId); it != p.defaults.end() && std::holds_alternative<Engine::ECS::Separation>(it->second))
            s = std::get<Engine::ECS::Separation>(it->second).value;

        // For same-type units, desired center-to-center distance is:
        // (r1+r2) + (sep1+sep2) = 2r + 2sep.
        return 2.0f * (r + s);
    }

    void spawnFromScenario()
    {
        auto &ecs = GetECS();

        const std::string text = Engine::ECS::readFileText("Scinerio.json");
        if (text.empty())
        {
            std::cerr << "[Scenario] Failed to read Scinerio.json next to executable\n";
            return;
        }

        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(text);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Scenario] JSON parse error: " << e.what() << "\n";
            return;
        }

        const std::string scenarioName = j.value("name", std::string("(unnamed)"));
        std::cout << "[Scenario] Loading: " << scenarioName << "\n";

        // Anchors
        std::unordered_map<std::string, std::pair<float, float>> anchors;
        if (j.contains("anchors") && j["anchors"].is_object())
        {
            for (auto it = j["anchors"].begin(); it != j["anchors"].end(); ++it)
            {
                const std::string key = it.key();
                const auto &a = it.value();
                const float ax = a.value("x", 0.0f);
                const float az = a.value("z", 0.0f);
                anchors.emplace(key, std::make_pair(ax, az));
            }
        }

        if (!j.contains("spawnGroups") || !j["spawnGroups"].is_array())
        {
            std::cerr << "[Scenario] Missing spawnGroups[]\n";
            return;
        }

        uint32_t totalSpawned = 0;
        for (const auto &g : j["spawnGroups"])
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

            float spacingM = sg.spacingAuto ? prefabAutoSpacingMeters(*prefab, ecs.components) : sg.spacingM;

            std::mt19937 rng(static_cast<uint32_t>(std::hash<std::string>{}(sg.id)));
            std::uniform_real_distribution<float> jitter(-sg.jitterM, sg.jitterM);

            const bool isCircle = (sg.formationKind == "circle");
            const bool isGrid = (!isCircle);
            int columns = (sg.columns > 0) ? sg.columns : static_cast<int>(std::ceil(std::sqrt(static_cast<float>(sg.count))));
            const int rows = static_cast<int>(std::ceil(static_cast<float>(sg.count) / static_cast<float>(columns)));
            const float halfW = (static_cast<float>(columns) - 1.0f) * 0.5f;
            const float halfH = (static_cast<float>(rows) - 1.0f) * 0.5f;

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

                if (isGrid)
                {
                    const int col = i % columns;
                    const int row = i / columns;
                    x += (static_cast<float>(col) - halfW) * spacingM;
                    z += (static_cast<float>(row) - halfH) * spacingM;
                }
                else
                {
                    const float angle = (sg.count > 0) ? (static_cast<float>(i) * 6.28318530718f / static_cast<float>(sg.count)) : 0.0f;
                    x += std::cos(angle) * sg.circleRadiusM;
                    z += std::sin(angle) * sg.circleRadiusM;
                }

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

                ++totalSpawned;
            }
        }

        std::cout << "[Scenario] Total units spawned: " << totalSpawned << "\n";
    }

    void setupMeshFromAssets()
    {
        // Load cooked mesh via AssetManager
        // The asset variable holds the mesh data and GPU buffers

        const char *path = "assets/ObjModels/male.smesh";
        m_bugattiHandle = m_assets->loadMesh(path);
        Engine::MeshAsset *asset = m_assets->getMesh(m_bugattiHandle);
        if (!asset)
        {
            std::cerr << "Failed to load/get mesh asset: " << path << std::endl;
            return;
        }

        // Create & register mesh pass
        m_meshPass = std::make_shared<Engine::MeshRenderPassModule>();
        Engine::MeshRenderPassModule::MeshBinding binding{};
        binding.vertexBuffer = asset->getVertexBuffer();
        binding.vertexOffset = 0;
        binding.indexBuffer = asset->getIndexBuffer();
        binding.indexOffset = 0;
        binding.indexCount = asset->getIndexCount();
        binding.indexType = asset->getIndexType();
        m_meshPass->setMesh(binding);

        GetRenderer().registerPass(m_meshPass);
    }

    void updateTriangleVisibility()
    {
        if (!m_trianglesPass)
            return;
        // When mesh is visible, hide triangle by setting vertexCount=0
        Engine::TrianglesRenderPassModule::VertexBinding binding = m_triangleBinding;
        binding.vertexCount = m_showMesh ? 0 : 3;
        m_trianglesPass->setVertexBinding(binding);
    }

    void OnEvent(const std::string &name)
    {
        // Keep the mouse event wiring; logic will be updated later.
        if (name == "MouseMove" || name.rfind("MouseMove", 0) == 0)
        {
            auto &win = GetWindow();
            win.GetCursorPosition(m_lastMouseX, m_lastMouseY);
            return;
        }

        if (name == "MouseButtonLeftDown")
        {
            auto &win = GetWindow();
            win.GetCursorPosition(m_lastMouseX, m_lastMouseY);
            std::cout << "[Input] LeftDown px=(" << m_lastMouseX << "," << m_lastMouseY << ")\n";
            return;
        }

        if (name == "MouseButtonLeftUp")
        {
            auto &win = GetWindow();
            win.GetCursorPosition(m_lastMouseX, m_lastMouseY);
            std::cout << "[Input] LeftUp px=(" << m_lastMouseX << "," << m_lastMouseY << ")\n";
            return;
        }

        if (name == "MouseButtonRightDown")
        {
            auto &win = GetWindow();
            const int w = win.GetWidth();
            const int h = win.GetHeight();

            double mx = 0.0, my = 0.0;
            win.GetCursorPosition(mx, my);

            // Screen -> NDC (temporary; camera/world projection comes later)
            const float ndcX = pxToNdcX(mx, w);
            const float ndcY = pxToNdcY(my, h);
            std::cout << "[Input] RightDown px=(" << mx << "," << my << ") ndc=(" << ndcX << "," << ndcY << ")\n";
            return;
        }
    }

private:
    // Asset management
    std::unique_ptr<Engine::AssetManager> m_assets;
    Engine::MeshHandle m_bugattiHandle{};

    // Triangle state
    Engine::VertexBufferHandle m_triangleVB{};
    Engine::VertexBufferHandle m_triangleInstancesVB{};
    std::shared_ptr<Engine::TrianglesRenderPassModule> m_trianglesPass;
    Engine::TrianglesRenderPassModule::VertexBinding m_triangleBinding{};
    bool m_showMesh = false;
    double m_timeAccum = 0.0;

    // Mouse state
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;

    // Mesh state
    std::shared_ptr<Engine::MeshRenderPassModule> m_meshPass;
};

int main()
{
    try
    {
        MySampleApp app;
        app.Run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}