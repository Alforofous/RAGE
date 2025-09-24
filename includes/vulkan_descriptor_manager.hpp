#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <cstring>
#include <array>
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
    
    // === Debug/Monitoring ===
    size_t getCacheSize() const;
    size_t getTotalPoolCount() const;
    size_t getTotalAllocatedSets() const;
    float getPoolUtilization() const;

private:
    const VulkanContext *context;

    // === Descriptor Pool Management ===
    struct DescriptorPoolInfo {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        uint32_t allocatedSets = 0;
        uint32_t maxSets;
    };

    std::vector<DescriptorPoolInfo> descriptorPools;

    // === Descriptor Set Caching ===
    struct DescriptorSetCacheKey {
        static constexpr size_t MAX_KEY_SIZE = 64;
        static constexpr size_t CACHE_KEY_ALIGNMENT = 8;
        alignas(CACHE_KEY_ALIGNMENT) std::array<uint8_t, MAX_KEY_SIZE> data;
        size_t size;
        
        DescriptorSetCacheKey() : size(0) {}
        
        DescriptorSetCacheKey(const void* keyData, size_t keySize) : size(keySize) {
            if (keySize > MAX_KEY_SIZE) {
                throw std::runtime_error("Cache key too large");
            }
            std::memcpy(this->data.data(), keyData, keySize);
        }
        
        bool operator==(const DescriptorSetCacheKey& other) const {
            return this->size == other.size && 
                   std::memcmp(this->data.data(), other.data.data(), this->size) == 0;
        }
    };
    
    struct DescriptorSetCacheKeyHash {
        size_t operator()(const DescriptorSetCacheKey& key) const {
            size_t hash = 0;
            for (size_t i = 0; i < key.size; ++i) {
                hash ^= std::hash<uint8_t>{}(key.data[i]) + HASH_MAGIC_CONSTANT + (hash << HASH_LEFT_SHIFT) + (hash >> HASH_RIGHT_SHIFT);
            }
            return hash;
        }
    };
    
    std::unordered_map<DescriptorSetCacheKey, VkDescriptorSet, DescriptorSetCacheKeyHash> descriptorSetCache;

    // === Pool Creation ===
    VkDescriptorPool createDescriptorPool(uint32_t maxSets = DEFAULT_MAX_SETS);
    VkDescriptorPool getOrCreateAvailablePool();

    // === Configuration Constants ===
    static constexpr uint32_t POOL_SIZE_STORAGE_IMAGE = 20;
    static constexpr uint32_t POOL_SIZE_UNIFORM_BUFFER = 40;
    static constexpr uint32_t POOL_SIZE_SAMPLER = 20;
    static constexpr uint32_t DEFAULT_MAX_SETS = 100;
    
    // Hash function constants
    static constexpr size_t HASH_MAGIC_CONSTANT = 0x9e3779b9;
    static constexpr size_t HASH_LEFT_SHIFT = 6;
    static constexpr size_t HASH_RIGHT_SHIFT = 2;

};