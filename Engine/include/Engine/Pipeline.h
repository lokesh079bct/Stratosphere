#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace Engine
{

  // Forward-declare for convenience — your project may pass VulkanContext->device instead.
  struct VulkanContext;

  struct PipelineCreateInfo
  {
    VkDevice device = VK_NULL_HANDLE;         // required
    VkRenderPass renderPass = VK_NULL_HANDLE; // required
    uint32_t subpass = 0;                     // default 0

    // shader stages (at least vertex and fragment for typical graphics pipeline)
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    // Vertex input state (optional, wrapper will provide a default empty state)
    VkPipelineVertexInputStateCreateInfo vertexInput{}; // set sType if filled
    bool vertexInputProvided = false;

    // Input assembly (optional)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    bool inputAssemblyProvided = false;

    // Viewport / Scissor will usually be dynamic. If not provided, wrapper can use
    // a default viewport/scissor that matches the renderpass extent during creation.
    std::vector<VkDynamicState> dynamicStates; // e.g. VK_DYNAMIC_STATE_VIEWPORT, SCISSOR

    // Rasterization state (optional)
    VkPipelineRasterizationStateCreateInfo rasterization{};
    bool rasterizationProvided = false;

    // Multisample state (optional)
    VkPipelineMultisampleStateCreateInfo multisample{};
    bool multisampleProvided = false;

    // Depth/stencil state (optional)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    bool depthStencilProvided = false;

    // Color blend state (optional) - if not provided wrapper will create one that
    // matches number of color attachments = 1 (no blending).
    VkPipelineColorBlendStateCreateInfo colorBlend{};
    bool colorBlendProvided = false;

    // Pre-created pipeline layout (optional). If not provided, descriptorSetLayouts
    // and pushConstantRanges will be used to create one.
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;

    // Optional pipeline cache to accelerate creation (VK_NULL_HANDLE ok)
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
  };

  class Pipeline
  {
  public:
    Pipeline() = default;
    ~Pipeline();

    // Non-copyable
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    // Create graphics pipeline according to the provided createInfo.
    // Returns VK_SUCCESS on success and sets m_pipeline / m_layout.
    VkResult create(const PipelineCreateInfo &info);

    // Destroy pipeline and optionally destroy the layout if it was created by this wrapper.
    void destroy(VkDevice device);

    // Bind the pipeline to a command buffer (graphics bind).
    void bind(VkCommandBuffer cmd) const;

    VkPipeline getVkPipeline() const { return m_pipeline; }
    VkPipelineLayout getLayout() const { return m_layout; }

    // Helper: load SPIR-V bytes from disk and create a VkShaderModule
    // Caller is responsible for destroying the returned module with vkDestroyShaderModule.
    static VkShaderModule createShaderModuleFromFile(VkDevice device, const std::string &spvPath);

  private:
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;

    // Flag indicating whether this wrapper created the layout (so destroy() knows
    // whether to destroy it). If the user passed a pipelineLayout in PipelineCreateInfo,
    // wrapper will not destroy it.
    bool m_ownsLayout = false;
  };
}