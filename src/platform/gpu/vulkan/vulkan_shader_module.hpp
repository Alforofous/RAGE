#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu/gpu_shader_module.hpp"
#include "gpu/gpu_types.hpp"

namespace RAGE {
    struct ShaderDescriptorBinding {
        uint32_t set = 0;
        uint32_t binding = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        uint32_t count = 1;
        VkShaderStageFlags stages = 0;
    };

    struct ShaderPushConstantRange {
        uint32_t offset = 0;
        uint32_t size = 0;
        VkShaderStageFlags stages = 0;
    };

    /**
     * A reflected Vulkan shader module loaded from SPIR-V.
     *
     * Wraps a single VkShaderModule and the SPIR-V reflection metadata extracted via
     * SPIRV-Reflect: entry-point name, stage, descriptor-set bindings, push-constant ranges,
     * and (for compute) the local workgroup size declared in the shader. The reflection data is
     * captured at construction and held by value — the SPIRV-Reflect parser state does not
     * outlive the constructor.
     *
     * Move-only. The VkShaderModule is destroyed in the destructor; the supplied SPIR-V byte
     * range does not need to outlive the constructor.
     *
     * @param device  Vulkan device that owns the resulting VkShaderModule.
     * @param spirv   SPIR-V code as a span of uint32_t words; size must be a multiple of 4 bytes.
     * @param entry   Name of the entry point to reflect against. Defaults to "main".
     *
     * @note For non-compute stages, localWorkgroupSize() returns {0, 0, 0}.
     */
    class VulkanShaderModule {
    public:
        VulkanShaderModule() = delete;
        VulkanShaderModule(VkDevice device, std::span<const uint32_t> spirv, std::string entry = "main");
        ~VulkanShaderModule();

        VulkanShaderModule(const VulkanShaderModule &) = delete;
        VulkanShaderModule &operator=(const VulkanShaderModule &) = delete;
        VulkanShaderModule(VulkanShaderModule &&other) noexcept;
        VulkanShaderModule &operator=(VulkanShaderModule &&other) noexcept;

        static VulkanShaderModule fromFile(VkDevice device, const std::filesystem::path &path,
                                           std::string entry = "main");

        VkShaderModule handle() const { return module_; }
        const std::string &entryPoint() const { return entryPoint_; }
        ShaderStage stage() const { return stage_; }
        VkShaderStageFlagBits vkStage() const { return vkStage_; }
        std::span<const ShaderDescriptorBinding> bindings() const { return bindings_; }
        std::span<const ShaderPushConstantRange> pushConstants() const { return pushConstants_; }
        std::array<uint32_t, 3> localWorkgroupSize() const { return localWorkgroupSize_; }

    private:
        void destroy() noexcept;
        void swap(VulkanShaderModule &other) noexcept;

        VkDevice device_ = VK_NULL_HANDLE;
        VkShaderModule module_ = VK_NULL_HANDLE;
        std::string entryPoint_;
        ShaderStage stage_ = ShaderStage::Compute;
        VkShaderStageFlagBits vkStage_ = VK_SHADER_STAGE_COMPUTE_BIT;
        std::vector<ShaderDescriptorBinding> bindings_;
        std::vector<ShaderPushConstantRange> pushConstants_;
        std::array<uint32_t, 3> localWorkgroupSize_{ 0, 0, 0 };
    };

    static_assert(GpuShaderModule<VulkanShaderModule>, "VulkanShaderModule must satisfy GpuShaderModule concept");
}
