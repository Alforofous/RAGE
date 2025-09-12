#pragma once
#include <vulkan/vulkan.h>
#include <map>
#include <vector>
#include "utils/shader_compiler.hpp"

class ShaderReflector {
public:
    struct ReflectionResult {
        std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > bindingsBySetNumber;
        std::vector<VkPushConstantRange> pushConstantRanges;
    };

    static ReflectionResult reflectShaderBytecode(const void *spirvCode, size_t spirvSize, ShaderKind shaderKind);
    static ReflectionResult combineReflectionResults(const std::vector<ReflectionResult> &results);
    static VkShaderStageFlagBits shaderKindToVkShaderStageFlagBits(ShaderKind kind);
};