#include "pipelines/vulkan_pipeline.hpp"
#include "utils/shader_compiler.hpp"
#include <stdexcept>
#include <map>
#include <algorithm>
#include "pipelines/shader_reflector.hpp"

VulkanPipeline::VulkanPipeline(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<GLSLShader> &glslShaders)
    : device(device), physicalDevice(physicalDevice) {
    this->compileShaders(glslShaders);
    this->createPipelineLayout();
}

VulkanPipeline::~VulkanPipeline() {
    this->dispose();
}

void VulkanPipeline::compileShaders(const std::vector<GLSLShader> &glslShaders) {
    std::vector<std::vector<uint32_t> > compiledShaders;
    std::vector<ShaderKind> shaderKinds;

    for (const auto &shader : glslShaders) {
        auto spirv = ShaderCompiler::compileGLSL(shader.source, shader.kind);

        if (spirv.empty()) {
            throw std::runtime_error("Failed to compile GLSL shader: " + ShaderCompiler::getLastError());
        }

        compiledShaders.push_back(std::move(spirv));
        shaderKinds.push_back(shader.kind);
    }

    std::vector<ShaderReflector::ReflectionResult> results;
    for (size_t i = 0; i < compiledShaders.size(); ++i) {
        const auto &spirv = compiledShaders[i];
        ShaderKind shaderKind = shaderKinds[i];

        auto result = ShaderReflector::reflectShaderBytecode(
            spirv.data(),
            spirv.size() * sizeof(uint32_t),
            shaderKind
        );

        results.push_back(result);
    }

    auto combined = ShaderReflector::combineReflectionResults(results);

    this->descriptorSetLayouts = this->createDescriptorSetLayouts(combined.bindingsBySetNumber);
    this->pushConstantRanges = combined.pushConstantRanges;

    this->createShaderModules(compiledShaders, shaderKinds);
}

void VulkanPipeline::createShaderModules(const std::vector<std::vector<uint32_t> > &compiledShaders, const std::vector<ShaderKind> &shaderKinds) {
    this->shaders.reserve(compiledShaders.size());

    for (size_t i = 0; i < compiledShaders.size(); ++i) {
        const auto &spirvCode = compiledShaders[i];

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
        createInfo.pCode = spirvCode.data();

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VkResult result = vkCreateShaderModule(this->device, &createInfo, nullptr, &shaderModule);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module from SPIR-V");
        }

        ShaderInfo shaderInfo;
        shaderInfo.module = shaderModule;
        shaderInfo.stage = ShaderReflector::shaderKindToVkShaderStageFlagBits(shaderKinds[i]);

        this->shaders.push_back(shaderInfo);
    }
}

void VulkanPipeline::createPipelineLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = this->descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = this->descriptorSetLayouts.data();

    pipelineLayoutInfo.pushConstantRangeCount = this->pushConstantRanges.size();
    pipelineLayoutInfo.pPushConstantRanges = this->pushConstantRanges.empty() ? nullptr : this->pushConstantRanges.data();

    if (vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &this->pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

std::vector<VkDescriptorSetLayout> VulkanPipeline::createDescriptorSetLayouts(const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > &bindingsBySetNumber) {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    if (this->device == VK_NULL_HANDLE) {
        throw std::runtime_error("Cannot create descriptor set layouts with null device");
    }

    for (const auto &setBindings : bindingsBySetNumber) {
        uint32_t setNumber = setBindings.first;
        const auto &bindings = setBindings.second;

        if (!bindings.empty()) {
            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = bindings.size();
            layoutInfo.pBindings = bindings.data();

            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            if (vkCreateDescriptorSetLayout(this->device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create descriptor set layout for set " + std::to_string(setNumber));
            }
            descriptorSetLayouts.push_back(layout);
        }
    }

    return descriptorSetLayouts;
}

void VulkanPipeline::pushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data) {
    if (!isValidPushConstantData(stageFlags, offset, size)) {
        throw std::runtime_error("Push constant data doesn't fit within shader-defined ranges");
    }
    vkCmdPushConstants(cmdBuffer, this->pipelineLayout, stageFlags, offset, size, data);
}

VkDescriptorSetLayout VulkanPipeline::getDescriptorSetLayout(uint32_t setIndex) const {
    if (setIndex >= this->descriptorSetLayouts.size()) {
        throw std::runtime_error("Invalid descriptor set layout index");
    }

    return this->descriptorSetLayouts[setIndex];
}

bool VulkanPipeline::isValidPushConstantData(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) const {
    return std::any_of(this->pushConstantRanges.begin(), this->pushConstantRanges.end(),
                       [stageFlags, offset, size](const auto &range) {
        return (range.stageFlags & stageFlags) != 0 &&
               offset >= range.offset &&
               (offset + size) <= (range.offset + range.size);
    });
}

void VulkanPipeline::dispose() {
    if (this->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(this->device, this->pipeline, nullptr);
        this->pipeline = VK_NULL_HANDLE;
    }

    for (const auto &shader : this->shaders) {
        if (shader.module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(this->device, shader.module, nullptr);
        }
    }
    this->shaders.clear();

    for (VkDescriptorSetLayout layout : this->descriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(this->device, layout, nullptr);
    }
    this->descriptorSetLayouts.clear();

    if (this->pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
        this->pipelineLayout = VK_NULL_HANDLE;
    }
}

std::vector<VkPipelineShaderStageCreateInfo> VulkanPipeline::getShaderStages() const {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.reserve(this->shaders.size());

    for (const auto &shader : this->shaders) {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = shader.stage;
        stageInfo.module = shader.module;
        stageInfo.pName = "main";
        shaderStages.push_back(stageInfo);
    }

    return shaderStages;
}

void VulkanPipeline::destroyShaderModule(VkShaderModule module) {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(this->device, module, nullptr);
    }
}

VkBuffer VulkanPipeline::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory) {
    VkBuffer buffer = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

    uint32_t memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(this->device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    if (vkBindBufferMemory(this->device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(this->device, bufferMemory, nullptr);
        vkDestroyBuffer(this->device, buffer, nullptr);
        throw std::runtime_error("Failed to bind buffer memory");
    }

    return buffer;
}

VkBuffer VulkanPipeline::createDeviceAddressBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory) {
    VkBuffer buffer = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

    uint32_t memoryTypeIndex = this->findMemoryType(memRequirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    allocInfo.pNext = &flagsInfo;

    if (vkAllocateMemory(this->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(this->device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    if (vkBindBufferMemory(this->device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(this->device, bufferMemory, nullptr);
        vkDestroyBuffer(this->device, buffer, nullptr);
        throw std::runtime_error("Failed to bind buffer memory");
    }

    return buffer;
}

void VulkanPipeline::destroyBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(this->device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(this->device, memory, nullptr);
    }
}

void *VulkanPipeline::mapMemory(VkDeviceMemory memory, VkDeviceSize size) {
    void *mappedMemory = nullptr;
    if (vkMapMemory(this->device, memory, 0, size, 0, &mappedMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map buffer memory");
    }

    return mappedMemory;
}

void VulkanPipeline::unmapMemory(VkDeviceMemory memory) {
    vkUnmapMemory(this->device, memory);
}

VkDeviceAddress VulkanPipeline::getBufferDeviceAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    return vkGetBufferDeviceAddress(this->device, &addressInfo);
}

void VulkanPipeline::copyToBuffer(VkDeviceMemory memory, const void *data, uint32_t dataSize, uint32_t bufferSize) {
    void *mappedMemory = this->mapMemory(memory, bufferSize);

    memcpy(mappedMemory, data, dataSize);

    this->unmapMemory(memory);
}

uint32_t VulkanPipeline::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(this->physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) != 0 && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}