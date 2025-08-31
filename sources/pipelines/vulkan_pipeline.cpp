#include "pipelines/vulkan_pipeline.hpp"
#include "utils/shader_compiler.hpp"
#include "utils/file_reader.hpp"
#include <spirv_reflect.h>
#include <stdexcept>
#include <map>

VulkanPipeline::VulkanPipeline(VkDevice device, const std::vector<GLSLShader> &glslShaders) : device(device) {
    for (const auto &shader : glslShaders) {
        auto spirv = ShaderCompiler::compileGLSL(shader.source, shader.kind);

        if (spirv.empty()) {
            throw std::runtime_error("Failed to compile GLSL shader: " + ShaderCompiler::getLastError());
        }

        this->compiledShaders.push_back(std::move(spirv));

        const auto &storedSpirv = this->compiledShaders.back();
        this->reflectShaderBytecode(storedSpirv.data(), storedSpirv.size() * sizeof(uint32_t));
    }

    this->descriptorSetLayouts = this->createDescriptorSetLayouts(this->bindingsBySetNumber);
    this->createPipelineLayout();
}

VulkanPipeline::~VulkanPipeline() {
    this->dispose();
}

void VulkanPipeline::createPipelineLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = this->descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = this->descriptorSetLayouts.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_ALL;
    pushConstant.offset = 0;
    pushConstant.size = MAX_PUSH_CONSTANT_SIZE;

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

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

VkShaderModule VulkanPipeline::createShaderModule(const std::string &shaderPath) {
    std::vector<uint32_t> code = FileUtils::readSPIRVFile(shaderPath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(this->device, &createInfo, nullptr, &shaderModule);

    return result == VK_SUCCESS ? shaderModule : VK_NULL_HANDLE;
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

VkPushConstantRange VulkanPipeline::getPushConstantRange() {
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

void VulkanPipeline::reflectShaderBytecode(const void *spirvCode, size_t spirvSize) {
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirvSize, spirvCode, &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw std::runtime_error("Failed to create SPIRV-Reflect module");
    }

    uint32_t bindingCount = 0;
    result = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        spvReflectDestroyShaderModule(&module);
        throw std::runtime_error("Failed to enumerate descriptor bindings");
    }

    if (bindingCount > 0) {
        std::vector<SpvReflectDescriptorBinding *> bindings(bindingCount);
        result = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data());

        if (result == SPV_REFLECT_RESULT_SUCCESS) {
            for (const auto *binding : bindings) {
                auto descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type);

                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = binding->binding;
                layoutBinding.descriptorType = descriptorType;
                layoutBinding.descriptorCount = binding->count;
                layoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
                layoutBinding.pImmutableSamplers = nullptr;

                this->bindingsBySetNumber[binding->set].push_back(layoutBinding);
            }
        }
    }

    spvReflectDestroyShaderModule(&module);
}

void VulkanPipeline::dispose() {
    if (this->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(this->device, this->pipeline, nullptr);
        this->pipeline = VK_NULL_HANDLE;
    }

    for (VkDescriptorSetLayout layout : this->descriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(this->device, layout, nullptr);
    }
    this->descriptorSetLayouts.clear();

    if (this->pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(this->device, this->pipelineLayout, nullptr);
        this->pipelineLayout = VK_NULL_HANDLE;
    }
}