#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include "vulkan_utils.hpp"

class VulkanDescriptorManager {
public:
    VulkanDescriptorManager(const VulkanContext *context);
    ~VulkanDescriptorManager();

    // === Descriptor Set Management ===
    VkDescriptorSet createDescriptorSet(VkDescriptorSetLayout layout);
    VkDescriptorSet getOrCreateCachedDescriptorSet(VkDescriptorSetLayout layout, const void* cacheKey, size_t cacheKeySize);
    void updateStorageImage(VkDescriptorSet descriptorSet, uint32_t binding, VkImageView imageView, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
    void updateUniformBuffer(VkDescriptorSet descriptorSet, uint32_t binding, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);

    // === Command Buffer Operations ===
    static void bindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, uint32_t firstSet, VkDescriptorSet descriptorSet);

    // === Resource Management ===
    void resetDescriptorPool();
    void clearCache();
    void dispose();

private:
    const VulkanContext *context;

    // === Descriptor Pool Management ===
    struct DescriptorPoolInfo {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        uint32_t allocatedSets = 0;
        uint32_t maxSets;
    };

    std::vector<DescriptorPoolInfo> descriptorPools;
    VkDescriptorPool currentPool = VK_NULL_HANDLE;

    // === Descriptor Set Caching ===
    struct DescriptorSetCacheKey {
        std::vector<uint8_t> data;
        
        bool operator==(const DescriptorSetCacheKey& other) const {
            return this->data == other.data;
        }
    };
    
    struct DescriptorSetCacheKeyHash {
        size_t operator()(const DescriptorSetCacheKey& key) const {
            return std::hash<std::string>{}(std::string(key.data.begin(), key.data.end()));
        }
    };
    
    std::unordered_map<DescriptorSetCacheKey, VkDescriptorSet, DescriptorSetCacheKeyHash> descriptorSetCache;

    // === Pool Creation ===
    VkDescriptorPool createDescriptorPool(uint32_t maxSets = DEFAULT_MAX_SETS);
    VkDescriptorPool getOrCreateAvailablePool();

    // === Pool Size Configuration ===
    static constexpr uint32_t POOL_SIZE_STORAGE_IMAGE = 20;
    static constexpr uint32_t POOL_SIZE_UNIFORM_BUFFER = 40;
    static constexpr uint32_t POOL_SIZE_SAMPLER = 20;
    static constexpr uint32_t DEFAULT_MAX_SETS = 100;
};