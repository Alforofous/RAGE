#pragma once

#include <functional>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>

namespace RAGE::Toolkit {
    /**
     * @brief Window-agnostic handles the pipeline needs from whoever owns the OS
     *        window. The engine never depends on a window type (library-shape rule):
     *        the app layer binds these from its windowing system (GLFW, SDL, ...)
     *        and the pipeline consumes them without knowing who made them.
     */
    struct WindowSurfaceSource {
        /// Instance extensions the windowing system requires (e.g. VK_KHR_surface + platform).
        std::vector<const char *> instanceExtensions;
        /// Creates the presentation surface once the instance exists. Must throw on failure.
        std::function<VkSurfaceKHR(VkInstance)> createSurface;
        /// Polled per frame for the current framebuffer size in pixels.
        std::function<std::pair<uint32_t, uint32_t>()> framebufferExtent;
    };
}
