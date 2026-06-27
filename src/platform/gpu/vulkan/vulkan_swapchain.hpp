#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu_queue_kind.hpp"
#include "gpu_types.hpp"
#include "vulkan_semaphore_pool.hpp"

namespace RAGE {
    template <QueueKind K> class VulkanQueue;

    struct VulkanSwapchainCreateInfo {
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        uint32_t graphicsQueueFamily = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        bool vsync = true;
    };

    struct VulkanSwapchainAcquire {
        uint32_t imageIndex = 0;
        bool outOfDate = false;
    };

    /**
     * A Vulkan swapchain bound to a borrowed VkSurfaceKHR.
     *
     * VulkanSwapchain owns the VkSwapchainKHR and one VkImageView per swapchain image; the
     * VkSurfaceKHR is borrowed and must outlive this object. Construction selects an
     * 8-bit BGRA/RGBA SRGB surface format when available and falls back to the first reported
     * format otherwise. Present mode is FIFO when vsync is requested, otherwise MAILBOX if the
     * driver advertises it (with FIFO as the fallback).
     *
     * Acquire and present operate against caller-provided binary semaphores: acquireNextImage()
     * signals the supplied semaphore when the next image is ready, and present() waits on the
     * supplied semaphore before queuing the present. Both report an outOfDate flag instead of
     * throwing on VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR so the caller can recreate the
     * swapchain with the new framebuffer extent.
     *
     * @note Swapchain images are owned by the swapchain itself, not by VulkanImage; callers
     *       record barriers and copies against the raw VkImage handles returned by image().
     */
    class VulkanSwapchain {
    public:
        VulkanSwapchain() = delete;
        explicit VulkanSwapchain(const VulkanSwapchainCreateInfo &info);
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain &) = delete;
        VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;
        VulkanSwapchain(VulkanSwapchain &&) = delete;
        VulkanSwapchain &operator=(VulkanSwapchain &&) = delete;

        VulkanSwapchainAcquire acquireNextImage(const VulkanSemaphoreHandle &signal);
        bool present(VulkanQueue<queue_kind::Graphics> &queue, const VulkanSemaphoreHandle &wait, uint32_t imageIndex);
        void recreate(uint32_t width, uint32_t height);

        uint32_t imageCount() const { return static_cast<uint32_t>(images_.size()); }
        VkImage image(uint32_t index) const { return images_.at(index); }
        VkImageView imageView(uint32_t index) const { return imageViews_.at(index); }
        ImageFormat format() const;
        VkFormat vkFormat() const { return surfaceFormat_.format; }
        std::pair<uint32_t, uint32_t> extent() const { return { extent_.width, extent_.height }; }

    private:
        void createSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain);
        void destroyImageViews() noexcept;
        void destroy() noexcept;

        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        uint32_t graphicsQueueFamily_ = 0;
        bool vsync_ = true;

        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage> images_;
        std::vector<VkImageView> imageViews_;
        VkSurfaceFormatKHR surfaceFormat_{};
        VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
        VkExtent2D extent_{};
    };
}
