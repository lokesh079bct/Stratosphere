#include "Engine/RendererMinimal.h"
#include "Engine/FilesUtils.h"
#include "Engine/SwapChain.h" // adjust path to your project
#include <stdexcept>
#include <array>
#include <iostream>

RendererMinimal::RendererMinimal(VkDevice device,
                                 VkPhysicalDevice physicalDevice,
                                 VkSurfaceKHR surface,
                                 Engine::SwapChain *swapchain,
                                 const Engine::QueueFamilyIndices &queueIndices,
                                 VkExtent2D initialExtent)
    : m_Device(device),
      m_PhysicalDevice(physicalDevice),
      m_Surface(surface),
      m_Swapchain(swapchain),
      m_QueueIndices(queueIndices),
      m_InitialExtent(initialExtent)
{
}

RendererMinimal::~RendererMinimal()
{
    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); ++i)
    {
        vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
    }

    cleanup();

    if (m_CommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    }

    if (m_PipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    if (m_GraphicsPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
    if (m_RenderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
}

void RendererMinimal::init()
{
    if (m_Initialized)
        return;

    // Query swapchain info from your SwapChain class (you must provide these accessors)
    m_ImageFormat = m_Swapchain->GetImageFormat();
    m_Extent = m_Swapchain->GetExtent();

    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();

    m_Initialized = true;
}

void RendererMinimal::recreate()
{
    vkDeviceWaitIdle(m_Device);
    cleanup();

    m_ImageFormat = m_Swapchain->GetImageFormat();
    m_Extent = m_Swapchain->GetExtent();

    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandBuffers();
}

// Small render pass: 1 color attachment that will be presented
void RendererMinimal::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_ImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &colorAttachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(m_Device, &info, nullptr, &m_RenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass");
    }
}

// Minimal pipeline: expects triangle.vert.spv and triangle.frag.spv in "shaders/" relative path
void RendererMinimal::createGraphicsPipeline()
{
    auto vertCode = readFile("shaders/triangle.vert.spv");
    auto fragCode = readFile("shaders/triangle.frag.spv");

    VkShaderModule vertModule = createShaderModule(m_Device, vertCode);
    VkShaderModule fragModule = createShaderModule(m_Device, fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // Vertex input (empty — we use gl_VertexIndex in vertex shader)
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 0;
    vertexInput.vertexAttributeDescriptionCount = 0;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport & scissor — set to swapchain extent
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_Extent.width;
    viewport.height = (float)m_Extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_Extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // you can flip if triangle doesn't show
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending (single attachment)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Pipeline layout (no descriptors yet)
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_RenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline");
    }
    std::cout << "Graphics pipeline created successfully." << std::endl;

    // shader modules can be destroyed after pipeline creation
    vkDestroyShaderModule(m_Device, fragModule, nullptr);
    vkDestroyShaderModule(m_Device, vertModule, nullptr);
}

void RendererMinimal::createFramebuffers()
{
    auto imageViews = m_Swapchain->GetImageViews(); // ensure accessor exists
    m_Framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); ++i)
    {
        VkImageView attachments[] = {imageViews[i]};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_Extent.width;
        framebufferInfo.height = m_Extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer");
        }
    }
}

void RendererMinimal::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_QueueIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool");
    }
}

void RendererMinimal::createCommandBuffers()
{
    size_t count = m_Framebuffers.size();
    m_CommandBuffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(count);

    if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers");
    }

    for (uint32_t i = 0; i < (uint32_t)count; ++i)
    {
        recordCommandBuffer(m_CommandBuffers[i], i);
    }
}

void RendererMinimal::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer");
    }

    VkClearValue clearColor{};
    clearColor.color = {{0.5f, 0.5f, 0.5f, 1.0f}};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_RenderPass;
    rp.framebuffer = m_Framebuffers[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = m_Extent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

    // Draw a triangle with no vertex buffer; vertex shader uses gl_VertexIndex
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer");
    }
}

void RendererMinimal::createSyncObjects()
{
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create sync objects");
        }
    }
}

VkResult RendererMinimal::drawFrame(VkQueue graphicsQueue, VkQueue presentQueue)
{
    vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain->GetSwapchain(), UINT64_MAX,
                                            m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return result;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swapchain image");
    }

    vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = waitSemaphores;
    submit.pWaitDstStageMask = waitStages;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_CommandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submit, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR sc[] = {m_Swapchain->GetSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = sc;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        return result;
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swapchain image");
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return VK_SUCCESS;
}

void RendererMinimal::cleanup()
{
    for (auto fb : m_Framebuffers)
        vkDestroyFramebuffer(m_Device, fb, nullptr);
    m_Framebuffers.clear();

    if (!m_CommandBuffers.empty())
    {
        vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(m_CommandBuffers.size()), m_CommandBuffers.data());
        m_CommandBuffers.clear();
    }

    if (m_RenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }
    if (m_GraphicsPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
        m_GraphicsPipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
}