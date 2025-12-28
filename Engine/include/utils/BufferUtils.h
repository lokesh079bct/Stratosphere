#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace Engine
{

    struct VertexBufferHandle
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    struct IndexBufferHandle
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    // Create (if needed) and map/copy vertex data into a host-visible vertex buffer.
    // If handle.buffer == VK_NULL_HANDLE, the function creates buffer+memory.
    // If buffer exists and size differs, it will re-create.
    // Returns VK_SUCCESS on success.
    VkResult CreateOrUpdateVertexBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        const void *vertexData,
        VkDeviceSize dataSize,
        VertexBufferHandle &handle);

    // Destroy buffer and memory held by VertexBufferHandle
    void DestroyVertexBuffer(VkDevice device, VertexBufferHandle &handle);

    VkResult CreateOrUpdateIndexBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        const void *indexData,
        VkDeviceSize dataSize,
        IndexBufferHandle &handle);

    void DestroyIndexBuffer(VkDevice device, IndexBufferHandle &handle);

} // namespace Engine