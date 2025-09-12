#pragma once
#include <vulkan/vulkan.h>
#include <map>
#include <vector>
#include "glsl_shader.hpp"

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
    struct ShaderInfo {
        VkShaderModule module = VK_NULL_HANDLE;
        VkShaderStageFlagBits stage;
    };

    VkDevice device;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > bindingsBySetNumber;
    std::vector<VkPushConstantRange> pushConstantRanges;
    std::vector<ShaderInfo> shaders;
    void destroyShaderModule(VkShaderModule module);

    void dispose();
    std::vector<VkPipelineShaderStageCreateInfo> getShaderStages() const;

private:
    void compileShaders(const std::vector<GLSLShader> &glslShaders);
    void createShaderModules(const std::vector<std::vector<uint32_t> > &compiledShaders, const std::vector<ShaderKind> &shaderKinds);
    void createDescriptorLayouts();
    void createPipelineLayout();

    std::vector<VkDescriptorSetLayout> createDescriptorSetLayouts(const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > &bindingsBySetNumber);
    void reflectShaders(const std::vector<std::vector<uint32_t> > &compiledShaders, const std::vector<ShaderKind> &shaderKinds);
    static bool isValidPushConstantSize(uint32_t size);
};