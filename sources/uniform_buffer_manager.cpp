#include "uniform_buffer_manager.hpp"
#include "vulkan_utils.hpp"
#include "utils/buffer_utils.hpp"
#include <stdexcept>
#include <cstring>
#include <algorithm>

static constexpr size_t DEFAULT_BUFFER_SIZE = 1024;

UniformBufferManager::UniformBufferManager(const VulkanContext* context, uint32_t maxFramesInFlight)
    : context(context), maxFramesInFlight(maxFramesInFlight) {
}

UniformBufferManager::~UniformBufferManager() {
    this->dispose();
}

void UniformBufferManager::registerLayout(VkDescriptorSetLayout layout, const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>& bindings) {
    LayoutInfo layoutInfo;
    layoutInfo.bindings = bindings;
    
    size_t maxBindingSize = DEFAULT_BUFFER_SIZE;
    for (const auto& setBindings : bindings) {
        for (const auto& binding : setBindings.second) {
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                if (binding.descriptorCount > 0) {
                    size_t estimatedSize = static_cast<size_t>(binding.descriptorCount) * DEFAULT_BUFFER_SIZE;
                    maxBindingSize = std::max(maxBindingSize, estimatedSize);
                }
            }
        }
    }
    
    layoutInfo.defaultBufferSize = maxBindingSize;
    this->layoutInfo[layout] = layoutInfo;
}

UniformBufferManager::BufferSet* UniformBufferManager::getBufferSet(const void* object, uint32_t frameIndex, VkDescriptorSetLayout layout) {
    if (frameIndex >= this->maxFramesInFlight) {
        throw std::runtime_error("Frame index out of bounds");
    }

    BufferSetKey key;
    key.object = object;
    key.frameIndex = frameIndex;
    key.layout = layout;

    auto it = this->bufferSetCache.find(key);
    if (it != this->bufferSetCache.end()) {
        return it->second.get();
    }

    std::unique_ptr<BufferSet> bufferSetPtr = std::make_unique<BufferSet>();
    BufferSet* bufferSet = bufferSetPtr.get();
    
    auto layoutIt = this->layoutInfo.find(layout);
    if (layoutIt == this->layoutInfo.end()) {
        throw std::runtime_error("Layout not registered");
    }

    const LayoutInfo& info = layoutIt->second;

    for (const auto& setBindings : info.bindings) {
        for (const auto& binding : setBindings.second) {
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                size_t bufferSize = info.defaultBufferSize;
                
                VkBuffer buffer = BufferUtils::createBuffer(
                    this->context->device,
                    this->context->physicalDevice,
                    bufferSize,
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    bufferSet->memory[binding.binding]
                );
                
                bufferSet->buffers[binding.binding] = buffer;
                bufferSet->sizes[binding.binding] = bufferSize;
            }
        }
    }

    this->bufferSetCache[key] = std::move(bufferSetPtr);
    return bufferSet;
}

void UniformBufferManager::updateBuffer(BufferSet* bufferSet, uint32_t binding, const void* data, size_t size) {
    if (bufferSet == nullptr) {
        throw std::runtime_error("BufferSet is null");
    }

    auto bufferIt = bufferSet->buffers.find(binding);
    if (bufferIt == bufferSet->buffers.end()) {
        throw std::runtime_error("Binding not found in BufferSet");
    }

    size_t currentSize = bufferSet->sizes[binding];
    
    if (size > currentSize) {
        VkDeviceMemory oldMemory = bufferSet->memory[binding];
        VkBuffer oldBuffer = bufferSet->buffers[binding];
        
        BufferUtils::destroyBuffer(this->context->device, oldBuffer, oldMemory);
        
        VkBuffer newBuffer = BufferUtils::createBuffer(
            this->context->device,
            this->context->physicalDevice,
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufferSet->memory[binding]
        );
        
        bufferSet->buffers[binding] = newBuffer;
        bufferSet->sizes[binding] = size;
    }

    void* mappedData = nullptr;
    VkResult result = vkMapMemory(this->context->device, bufferSet->memory[binding], 0, size, 0, &mappedData);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map uniform buffer memory");
    }
    
    std::memcpy(mappedData, data, size);
    vkUnmapMemory(this->context->device, bufferSet->memory[binding]);
}

VkBuffer UniformBufferManager::getBuffer(BufferSet* bufferSet, uint32_t binding) const {
    if (bufferSet == nullptr) {
        throw std::runtime_error("BufferSet is null");
    }

    auto it = bufferSet->buffers.find(binding);
    if (it == bufferSet->buffers.end()) {
        throw std::runtime_error("Binding not found in BufferSet");
    }

    return it->second;
}

size_t UniformBufferManager::getBufferSize(BufferSet* bufferSet, uint32_t binding) const {
    if (bufferSet == nullptr) {
        throw std::runtime_error("BufferSet is null");
    }

    auto it = bufferSet->sizes.find(binding);
    if (it == bufferSet->sizes.end()) {
        throw std::runtime_error("Binding not found in BufferSet");
    }

    return it->second;
}

void UniformBufferManager::dispose() {
    for (auto& pair : this->bufferSetCache) {
        BufferSet* bufferSet = pair.second.get();
        for (auto& bufferPair : bufferSet->buffers) {
            uint32_t binding = bufferPair.first;
            VkBuffer buffer = bufferPair.second;
            VkDeviceMemory memory = bufferSet->memory[binding];
            BufferUtils::destroyBuffer(this->context->device, buffer, memory);
        }
    }
    this->bufferSetCache.clear();
    this->layoutInfo.clear();
}

