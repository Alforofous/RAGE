#pragma once
#include <vulkan/vulkan.h>
#include <map>
#include <vector>
#include "glsl_shader.hpp"

class VulkanPipeline {
public:
    VulkanPipeline(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<GLSLShader> &glslShaders);
    virtual ~VulkanPipeline();

    // === Core Pipeline Operations ===
    virtual void bind(VkCommandBuffer cmdBuffer) = 0;
    virtual void bindDescriptorSet(VkCommandBuffer cmdBuffer, uint32_t setIndex, VkDescriptorSet set) = 0;
    virtual VkPipelineBindPoint getBindPoint() const = 0;

    void pushConstants(VkCommandBuffer cmdBuffer, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data);

    // === Pipeline State ===
    bool isReady() const { return this->pipeline != VK_NULL_HANDLE; }

    // === Layout and Resource Access ===
    VkDevice getDevice() const { return this->device; }
    VkPipeline getPipeline() const { return this->pipeline; }
    VkPipelineLayout getLayout() const { return this->pipelineLayout; }

    // === Descriptor Set Information ===
    size_t getDescriptorSetLayoutCount() const { return this->descriptorSetLayouts.size(); }
    VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setIndex) const;

    // === Push Constants Information ===
    size_t getPushConstantRangeCount() const { return this->pushConstantRanges.size(); }
    const std::vector<VkPushConstantRange>& getPushConstantRanges() const { return this->pushConstantRanges; }

protected:
    struct ShaderInfo {
        VkShaderModule module = VK_NULL_HANDLE;
        VkShaderStageFlagBits stage;
    };

    // === Virtual Hook for Derived Classes ===
    virtual void createPipeline() = 0;

    // === Resource Access for Derived Classes ===
    const std::vector<ShaderInfo>& getCompiledShaders() const { return this->shaders; }
    std::vector<VkPipelineShaderStageCreateInfo> getShaderStages() const;

    // === Helper Methods for Derived Classes ===
    VkBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory);
    VkBuffer createDeviceAddressBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory);
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory memory);
    void *mapMemory(VkDeviceMemory memory, VkDeviceSize size);
    void unmapMemory(VkDeviceMemory memory);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);
    void copyToBuffer(VkDeviceMemory memory, const void *data, uint32_t dataSize, uint32_t bufferSize);

    VkPipeline pipeline = VK_NULL_HANDLE;

private:
    // === Private Members ===
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    // === Core Resources (Private - Use Accessors) ===
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
    std::vector<ShaderInfo> shaders;

    // === Construction and Cleanup ===
    void compileShaders(const std::vector<GLSLShader> &glslShaders);
    void createPipelineLayout();
    void dispose();

    // === Internal Implementation ===
    void createShaderModules(const std::vector<std::vector<uint32_t> > &compiledShaders, const std::vector<ShaderKind> &shaderKinds);
    void destroyShaderModule(VkShaderModule module);
    std::vector<VkDescriptorSetLayout> createDescriptorSetLayouts(const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > &bindingsBySetNumber);
    bool isValidPushConstantData(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // === Friend Access for Protected Members ===
    template<VkPipelineBindPoint> friend class VulkanPipelineBase;
};

template<VkPipelineBindPoint BindPoint>
class VulkanPipelineBase : public VulkanPipeline {
public:
    VulkanPipelineBase(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<GLSLShader> &glslShaders)
        : VulkanPipeline(device, physicalDevice, glslShaders) {}

    void bind(VkCommandBuffer cmdBuffer) override {
        vkCmdBindPipeline(cmdBuffer, BindPoint, this->getPipeline());
    }

    void bindDescriptorSet(VkCommandBuffer cmdBuffer, uint32_t setIndex, VkDescriptorSet set) override {
        if (setIndex >= this->getDescriptorSetLayoutCount()) {
            return;
        }
        vkCmdBindDescriptorSets(cmdBuffer, BindPoint, this->getLayout(), setIndex, 1, &set, 0, nullptr);
    }

    VkPipelineBindPoint getBindPoint() const override { return BindPoint; }
};