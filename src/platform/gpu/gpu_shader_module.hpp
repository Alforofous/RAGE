#pragma once

#include <concepts>
#include "gpu_types.hpp"

namespace RAGE {
    /**
     * Backend-agnostic shader module concept.
     *
     * The minimal contract every shader-module implementation must satisfy so higher-layer
     * components (Material, PipelineCache, …) can query stage information without depending
     * on a concrete backend type.
     *
     * @param T Concrete shader-module type.
     */
    template <typename T>
    concept GpuShaderModule = requires(const T &t) {
        { t.stage() } -> std::convertible_to<ShaderStage>;
    };
}
