#include "Pipeline.h"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace Engine
{
    Pipeline::~Pipeline()
    {
        // Explicit destroy must be called by owner with the VkDevice.
        // Leaving destructor empty avoids needing a device pointer here.
    }

    VkShaderModule Pipeline::createShaderModuleFromFile(VkDevice device, const std::string &spvPath)
    {
        std::ifstream file(spvPath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open SPV file: " + spvPath);
        }
        size_t size = (size_t)file.tellg();
        file.seekg(0);
        std::vector<char> buffer(size);
        file.read(buffer.data(), size);
        file.close();

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module from: " + spvPath);
        }
        return module;
    }

    VkResult Pipeline::create(const PipelineCreateInfo &info)
    {
        if (info.device == VK_NULL_HANDLE || info.renderPass == VK_NULL_HANDLE || info.shaderStages.empty())
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkDevice device = info.device;

        // Pipeline layout: use provided or make a new one from descriptor set layouts & push constants
        VkPipelineLayout layout = info.pipelineLayout;
        m_ownsLayout = VK_NULL_HANDLE == layout;
        if (m_ownsLayout)
        {
            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = static_cast<uint32_t>(info.descriptorSetLayouts.size());
            layoutInfo.pSetLayouts = info.descriptorSetLayouts.empty() ? nullptr : info.descriptorSetLayouts.data();
            layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(info.pushConstantRanges.size());
            layoutInfo.pPushConstantRanges = info.pushConstantRanges.empty() ? nullptr : info.pushConstantRanges.data();

            VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout);
            if (r != VK_SUCCESS)
                return r;
        }

        // Vertex input state
        VkPipelineVertexInputStateCreateInfo vertexInput = info.vertexInput;
        if (!info.vertexInputProvided)
        {
            std::memset(&vertexInput, 0, sizeof(vertexInput));
            vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInput.vertexBindingDescriptionCount = 0;
            vertexInput.vertexAttributeDescriptionCount = 0;
            vertexInput.pVertexBindingDescriptions = nullptr;
            vertexInput.pVertexAttributeDescriptions = nullptr;
        }

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = info.inputAssembly;
        if (!info.inputAssemblyProvided)
        {
            std::memset(&inputAssembly, 0, sizeof(inputAssembly));
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;
        }

        // Viewport / scissor: we'll make them dynamic by default if none provided via dynamicStates
        std::vector<VkDynamicState> dynamicStates = info.dynamicStates;
        if (dynamicStates.empty())
        {
            // default to make viewport and scissor dynamic (recommended)
            dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
            dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
        }
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Rasterization
        VkPipelineRasterizationStateCreateInfo rasterization = info.rasterization;
        if (!info.rasterizationProvided)
        {
            std::memset(&rasterization, 0, sizeof(rasterization));
            rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization.depthClampEnable = VK_FALSE;
            rasterization.rasterizerDiscardEnable = VK_FALSE;
            rasterization.polygonMode = VK_POLYGON_MODE_FILL;
            rasterization.lineWidth = 1.0f;
            rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
            rasterization.depthBiasEnable = VK_FALSE;
        }

        // Multisample
        VkPipelineMultisampleStateCreateInfo multisample = info.multisample;
        if (!info.multisampleProvided)
        {
            std::memset(&multisample, 0, sizeof(multisample));
            multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisample.sampleShadingEnable = VK_FALSE;
        }

        // Depth/stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil = info.depthStencil;
        if (!info.depthStencilProvided)
        {
            std::memset(&depthStencil, 0, sizeof(depthStencil));
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
        }

        // Color blend
        VkPipelineColorBlendStateCreateInfo colorBlend = info.colorBlend;
        VkPipelineColorBlendAttachmentState colorAttachment{};
        if (!info.colorBlendProvided)
        {
            std::memset(&colorAttachment, 0, sizeof(colorAttachment));
            colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorAttachment.blendEnable = VK_FALSE;

            std::memset(&colorBlend, 0, sizeof(colorBlend));
            colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlend.logicOpEnable = VK_FALSE;
            colorBlend.attachmentCount = 1;
            colorBlend.pAttachments = &colorAttachment;
        }
        else
        {
            // Assume user filled colorBlend and its attachments properly.
        }

        // pipeline stages: use provided shaderStages (must be valid)
        const auto &stages = info.shaderStages;

        // GRAPHICS pipeline create info
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;

        // ViewportState: when viewport & scissor dynamic, count must still be set
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        pipelineInfo.pViewportState = &viewportState;

        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicState;

        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = info.renderPass;
        pipelineInfo.subpass = info.subpass;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        VkResult res = vkCreateGraphicsPipelines(device, info.pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);
        if (res != VK_SUCCESS)
        {
            if (m_ownsLayout && layout != VK_NULL_HANDLE)
            {
                vkDestroyPipelineLayout(device, layout, nullptr);
                layout = VK_NULL_HANDLE;
                m_ownsLayout = false;
            }
            return res;
        }

        m_layout = layout;
        return VK_SUCCESS;
    }

    void Pipeline::destroy(VkDevice device)
    {
        if (m_pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }
        if (m_ownsLayout && m_layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, m_layout, nullptr);
            m_layout = VK_NULL_HANDLE;
            m_ownsLayout = false;
        }
    }

    void Pipeline::bind(VkCommandBuffer cmd) const
    {
        if (m_pipeline != VK_NULL_HANDLE)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        }
    }
}