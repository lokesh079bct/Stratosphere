#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

namespace Engine
{
    class Window;

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    class VulkanContext
    {
    public:
        VulkanContext(Window &window);
        ~VulkanContext();

        void Init();
        void Shutdown();

    private:
        void createInstance();
        void createSurface();
        void pickPhysicalDeviceForPresentation();
        QueueFamilyIndices findQueueFamiliesForPresentation(VkPhysicalDevice device) const;
        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities{};
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

    private:
        Window &m_Window;
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        QueueFamilyIndices m_QueueFamilyIndices;
    };

} // namespace Engine