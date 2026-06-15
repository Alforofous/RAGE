#include "vulkan_shader_module.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <spirv_reflect.h>

namespace {
    RAGE::ShaderStage toRageStage(SpvReflectShaderStageFlagBits stage) {
        switch (stage) {
            case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
                return RAGE::ShaderStage::Vertex;
            case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
                return RAGE::ShaderStage::Fragment;
            case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
                return RAGE::ShaderStage::Compute;
            case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
                return RAGE::ShaderStage::RayGeneration;
            case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
                return RAGE::ShaderStage::Miss;
            case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
                return RAGE::ShaderStage::ClosestHit;
            case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
                return RAGE::ShaderStage::AnyHit;
            case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
                return RAGE::ShaderStage::Intersection;
            case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:
                return RAGE::ShaderStage::Callable;
            default:
                throw std::runtime_error("VulkanShaderModule: unsupported SPIR-V stage");
        }
    }

    VkShaderStageFlagBits toVkStage(SpvReflectShaderStageFlagBits stage) {
        return static_cast<VkShaderStageFlagBits>(stage);
    }

    std::vector<uint32_t> readSpirvFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("VulkanShaderModule: failed to open SPIR-V file: " + path.string());
        }

        const std::streamsize byteSize = file.tellg();
        if (byteSize < 0 || (byteSize % sizeof(uint32_t)) != 0) {
            throw std::runtime_error("VulkanShaderModule: SPIR-V file size not a multiple of 4 bytes");
        }

        std::vector<uint32_t> words(static_cast<size_t>(byteSize) / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char *>(words.data()), byteSize);

        return words;
    }
}

namespace RAGE {
    VulkanShaderModule::VulkanShaderModule(VkDevice device, std::span<const uint32_t> spirv, std::string entry)
        : device_(device)
        , entryPoint_(std::move(entry)) {
        if (device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("VulkanShaderModule: null device");
        }
        if (spirv.empty()) {
            throw std::runtime_error("VulkanShaderModule: empty SPIR-V");
        }

        SpvReflectShaderModule reflect{};
        const SpvReflectResult rr = spvReflectCreateShaderModule(spirv.size_bytes(), spirv.data(), &reflect);
        if (rr != SPV_REFLECT_RESULT_SUCCESS) {
            throw std::runtime_error("VulkanShaderModule: SPIRV-Reflect failed to parse module");
        }

        try {
            stage_ = toRageStage(reflect.shader_stage);
            vkStage_ = toVkStage(reflect.shader_stage);

            const SpvReflectEntryPoint *ep = spvReflectGetEntryPoint(&reflect, entryPoint_.c_str());
            if (ep == nullptr) {
                throw std::runtime_error("VulkanShaderModule: entry point not found in SPIR-V: " + entryPoint_);
            }

            if (stage_ == ShaderStage::Compute) {
                localWorkgroupSize_ = { ep->local_size.x, ep->local_size.y, ep->local_size.z };
            }

            uint32_t setCount = 0;
            spvReflectEnumerateEntryPointDescriptorSets(&reflect, entryPoint_.c_str(), &setCount, nullptr);
            std::vector<SpvReflectDescriptorSet *> sets(setCount);
            spvReflectEnumerateEntryPointDescriptorSets(&reflect, entryPoint_.c_str(), &setCount, sets.data());

            for (const SpvReflectDescriptorSet *set : sets) {
                for (uint32_t i = 0; i < set->binding_count; ++i) {
                    const SpvReflectDescriptorBinding *b = set->bindings[i];
                    uint32_t count = 1;
                    for (uint32_t d = 0; d < b->array.dims_count; ++d) {
                        count *= b->array.dims[d];
                    }
                    bindings_.push_back({ .set = set->set,
                                          .binding = b->binding,
                                          .type = static_cast<VkDescriptorType>(b->descriptor_type),
                                          .count = count,
                                          .stages = static_cast<VkShaderStageFlags>(vkStage_) });
                }
            }

            uint32_t pcCount = 0;
            spvReflectEnumerateEntryPointPushConstantBlocks(&reflect, entryPoint_.c_str(), &pcCount, nullptr);
            std::vector<SpvReflectBlockVariable *> pcs(pcCount);
            spvReflectEnumerateEntryPointPushConstantBlocks(&reflect, entryPoint_.c_str(), &pcCount, pcs.data());

            for (const SpvReflectBlockVariable *pc : pcs) {
                pushConstants_.push_back(
                    { .offset = pc->offset, .size = pc->size, .stages = static_cast<VkShaderStageFlags>(vkStage_) });
            }
        } catch (...) {
            spvReflectDestroyShaderModule(&reflect);
            throw;
        }

        spvReflectDestroyShaderModule(&reflect);

        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = spirv.size_bytes();
        ci.pCode = spirv.data();

        if (vkCreateShaderModule(device_, &ci, nullptr, &module_) != VK_SUCCESS) {
            throw std::runtime_error("VulkanShaderModule: vkCreateShaderModule failed");
        }
    }

    VulkanShaderModule::~VulkanShaderModule() {
        destroy();
    }

    VulkanShaderModule::VulkanShaderModule(VulkanShaderModule &&other) noexcept {
        swap(other);
    }

    VulkanShaderModule &VulkanShaderModule::operator=(VulkanShaderModule &&other) noexcept {
        if (this != &other) {
            destroy();
            swap(other);
        }

        return *this;
    }

    VulkanShaderModule VulkanShaderModule::fromFile(VkDevice device, const std::filesystem::path &path,
                                                    std::string entry) {
        const std::vector<uint32_t> spirv = readSpirvFile(path);

        return VulkanShaderModule(device, spirv, std::move(entry));
    }

    void VulkanShaderModule::destroy() noexcept {
        if (module_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module_, nullptr);
            module_ = VK_NULL_HANDLE;
        }
    }

    void VulkanShaderModule::swap(VulkanShaderModule &other) noexcept {
        std::swap(device_, other.device_);
        std::swap(module_, other.module_);
        std::swap(entryPoint_, other.entryPoint_);
        std::swap(stage_, other.stage_);
        std::swap(vkStage_, other.vkStage_);
        std::swap(bindings_, other.bindings_);
        std::swap(pushConstants_, other.pushConstants_);
        std::swap(localWorkgroupSize_, other.localWorkgroupSize_);
    }
}
