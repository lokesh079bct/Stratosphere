#include "Engine/VulkanContext.h"
#include "Engine/Window.h"
#include <GLFW/glfw3.h> // for glfwCreateWindowSurface
#include <iostream>
#include <vector>
#include <stdexcept>

namespace Engine
{

    VulkanContext::VulkanContext(Window &window) : m_Window(window)
    {
        // constructor does not init the instance automatically; explicit Init() call pattern can be used
        Init();
    }

    VulkanContext::~VulkanContext()
    {
        Shutdown();
    }

    void VulkanContext::Init()
    {
        createInstance();
        createSurface();
        pickPhysicalDeviceForPresentation();
        // pick physical device, create logical device, queues...
    }

    void VulkanContext::Shutdown()
    {
        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }
        if (m_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
        }
    }

    void VulkanContext::createInstance()
    {
        uint32_t extCount = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&extCount);
        std::vector<const char *> extensions(glfwExts, glfwExts + extCount);

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "MyEngine";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "MyEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &appInfo;
        ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();

        VkResult res = vkCreateInstance(&ci, nullptr, &m_Instance);
        if (res != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
    }

    void VulkanContext::createSurface()
    {
        void *w = m_Window.GetWindowPointer();
        if (!w)
        {
            throw std::runtime_error("VulkanContext::createSurface - window handle is null");
        }
        GLFWwindow *window = reinterpret_cast<GLFWwindow *>(w);
        VkResult result = glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface via GLFW");
        }
        std::cout << "Vulkan surface created successfully." << std::endl;
    }

    void VulkanContext::pickPhysicalDeviceForPresentation()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        // Evaluate devices and pick the first suitable one
        for (const auto &device : devices)
        {
            QueueFamilyIndices indices = findQueueFamiliesForPresentation(device);

            bool swapchainAdequate = false;
            SwapChainSupportDetails swapDetails = querySwapChainSupport(device);
            swapchainAdequate = !swapDetails.formats.empty() && !swapDetails.presentModes.empty();

            if (indices.isComplete() && swapchainAdequate)
            {
                m_PhysicalDevice = device;
                m_QueueFamilyIndices = indices;
                std::cout << "Selected physical device\n";
                return;
            }
        }

        throw std::runtime_error("Failed to find a suitable GPU (no device met requirements)");
    }

    QueueFamilyIndices VulkanContext::findQueueFamiliesForPresentation(VkPhysicalDevice device) const
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &qf : queueFamilies)
        {
            // Check for graphics capability
            if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = static_cast<uint32_t>(i);
            }

            // Check for presentation support to our surface
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), m_Surface, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = static_cast<uint32_t>(i);
            }

            if (indices.isComplete())
                break;
            ++i;
        }

        return indices;
    }

    VulkanContext::SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) const
    {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }
}