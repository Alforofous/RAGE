#pragma once
#include <vulkan/vulkan.h>
#include <map>
#include <vector>
#include "glsl_shader.hpp"

class VulkanPipeline {
public:
    VulkanPipeline(VkDevice device, const std::vector<GLSLShader> &glslShaders);
    virtual ~VulkanPipeline();

    virtual void bind(VkCommandBuffer cmdBuffer) = 0;
    virtual void bindDescriptorSet(VkCommandBuffer cmdBuffer, uint32_t setIndex, VkDescriptorSet set) = 0;
    virtual VkPipelineBindPoint getBindPoint() const = 0;

    void pushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data);

    bool isReady() const { return this->pipeline != VK_NULL_HANDLE; }

    VkPipelineLayout getLayout() const { return this->pipelineLayout; }
    VkPipeline getPipeline() const { return this->pipeline; }

    size_t getDescriptorSetLayoutCount() const { return this->descriptorSetLayouts.size(); }
    VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setIndex) const;

    size_t getPushConstantRangeCount() const { return this->pushConstantRanges.size(); }
    const std::vector<VkPushConstantRange>& getPushConstantRanges() const { return this->pushConstantRanges; }

protected:
    struct ShaderInfo {
        VkShaderModule module = VK_NULL_HANDLE;
        VkShaderStageFlagBits stage;
    };

    virtual void createPipeline() = 0;

    void compileShaders(const std::vector<GLSLShader> &glslShaders);
    void createPipelineLayout();
    void dispose();
    std::vector<VkPipelineShaderStageCreateInfo> getShaderStages() const;

    const VkDevice device;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
    std::vector<ShaderInfo> shaders;

private:
    void createShaderModules(const std::vector<std::vector<uint32_t> > &compiledShaders, const std::vector<ShaderKind> &shaderKinds);
    void destroyShaderModule(VkShaderModule module);
    std::vector<VkDescriptorSetLayout> createDescriptorSetLayouts(const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > &bindingsBySetNumber);
    bool isValidPushConstantData(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) const;
};

template<VkPipelineBindPoint BindPoint>
class VulkanPipelineBase : public VulkanPipeline {
public:
    VulkanPipelineBase(VkDevice device, const std::vector<GLSLShader> &glslShaders);

    void bind(VkCommandBuffer cmdBuffer) override;
    void bindDescriptorSet(VkCommandBuffer cmdBuffer, uint32_t setIndex, VkDescriptorSet set) override;
    VkPipelineBindPoint getBindPoint() const override { return BindPoint; }
};