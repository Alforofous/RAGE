#include "pipelines/vulkan_pipeline.hpp"
#include "utils/shader_compiler.hpp"
#include "utils/buffer_utils.hpp"
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

    // Store binding information for uniform buffer creation
    this->bindingsBySetNumber = combined.bindingsBySetNumber;
    
    this->descriptorSetLayouts = this->createDescriptorSetLayouts(combined.bindingsBySetNumber);
    this->pushConstantRanges = combined.pushConstantRanges;

    this->createShaderModules(compiledShaders, shaderKinds);
    this->createUniformBuffers();
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
    this->destroyUniformBuffers();

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

void VulkanPipeline::createUniformBuffers() {
    // Create uniform buffers for all uniform buffer bindings found in shaders
    for (const auto &setBindings : this->bindingsBySetNumber) {
        for (const auto &binding : setBindings.second) {
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                UniformBuffer uniformBuffer;
                uniformBuffer.type = binding.descriptorType;
                // Use a reasonable default size - materials will specify actual size when setting uniforms
                uniformBuffer.size = 1024; // 1KB default
                
                uniformBuffer.buffer = BufferUtils::createBuffer(
                    this->device,
                    this->physicalDevice,
                    uniformBuffer.size,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    uniformBuffer.memory
                );
                
                this->uniformBuffers[binding.binding] = uniformBuffer;
            }
        }
    }
}

void VulkanPipeline::destroyUniformBuffers() {
    for (auto &pair : this->uniformBuffers) {
        UniformBuffer &uniformBuffer = pair.second;
        if (uniformBuffer.buffer != VK_NULL_HANDLE) {
            BufferUtils::destroyBuffer(this->device, uniformBuffer.buffer, uniformBuffer.memory);
        }
    }
    this->uniformBuffers.clear();
}

void VulkanPipeline::setUniform(uint32_t binding, const void* data, size_t size) {
    auto it = this->uniformBuffers.find(binding);
    if (it == this->uniformBuffers.end()) {
        throw std::runtime_error("Uniform buffer binding " + std::to_string(binding) + " not found in pipeline");
    }
    
    UniformBuffer &uniformBuffer = it->second;
    
    // Resize buffer if needed
    if (size > uniformBuffer.size) {
        // Destroy old buffer
        BufferUtils::destroyBuffer(this->device, uniformBuffer.buffer, uniformBuffer.memory);
        
        // Create new larger buffer
        uniformBuffer.size = size;
        uniformBuffer.buffer = BufferUtils::createBuffer(
            this->device,
            this->physicalDevice,
            uniformBuffer.size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffer.memory
        );
    }
    
    // Copy data to buffer
    void *bufferData = nullptr;
    vkMapMemory(this->device, uniformBuffer.memory, 0, size, 0, &bufferData);
    memcpy(bufferData, data, size);
    vkUnmapMemory(this->device, uniformBuffer.memory);
}

VkBuffer VulkanPipeline::getUniformBuffer(uint32_t binding) const {
    auto it = this->uniformBuffers.find(binding);
    if (it == this->uniformBuffers.end()) {
        throw std::runtime_error("Uniform buffer binding " + std::to_string(binding) + " not found in pipeline");
    }
    return it->second.buffer;
}