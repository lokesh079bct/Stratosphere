#include "MySampleApp.h"

#include "Engine/VulkanContext.h"
#include "Engine/Window.h"

#include "ECS/Prefab.h"
#include "ECS/PrefabSpawner.h"
#include "ECS/ECSContext.h"

#include "ScenarioSpawner.h"
#include "assets/AssetManager.h"

#include <filesystem>
#include <iostream>
#include <sstream>

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

MySampleApp::MySampleApp() : Engine::Application()
{
    m_assets = std::make_unique<Engine::AssetManager>(
        GetVulkanContext().GetDevice(),
        GetVulkanContext().GetPhysicalDevice(),
        GetVulkanContext().GetGraphicsQueue(),
        GetVulkanContext().GetGraphicsQueueFamilyIndex());

    // RTS camera initialization (classic defaults).
    m_rtsCam.focus = {0.0f, 0.0f, 0.0f};
    m_rtsCam.yawDeg = -45.0f;
    m_rtsCam.pitchDeg = -55.0f;
    m_rtsCam.height = 70.0f;
    m_rtsCam.minHeight = 5.0f;
    m_rtsCam.maxHeight = 250.0f;

    auto &win = GetWindow();
    const float aspect = static_cast<float>(win.GetWidth()) / static_cast<float>(win.GetHeight());
    ApplyRTSCamera(aspect);

    // Seed mouse position so the first frame doesn't produce a huge delta.
    double mx = 0.0, my = 0.0;
    win.GetCursorPosition(mx, my);
    m_lastMouse = {static_cast<float>(mx), static_cast<float>(my)};

    // Allow gameplay systems to resolve RenderModel handles to loaded assets.
    m_systems.SetAssetManager(m_assets.get());
    m_systems.SetRenderer(&GetRenderer());
    m_systems.SetCamera(&m_camera);

    setupECSFromPrefabs();

    // Systems can be initialized after prefabs are registered.
    m_systems.Initialize(GetECS().components);

    // Hook engine window events into our handler.
    SetEventCallback([this](const std::string &e)
                     { this->OnEvent(e); });
}

MySampleApp::~MySampleApp() = default;

void MySampleApp::Close()
{
    vkDeviceWaitIdle(GetVulkanContext().GetDevice());

    if (m_assets)
        m_assets->garbageCollect();

    Engine::Application::Close();
}

void MySampleApp::OnUpdate(Engine::TimeStep ts)
{
    auto &win = GetWindow();
    const float aspect = static_cast<float>(win.GetWidth()) / static_cast<float>(win.GetHeight());

    // Read mouse and compute per-frame delta.
    double mx = 0.0, my = 0.0;
    win.GetCursorPosition(mx, my);
    const glm::vec2 mouse{static_cast<float>(mx), static_cast<float>(my)};
    const glm::vec2 delta = mouse - m_lastMouse;
    m_lastMouse = mouse;

    // Pan (RMB drag) in ground plane; modifies focus only.
    if (m_isPanning)
    {
        if (m_panJustStarted)
        {
            // Prevent a jump on the initial press frame.
            m_panJustStarted = false;
        }
        else
        {
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(m_rtsCam.yawDeg)) * std::cos(glm::radians(m_rtsCam.pitchDeg));
        forward.y = std::sin(glm::radians(m_rtsCam.pitchDeg));
        forward.z = std::sin(glm::radians(m_rtsCam.yawDeg)) * std::cos(glm::radians(m_rtsCam.pitchDeg));
        forward = glm::normalize(forward);

        const glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
        const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));

        glm::vec3 forwardXZ{forward.x, 0.0f, forward.z};
        glm::vec3 rightXZ{right.x, 0.0f, right.z};

        const float forwardLen2 = glm::dot(forwardXZ, forwardXZ);
        const float rightLen2 = glm::dot(rightXZ, rightXZ);
        if (forwardLen2 > 1e-6f)
            forwardXZ *= 1.0f / std::sqrt(forwardLen2);
        if (rightLen2 > 1e-6f)
            rightXZ *= 1.0f / std::sqrt(rightLen2);

        const float panScale = m_rtsCam.basePanSpeed * m_rtsCam.height;
        // Update focus (not position). Mouse delta is in pixels.
        m_rtsCam.focus += (-rightXZ * delta.x + forwardXZ * delta.y) * panScale;
        m_rtsCam.focus.y = 0.0f;
        }
    }

    // Zoom (mouse wheel) modifies height.
    const float wheel = m_scrollDelta;
    m_scrollDelta = 0.0f;
    if (wheel != 0.0f)
    {
        m_rtsCam.height -= wheel * m_rtsCam.zoomSpeed;
        m_rtsCam.height = glm::clamp(m_rtsCam.height, m_rtsCam.minHeight, m_rtsCam.maxHeight);
    }

    // Apply RTS state to engine camera every frame.
    ApplyRTSCamera(aspect);

    m_systems.Update(GetECS(), ts.DeltaSeconds);
}

void MySampleApp::ApplyRTSCamera(float aspect)
{
    // Projection stays perspective; keep it synced with window aspect.
    m_camera.SetPerspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);

    // Direction from yaw/pitch.
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(m_rtsCam.yawDeg)) * std::cos(glm::radians(m_rtsCam.pitchDeg));
    forward.y = std::sin(glm::radians(m_rtsCam.pitchDeg));
    forward.z = std::sin(glm::radians(m_rtsCam.yawDeg)) * std::cos(glm::radians(m_rtsCam.pitchDeg));
    forward = glm::normalize(forward);

    // Stable RTS mapping: keep a fixed slant while moving over ground.
    glm::vec3 forwardXZ{forward.x, 0.0f, forward.z};
    const float forwardLen2 = glm::dot(forwardXZ, forwardXZ);
    if (forwardLen2 > 1e-6f)
        forwardXZ *= 1.0f / std::sqrt(forwardLen2);
    else
        forwardXZ = {0.0f, 0.0f, -1.0f};

    const float backDistance = m_rtsCam.height;
    const glm::vec3 camPos = m_rtsCam.focus - forwardXZ * backDistance + glm::vec3(0.0f, m_rtsCam.height, 0.0f);

    m_camera.SetPosition(camPos);
    m_camera.SetRotation(m_rtsCam.yawDeg, m_rtsCam.pitchDeg);
}

void MySampleApp::OnRender()
{
    // Rendering handled by Renderer/Engine.
}

void MySampleApp::setupECSFromPrefabs()
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

    Sample::SpawnFromScenarioFile(ecs, "Scinerio.json", /*selectSpawned=*/true);
}

void MySampleApp::OnEvent(const std::string &name)
{
    std::istringstream iss(name);
    std::string evt;
    iss >> evt;

    if (evt == "MouseButtonRightDown")
    {
        m_isPanning = true;
        m_panJustStarted = true;
        auto &win = GetWindow();
        double mx = 0.0, my = 0.0;
        win.GetCursorPosition(mx, my);
        m_lastMouse = {static_cast<float>(mx), static_cast<float>(my)};
        return;
    }

    if (evt == "MouseButtonRightUp")
    {
        m_isPanning = false;
        m_panJustStarted = false;
        return;
    }

    if (evt == "MouseScroll")
    {
        double xoff = 0.0, yoff = 0.0;
        iss >> xoff >> yoff;
        (void)xoff;
        m_scrollDelta += static_cast<float>(yoff);
        return;
    }

    if (name == "EscapePressed")
    {
        Close();
        return;
    }
}
