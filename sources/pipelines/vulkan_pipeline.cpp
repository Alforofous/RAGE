#include "pipelines/vulkan_pipeline.hpp"
#include "utils/shader_compiler.hpp"
#include <stdexcept>
#include <map>

VulkanPipeline::VulkanPipeline(VkDevice device, const std::vector<GLSLShader> &glslShaders) : device(device) {
    this->compileShaders(glslShaders);
    this->createShaderModules();
    this->createDescriptorLayouts();
    this->createPipelineLayout();
}

VulkanPipeline::~VulkanPipeline() {
    this->dispose();
}

void VulkanPipeline::compileShaders(const std::vector<GLSLShader> &glslShaders) {
    for (const auto &shader : glslShaders) {
        auto spirv = ShaderCompiler::compileGLSL(shader.source, shader.kind);

        if (spirv.empty()) {
            throw std::runtime_error("Failed to compile GLSL shader: " + ShaderCompiler::getLastError());
        }

        this->compiledShaders.push_back(std::move(spirv));
        this->shaderKinds.push_back(shader.kind);
    }

    // Reflect all shaders after compilation
    this->reflectShaders();
}

void VulkanPipeline::createShaderModules() {
    this->shaderModules.reserve(this->compiledShaders.size());
    for (size_t i = 0; i < this->compiledShaders.size(); ++i) {
        VkShaderModule module = this->createShaderModuleFromSPIRV(i);
        this->shaderModules.push_back(module);
    }
}

void VulkanPipeline::createDescriptorLayouts() {
    this->descriptorSetLayouts = this->createDescriptorSetLayouts(this->bindingsBySetNumber);
}

void VulkanPipeline::createPipelineLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = this->descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = this->descriptorSetLayouts.data();

    // Use actual push constants from reflection
    pipelineLayoutInfo.pushConstantRangeCount = this->pushConstantRanges.size();
    pipelineLayoutInfo.pPushConstantRanges = this->pushConstantRanges.empty() ? nullptr : this->pushConstantRanges.data();

    if (vkCreatePipelineLayout(this->device, &pipelineLayoutInfo, nullptr, &this->pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

std::vector<VkDescriptorSetLayout> VulkanPipeline::createDescriptorSetLayouts(const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > &bindingsBySetNumber) {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

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

void VulkanPipeline::bindDescriptorSet(VkCommandBuffer cmdBuffer, uint32_t setIndex, VkDescriptorSet set) {
    if (setIndex >= this->descriptorSetLayouts.size()) {
        throw std::runtime_error("Invalid descriptor set index");
    }
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipelineLayout, setIndex, 1, &set, 0, nullptr);
}

void VulkanPipeline::pushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data) {
    if (!isValidPushConstantSize(size)) {
        throw std::runtime_error("Push constant size exceeds maximum");
    }
    vkCmdPushConstants(cmdBuffer, this->pipelineLayout, stageFlags, offset, size, data);
}

void VulkanPipeline::destroyShaderModule(VkShaderModule module) {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(this->device, module, nullptr);
    }
}

VkShaderModule VulkanPipeline::createShaderModuleFromSPIRV(size_t shaderIndex) {
    if (shaderIndex >= this->compiledShaders.size()) {
        throw std::runtime_error("Invalid shader index: " + std::to_string(shaderIndex));
    }

    const auto &spirvCode = this->compiledShaders[shaderIndex];

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(this->device, &createInfo, nullptr, &shaderModule);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module from SPIR-V");
    }

    return shaderModule;
}

VkPushConstantRange VulkanPipeline::getPushConstantRange() const {
    if (!this->pushConstantRanges.empty()) {
        return this->pushConstantRanges[0];  // Return first range if available
    }

    // Fallback to default range if no push constants found
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_ALL;
    range.offset = 0;
    range.size = MAX_PUSH_CONSTANT_SIZE;

    return range;
}

VkDescriptorSetLayout VulkanPipeline::getDescriptorSetLayout(uint32_t setIndex) const {
    if (setIndex >= this->descriptorSetLayouts.size()) {
        throw std::runtime_error("Invalid descriptor set layout index");
    }

    return this->descriptorSetLayouts[setIndex];
}

bool VulkanPipeline::isValidPushConstantSize(uint32_t size) {
    return size <= MAX_PUSH_CONSTANT_SIZE;
}

void VulkanPipeline::reflectShaders() {
    std::vector<ShaderReflector::ReflectionResult> results;

    // Reflect each compiled shader
    for (size_t i = 0; i < this->compiledShaders.size(); ++i) {
        const auto &spirv = this->compiledShaders[i];
        ShaderKind shaderKind = this->shaderKinds[i];

        auto result = ShaderReflector::reflectShaderBytecode(
            spirv.data(),
            spirv.size() * sizeof(uint32_t),
            shaderKind
        );

        results.push_back(result);
    }

    // Combine all reflection results
    auto combined = ShaderReflector::combineReflectionResults(results);
    this->bindingsBySetNumber = combined.bindingsBySetNumber;
    this->pushConstantRanges = combined.pushConstantRanges;
}

void VulkanPipeline::dispose() {
    if (this->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(this->device, this->pipeline, nullptr);
        this->pipeline = VK_NULL_HANDLE;
    }

    // Clean up shader modules
    for (VkShaderModule module : this->shaderModules) {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(this->device, module, nullptr);
        }
    }
    this->shaderModules.clear();

    for (VkDescriptorSetLayout layout : this->descriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(this->device, layout, nullptr);
    }
    this->descriptorSetLayouts.clear();

    if (this->pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
        this->pipelineLayout = VK_NULL_HANDLE;
    }
}

VkShaderModule VulkanPipeline::getShaderModule(size_t shaderIndex) const {
    if (shaderIndex >= this->shaderModules.size()) {
        throw std::runtime_error("Invalid shader module index: " + std::to_string(shaderIndex));
    }

    return this->shaderModules[shaderIndex];
}

std::vector<VkPipelineShaderStageCreateInfo> VulkanPipeline::getShaderStages() const {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.reserve(this->shaderModules.size());

    for (size_t i = 0; i < this->shaderModules.size(); ++i) {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = ShaderReflector::shaderKindToVkShaderStageFlagBits(this->shaderKinds[i]);
        stageInfo.module = this->shaderModules[i];
        stageInfo.pName = "main";
        shaderStages.push_back(stageInfo);
    }

    return shaderStages;
}