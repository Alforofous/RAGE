#include "pipelines/shader_reflector.hpp"
#include <spirv_reflect.h>
#include <stdexcept>

ShaderReflector::ReflectionResult ShaderReflector::reflectShaderBytecode(const void *spirvCode, size_t spirvSize, ShaderKind shaderKind) {
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirvSize, spirvCode, &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw std::runtime_error("Failed to create SPIRV-Reflect module");
    }

    ReflectionResult reflectionResult;

    uint32_t bindingCount = 0;
    result = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        spvReflectDestroyShaderModule(&module);
        throw std::runtime_error("Failed to enumerate descriptor bindings");
    }

    if (bindingCount > 0) {
        std::vector<SpvReflectDescriptorBinding *> bindings(bindingCount);
        result = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data());

        if (result == SPV_REFLECT_RESULT_SUCCESS) {
            VkShaderStageFlagBits stageFlags = shaderKindToVkShaderStageFlagBits(shaderKind);

            for (const auto *binding : bindings) {
                auto descriptorType = static_cast<VkDescriptorType>(binding->descriptor_type);

                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = binding->binding;
                layoutBinding.descriptorType = descriptorType;
                layoutBinding.descriptorCount = binding->count;
                layoutBinding.stageFlags = stageFlags;
                layoutBinding.pImmutableSamplers = nullptr;

                reflectionResult.bindingsBySetNumber[binding->set].push_back(layoutBinding);
            }
        }
    }

    uint32_t pushConstantCount = 0;
    result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);

    if (result == SPV_REFLECT_RESULT_SUCCESS && pushConstantCount > 0) {
        std::vector<SpvReflectBlockVariable *> pushConstants(pushConstantCount);
        result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());

        if (result == SPV_REFLECT_RESULT_SUCCESS) {
            for (const auto *pushConstant : pushConstants) {
                VkPushConstantRange range{};
                range.stageFlags = shaderKindToVkShaderStageFlagBits(shaderKind);
                range.offset = pushConstant->offset;
                range.size = pushConstant->size;

                reflectionResult.pushConstantRanges.push_back(range);
            }
        }
    }

    spvReflectDestroyShaderModule(&module);

    return reflectionResult;
}

ShaderReflector::ReflectionResult ShaderReflector::combineReflectionResults(const std::vector<ReflectionResult> &results) {
    ReflectionResult combined;

    for (const auto &result : results) {
        for (const auto &setBindings : result.bindingsBySetNumber) {
            uint32_t setNumber = setBindings.first;
            const auto &bindings = setBindings.second;

            for (const auto &binding : bindings) {
                auto &existingBindings = combined.bindingsBySetNumber[setNumber];
                auto existingIt = std::find_if(existingBindings.begin(), existingBindings.end(),
                                               [&binding](const VkDescriptorSetLayoutBinding &existing) {
                    return existing.binding == binding.binding;
                });

                if (existingIt != existingBindings.end()) {
                    existingIt->stageFlags |= binding.stageFlags;
                }
                else {
                    existingBindings.push_back(binding);
                }
            }
        }
    }

    for (const auto &result : results) {
        for (const auto &range : result.pushConstantRanges) {
            auto existingIt = std::find_if(combined.pushConstantRanges.begin(), combined.pushConstantRanges.end(),
                                           [&range](const VkPushConstantRange &existing) {
                return existing.offset == range.offset && existing.size == range.size;
            });

            if (existingIt != combined.pushConstantRanges.end()) {
                existingIt->stageFlags |= range.stageFlags;
            }
            else {
                combined.pushConstantRanges.push_back(range);
            }
        }
    }

    return combined;
}

VkShaderStageFlagBits ShaderReflector::shaderKindToVkShaderStageFlagBits(ShaderKind kind) {
    switch (kind) {
        case ShaderKind::VERTEX:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderKind::FRAGMENT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderKind::COMPUTE:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderKind::RAYGEN:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case ShaderKind::MISS:
            return VK_SHADER_STAGE_MISS_BIT_KHR;
        case ShaderKind::CLOSEST_HIT:
            return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case ShaderKind::ANY_HIT:
            return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        default:
            throw std::runtime_error("Invalid shader kind");
    }
}