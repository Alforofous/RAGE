#include "vulkan_descriptor_manager.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>

VulkanDescriptorManager::VulkanDescriptorManager(const VulkanContext *context, uint32_t maxFramesInFlight)
    : context(context), maxFramesInFlight(maxFramesInFlight) {
    this->descriptorPools.resize(maxFramesInFlight);
    this->descriptorSetCachePerFrame.resize(maxFramesInFlight);
    for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
        this->createDescriptorPool(i);
    }
}

VulkanDescriptorManager::~VulkanDescriptorManager() {
    this->dispose();
}

VkDescriptorSet VulkanDescriptorManager::createDescriptorSet(VkDescriptorSetLayout layout) {
    return this->createDescriptorSet(0, layout);
}

VkDescriptorSet VulkanDescriptorManager::createDescriptorSet(uint32_t frameIndex, VkDescriptorSetLayout layout) {
    if (frameIndex >= this->maxFramesInFlight) {
        throw std::runtime_error("Frame index out of bounds");
    }

    VkDescriptorPool pool = this->getOrCreateAvailablePool(frameIndex);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(this->context->device, &allocInfo, &descriptorSet);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        VkDescriptorPool newPool = this->createDescriptorPool(frameIndex);
        allocInfo.descriptorPool = newPool;
        result = vkAllocateDescriptorSets(this->context->device, &allocInfo, &descriptorSet);
        pool = newPool;
    }

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    for (auto &poolInfo : this->descriptorPools[frameIndex]) {
        if (poolInfo.pool == pool) {
            poolInfo.allocatedSets++;
            break;
        }
    }

    return descriptorSet;
}

VkDescriptorSet VulkanDescriptorManager::getOrCreateCachedDescriptorSet(VkDescriptorSetLayout layout, const void* cacheKey, size_t cacheKeySize) {
    return this->getOrCreateCachedDescriptorSet(0, layout, cacheKey, cacheKeySize);
}

VkDescriptorSet VulkanDescriptorManager::getOrCreateCachedDescriptorSet(uint32_t frameIndex, VkDescriptorSetLayout layout, const void* cacheKey, size_t cacheKeySize) {
    if (frameIndex >= this->maxFramesInFlight) {
        throw std::runtime_error("Frame index out of bounds");
    }

    DescriptorSetCacheKey key(cacheKey, cacheKeySize);
    
    auto it = this->descriptorSetCachePerFrame[frameIndex].find(key);
    if (it != this->descriptorSetCachePerFrame[frameIndex].end()) {
        return it->second;
    }
    
    VkDescriptorSet descriptorSet = this->createDescriptorSet(frameIndex, layout);
    this->descriptorSetCachePerFrame[frameIndex][key] = descriptorSet;
    
    return descriptorSet;
}

void VulkanDescriptorManager::updateStorageImage(VkDescriptorSet descriptorSet, uint32_t binding, VkImageView imageView, VkImageLayout layout) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = layout;
    imageInfo.imageView = imageView;
    imageInfo.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(this->context->device, 1, &descriptorWrite, 0, nullptr);
}

void VulkanDescriptorManager::updateUniformBuffer(VkDescriptorSet descriptorSet, uint32_t binding, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = size;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(this->context->device, 1, &descriptorWrite, 0, nullptr);
}

void VulkanDescriptorManager::bindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout, uint32_t firstSet, VkDescriptorSet descriptorSet) {
    vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout, firstSet, 1, &descriptorSet, 0, nullptr);
}

void VulkanDescriptorManager::resetDescriptorPool() {
    for (uint32_t i = 0; i < this->maxFramesInFlight; ++i) {
        this->resetDescriptorPoolForFrame(i);
    }
}

void VulkanDescriptorManager::resetDescriptorPoolForFrame(uint32_t frameIndex) {
    if (frameIndex >= this->maxFramesInFlight) {
        throw std::runtime_error("Frame index out of bounds");
    }

    this->descriptorSetCachePerFrame[frameIndex].clear();
    for (auto &poolInfo : this->descriptorPools[frameIndex]) {
        if (poolInfo.pool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(this->context->device, poolInfo.pool, 0);
            poolInfo.allocatedSets = 0;
        }
    }
}

void VulkanDescriptorManager::clearCache() {
    for (auto &cache : this->descriptorSetCachePerFrame) {
        cache.clear();
    }
}

size_t VulkanDescriptorManager::getCacheSize() const {
    size_t total = 0;
    for (const auto &cache : this->descriptorSetCachePerFrame) {
        total += cache.size();
    }
    return total;
}

size_t VulkanDescriptorManager::getTotalPoolCount() const {
    size_t total = 0;
    for (const auto &framePools : this->descriptorPools) {
        total += framePools.size();
    }
    return total;
}

size_t VulkanDescriptorManager::getTotalAllocatedSets() const {
    size_t total = 0;
    for (const auto& framePools : this->descriptorPools) {
        for (const auto& poolInfo : framePools) {
            total += poolInfo.allocatedSets;
        }
    }
    return total;
}

float VulkanDescriptorManager::getPoolUtilization() const {
    size_t totalAllocated = 0;
    size_t totalCapacity = 0;
    
    for (const auto& framePools : this->descriptorPools) {
        for (const auto& poolInfo : framePools) {
            totalAllocated += poolInfo.allocatedSets;
            totalCapacity += poolInfo.maxSets;
        }
    }
    
    return totalCapacity > 0 ? static_cast<float>(totalAllocated) / static_cast<float>(totalCapacity) : 0.0f;
}

VkDescriptorPool VulkanDescriptorManager::createDescriptorPool(uint32_t frameIndex, uint32_t maxSets) {
    if (frameIndex >= this->maxFramesInFlight) {
        throw std::runtime_error("Frame index out of bounds");
    }

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, POOL_SIZE_STORAGE_IMAGE },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, POOL_SIZE_UNIFORM_BUFFER },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, POOL_SIZE_SAMPLER }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(this->context->device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    DescriptorPoolInfo poolInfo_struct{};
    poolInfo_struct.pool = pool;
    poolInfo_struct.allocatedSets = 0;
    poolInfo_struct.maxSets = maxSets;
    this->descriptorPools[frameIndex].push_back(poolInfo_struct);

    std::cout << "Created descriptor pool for frame " << frameIndex << " with max sets: " << maxSets << std::endl;

    return pool;
}

VkDescriptorPool VulkanDescriptorManager::getOrCreateAvailablePool(uint32_t frameIndex) {
    if (frameIndex >= this->maxFramesInFlight) {
        throw std::runtime_error("Frame index out of bounds");
    }

    for (const auto &poolInfo : this->descriptorPools[frameIndex]) {
        if (poolInfo.allocatedSets < poolInfo.maxSets) {
            return poolInfo.pool;
        }
    }

    return this->createDescriptorPool(frameIndex);
}

void VulkanDescriptorManager::dispose() {
    this->clearCache();
    for (auto &framePools : this->descriptorPools) {
        for (auto &poolInfo : framePools) {
            if (poolInfo.pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(this->context->device, poolInfo.pool, nullptr);
                poolInfo.pool = VK_NULL_HANDLE;
            }
        }
        framePools.clear();
    }
    this->descriptorPools.clear();
    this->descriptorSetCachePerFrame.clear();
}