#pragma once
#include "Engine/QueueFamilyStruct.h"
#include <vulkan/vulkan.h>
#include <vector>

// Forward declaration for your SwapChain and QueueFamilyIndices
namespace Engine
{
    class SwapChain;
    struct QueueFamilyIndices;
}

class RendererMinimal
{
public:
    RendererMinimal(VkDevice device,
                    VkPhysicalDevice physicalDevice,
                    VkSurfaceKHR surface,
                    Engine::SwapChain *swapchain,
                    const Engine::QueueFamilyIndices &queueIndices,
                    VkExtent2D initialExtent);

    ~RendererMinimal();

    // Initialize everything that depends on the existing swapchain
    void init();

    // Recreate renderer resources after swapchain recreation (window resize)
    void recreate();

    // Draw a frame. Returns VkResult from present/acquire so caller can handle VK_ERROR_OUT_OF_DATE_KHR
    VkResult drawFrame(VkQueue graphicsQueue, VkQueue presentQueue);

private:
    // helpers
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void createSyncObjects();
    void cleanup();

    VkDevice m_Device;
    VkPhysicalDevice m_PhysicalDevice;
    VkSurfaceKHR m_Surface;
    Engine::SwapChain *m_Swapchain;
    Engine::QueueFamilyIndices m_QueueIndices;

    // Swapchain dependent
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> m_Framebuffers;
    std::vector<VkCommandBuffer> m_CommandBuffers;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;

    VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_Extent{};
    VkExtent2D m_InitialExtent{};

    // Sync: simple double-buffering
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    size_t m_CurrentFrame = 0;

    bool m_Initialized = false;
};