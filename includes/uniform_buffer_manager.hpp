#pragma once
#include <vulkan/vulkan.h>
#include <map>
#include <vector>
#include <memory>
#include <cstdint>

class VulkanContext;

class UniformBufferManager {
public:
    UniformBufferManager(const VulkanContext* context, uint32_t maxFramesInFlight);
    ~UniformBufferManager();
    
    struct BufferSet {
        std::map<uint32_t, VkBuffer> buffers;
        std::map<uint32_t, VkDeviceMemory> memory;
        std::map<uint32_t, size_t> sizes;
    };
    
    void registerLayout(VkDescriptorSetLayout layout, const std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>& bindings);
    
    BufferSet* getBufferSet(const void* object, uint32_t frameIndex, VkDescriptorSetLayout layout);
    
    void updateBuffer(BufferSet* bufferSet, uint32_t binding, const void* data, size_t size);
    
    VkBuffer getBuffer(BufferSet* bufferSet, uint32_t binding) const;
    size_t getBufferSize(BufferSet* bufferSet, uint32_t binding) const;
    
    void dispose();
    
private:
    const VulkanContext* context;
    const uint32_t maxFramesInFlight;
    
    struct LayoutInfo {
        std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bindings;
        size_t defaultBufferSize;
    };
    std::map<VkDescriptorSetLayout, LayoutInfo> layoutInfo;
    
    struct BufferSetKey {
        const void* object;
        uint32_t frameIndex;
        VkDescriptorSetLayout layout;
        
        bool operator<(const BufferSetKey& other) const {
            if (this->object != other.object) {
                return this->object < other.object;
            }
            if (this->frameIndex != other.frameIndex) {
                return this->frameIndex < other.frameIndex;
            }
            return this->layout < other.layout;
        }
    };
    std::map<BufferSetKey, std::unique_ptr<BufferSet>> bufferSetCache;
};

