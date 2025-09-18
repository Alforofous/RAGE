#include "vulkan_descriptor_manager.hpp"
#include <stdexcept>
#include <iostream>

VulkanDescriptorManager::VulkanDescriptorManager(const VulkanContext *context)
    : context(context) {
    this->currentPool = this->createDescriptorPool();
}

VulkanDescriptorManager::~VulkanDescriptorManager() {
    this->dispose();
}

VkDescriptorSet VulkanDescriptorManager::createDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorPool pool = this->getOrCreateAvailablePool();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(this->context->device, &allocInfo, &descriptorSet);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        this->currentPool = this->createDescriptorPool();
        allocInfo.descriptorPool = this->currentPool;
        result = vkAllocateDescriptorSets(this->context->device, &allocInfo, &descriptorSet);
    }

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    for (auto &poolInfo : this->descriptorPools) {
        if (poolInfo.pool == pool) {
            poolInfo.allocatedSets++;
            break;
        }
    }

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
    for (auto &poolInfo : this->descriptorPools) {
        if (poolInfo.pool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(this->context->device, poolInfo.pool, 0);
            poolInfo.allocatedSets = 0;
        }
    }
}

VkDescriptorPool VulkanDescriptorManager::createDescriptorPool(uint32_t maxSets) {
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
    this->descriptorPools.push_back(poolInfo_struct);

    std::cout << "Created descriptor pool with max sets: " << maxSets << std::endl;

    return pool;
}

VkDescriptorPool VulkanDescriptorManager::getOrCreateAvailablePool() {
    for (const auto &poolInfo : this->descriptorPools) {
        if (poolInfo.allocatedSets < poolInfo.maxSets) {
            return poolInfo.pool;
        }
    }

    return this->createDescriptorPool();
}

void VulkanDescriptorManager::dispose() {
    for (auto &poolInfo : this->descriptorPools) {
        if (poolInfo.pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(this->context->device, poolInfo.pool, nullptr);
            poolInfo.pool = VK_NULL_HANDLE;
        }
    }
    this->descriptorPools.clear();
    this->currentPool = VK_NULL_HANDLE;
}