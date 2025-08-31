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
    static VkPushConstantRange getPushConstantRange();
    VkPipelineLayout getLayout() const { return this->pipelineLayout; }
    VkPipeline getPipeline() const { return this->pipeline; }
    VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setIndex) const;

protected:
    VkDevice device;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > bindingsBySetNumber;
    std::vector<std::vector<uint32_t> > compiledShaders;  // Storage for compiled SPIR-V bytecode
    void destroyShaderModule(VkShaderModule module);
    VkShaderModule createShaderModule(const std::string &shaderPath);

    void dispose();
    VkShaderModule createShaderModuleFromSPIRV(size_t shaderIndex);  // Create VkShaderModule from compiled SPIR-V

private:
    std::vector<VkDescriptorSetLayout> createDescriptorSetLayouts(const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > &bindingsBySetNumber);
    void createPipelineLayout();
    void reflectShaderBytecode(const void *spirvCode, size_t spirvSize);
    static bool isValidPushConstantSize(uint32_t size);
};