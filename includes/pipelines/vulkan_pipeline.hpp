#pragma once
#include <vulkan/vulkan.h>
#include <map>
#include <vector>
#include "glsl_shader.hpp"
#include "pipelines/shader_reflector.hpp"

class VulkanPipeline {
public:
    static constexpr uint32_t MAX_PUSH_CONSTANT_SIZE = 256;
    VulkanPipeline(VkDevice device, const std::vector<GLSLShader> &glslShaders);
    virtual ~VulkanPipeline();

    virtual void bind(VkCommandBuffer cmdBuffer) = 0;

    void bindDescriptorSet(VkCommandBuffer cmdBuffer, uint32_t setIndex, VkDescriptorSet set);
    void pushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data);

    uint32_t getDescriptorSetLayoutCount() const { return this->descriptorSetLayouts.size(); }
    VkPushConstantRange getPushConstantRange() const;
    VkPipelineLayout getLayout() const { return this->pipelineLayout; }
    VkPipeline getPipeline() const { return this->pipeline; }
    VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setIndex) const;

protected:
    VkDevice device;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > bindingsBySetNumber;
    std::vector<VkPushConstantRange> pushConstantRanges;
    std::vector<std::vector<uint32_t> > compiledShaders;
    std::vector<VkShaderModule> shaderModules;
    std::vector<ShaderKind> shaderKinds;
    void destroyShaderModule(VkShaderModule module);

    void dispose();
    VkShaderModule createShaderModuleFromSPIRV(size_t shaderIndex);
    VkShaderModule getShaderModule(size_t shaderIndex) const;
    std::vector<VkPipelineShaderStageCreateInfo> getShaderStages() const;

private:
    // Initialization phases
    void compileShaders(const std::vector<GLSLShader> &glslShaders);
    void createShaderModules();
    void createDescriptorLayouts();
    void createPipelineLayout();

    // Helper methods
    std::vector<VkDescriptorSetLayout> createDescriptorSetLayouts(const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > &bindingsBySetNumber);
    void reflectShaders();
    static bool isValidPushConstantSize(uint32_t size);
};