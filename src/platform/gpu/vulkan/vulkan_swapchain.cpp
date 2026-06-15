#include "vulkan_swapchain.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include "shared/logger.hpp"
#include "vulkan_queue.hpp"

namespace {
    VkSurfaceFormatKHR pickSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
        if (count == 0) {
            throw std::runtime_error("VulkanSwapchain: surface reports no formats");
        }

        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, formats.data());

        for (const VkSurfaceFormatKHR &f : formats) {
            const bool preferred = (f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_R8G8B8A8_SRGB) &&
                                   f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            if (preferred) {
                return f;
            }
        }

        return formats.front();
    }

    VkPresentModeKHR pickPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool vsync) {
        if (vsync) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        uint32_t count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr);
        if (count == 0) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        std::vector<VkPresentModeKHR> modes(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, modes.data());

        for (const VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D pickExtent(const VkSurfaceCapabilitiesKHR &caps, uint32_t width, uint32_t height) {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return caps.currentExtent;
        }

        VkExtent2D out{};
        out.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        out.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);

        return out;
    }

    uint32_t pickImageCount(const VkSurfaceCapabilitiesKHR &caps) {
        uint32_t count = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && count > caps.maxImageCount) {
            count = caps.maxImageCount;
        }

        return count;
    }

    void checkPresentSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t queueFamily) {
        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, surface, &supported);
        if (supported != VK_TRUE) {
            throw std::runtime_error("VulkanSwapchain: graphics queue family does not support present on this surface");
        }
    }
}

namespace RAGE {
    VulkanSwapchain::VulkanSwapchain(const VulkanSwapchainCreateInfo &info)
        : physicalDevice_(info.physicalDevice)
        , device_(info.device)
        , surface_(info.surface)
        , graphicsQueueFamily_(info.graphicsQueueFamily)
        , vsync_(info.vsync) {
        if (physicalDevice_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE || surface_ == VK_NULL_HANDLE) {
            throw std::runtime_error("VulkanSwapchainCreateInfo missing required handles");
        }

        checkPresentSupport(physicalDevice_, surface_, graphicsQueueFamily_);
        surfaceFormat_ = pickSurfaceFormat(physicalDevice_, surface_);
        presentMode_ = pickPresentMode(physicalDevice_, surface_, vsync_);

        createSwapchain(info.width, info.height, VK_NULL_HANDLE);
    }

    VulkanSwapchain::~VulkanSwapchain() {
        destroy();
    }

    void VulkanSwapchain::createSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain) {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

        extent_ = pickExtent(caps, width, height);
        const uint32_t imageCount = pickImageCount(caps);

        VkSwapchainCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface = surface_;
        ci.minImageCount = imageCount;
        ci.imageFormat = surfaceFormat_.format;
        ci.imageColorSpace = surfaceFormat_.colorSpace;
        ci.imageExtent = extent_;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform = caps.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = presentMode_;
        ci.clipped = VK_TRUE;
        ci.oldSwapchain = oldSwapchain;

        if (vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain");
        }

        uint32_t actualCount = 0;
        vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, nullptr);
        images_.resize(actualCount);
        vkGetSwapchainImagesKHR(device_, swapchain_, &actualCount, images_.data());

        imageViews_.resize(actualCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < actualCount; ++i) {
            VkImageViewCreateInfo viewCI{};
            viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCI.image = images_[i];
            viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCI.format = surfaceFormat_.format;
            viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCI.subresourceRange.levelCount = 1;
            viewCI.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device_, &viewCI, nullptr, &imageViews_[i]) != VK_SUCCESS) {
                destroyImageViews();
                vkDestroySwapchainKHR(device_, swapchain_, nullptr);
                swapchain_ = VK_NULL_HANDLE;
                throw std::runtime_error("Failed to create swapchain image view");
            }
        }
    }

    void VulkanSwapchain::destroyImageViews() noexcept {
        for (VkImageView view : imageViews_) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, view, nullptr);
            }
        }
        imageViews_.clear();
    }

    void VulkanSwapchain::destroy() noexcept {
        destroyImageViews();
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
        images_.clear();
    }

    VulkanSwapchainAcquire VulkanSwapchain::acquireNextImage(const VulkanSemaphoreHandle &signal) {
        VulkanSwapchainAcquire result{};

        const VkResult r = vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<uint64_t>::max(),
                                                 signal.vulkanHandle(), VK_NULL_HANDLE, &result.imageIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            result.outOfDate = true;

            return result;
        }
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("vkAcquireNextImageKHR failed");
        }
        if (r == VK_SUBOPTIMAL_KHR) {
            result.outOfDate = true;
        }

        return result;
    }

    bool VulkanSwapchain::present(VulkanQueue<queue_kind::Graphics> &queue, const VulkanSemaphoreHandle &wait,
                                  uint32_t imageIndex) {
        VkSemaphore waitHandle = wait.vulkanHandle();

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &waitHandle;
        pi.swapchainCount = 1;
        pi.pSwapchains = &swapchain_;
        pi.pImageIndices = &imageIndex;

        const VkResult r = vkQueuePresentKHR(queue.vkQueue(), &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
            return true;
        }
        if (r != VK_SUCCESS) {
            throw std::runtime_error("vkQueuePresentKHR failed");
        }

        return false;
    }

    void VulkanSwapchain::recreate(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            return;
        }

        vkDeviceWaitIdle(device_);

        VkSwapchainKHR old = swapchain_;
        swapchain_ = VK_NULL_HANDLE;
        destroyImageViews();
        images_.clear();

        try {
            createSwapchain(width, height, old);
        } catch (...) {
            if (old != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(device_, old, nullptr);
            }
            throw;
        }

        if (old != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, old, nullptr);
        }
    }

    ImageFormat VulkanSwapchain::format() const {
        switch (surfaceFormat_.format) {
            case VK_FORMAT_B8G8R8A8_SRGB:
                return ImageFormat::BGRA8_SRGB;
            case VK_FORMAT_R8G8B8A8_SRGB:
                return ImageFormat::RGBA8_SRGB;
            case VK_FORMAT_B8G8R8A8_UNORM:
                return ImageFormat::BGRA8_UNORM;
            case VK_FORMAT_R8G8B8A8_UNORM:
                return ImageFormat::RGBA8_UNORM;
            default:
                Core::log(Core::LogLevel::Warn, "VulkanSwapchain: surface format has no ImageFormat mapping");

                return ImageFormat::BGRA8_UNORM;
        }
    }
}
