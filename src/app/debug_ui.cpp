#include "debug_ui.hpp"

#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "engine/rendering/renderer.hpp"

namespace RAGE::App {
    namespace {
        constexpr uint32_t kDescriptorPoolSize = 64;

        void check(VkResult r, const char *what) {
            if (r != VK_SUCCESS) {
                throw std::runtime_error(std::string("DebugUi: ") + what + " failed");
            }
        }

        void imguiVkResultCheck(VkResult r) {
            check(r, "ImGui backend call");
        }

        VkDescriptorPool createDescriptorPool(VkDevice device) {
            const VkDescriptorPoolSize poolSizes[] = {
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kDescriptorPoolSize },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          kDescriptorPoolSize },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         kDescriptorPoolSize },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         kDescriptorPoolSize },
            };
            VkDescriptorPoolCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            info.maxSets = kDescriptorPoolSize * 4;
            info.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
            info.pPoolSizes = poolSizes;

            VkDescriptorPool pool = VK_NULL_HANDLE;
            check(vkCreateDescriptorPool(device, &info, nullptr, &pool), "vkCreateDescriptorPool");
            return pool;
        }

        VkRenderPass createRenderPass(VkDevice device, VkFormat format) {
            VkAttachmentDescription color{};
            color.format = format;
            color.samples = VK_SAMPLE_COUNT_1_BIT;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference colorRef{};
            colorRef.attachment = 0;
            colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorRef;

            VkSubpassDependency dep{};
            dep.srcSubpass = VK_SUBPASS_EXTERNAL;
            dep.dstSubpass = 0;
            dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo rp{};
            rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            rp.attachmentCount = 1;
            rp.pAttachments = &color;
            rp.subpassCount = 1;
            rp.pSubpasses = &subpass;
            rp.dependencyCount = 1;
            rp.pDependencies = &dep;

            VkRenderPass renderPass = VK_NULL_HANDLE;
            check(vkCreateRenderPass(device, &rp, nullptr, &renderPass), "vkCreateRenderPass");
            return renderPass;
        }
    }

    struct DebugUi::Impl {
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkInstance instance = VK_NULL_HANDLE;
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        uint32_t graphicsQueueFamily = 0;
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;
        uint32_t imageCount = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        bool imguiInitialized = false;
        bool vulkanBackendInitialized = false;
        BuilderFn builder;
        // Latched after each ImGui::NewFrame() so external pollers can read scroll without
        // racing the renderer's NewFrame call (which happens later in the frame than the
        // controller's input read in main.cpp's loop). One-frame latency, imperceptible.
        float lastScrollDelta = 0.0f;

        ~Impl() { shutdownAll(); }

        void shutdownAll() noexcept {
            if (device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device);
            }
            destroySwapResources();
            if (vulkanBackendInitialized) {
                ImGui_ImplVulkan_Shutdown();
                vulkanBackendInitialized = false;
            }
            if (imguiInitialized) {
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
                imguiInitialized = false;
            }
            if (renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device, renderPass, nullptr);
                renderPass = VK_NULL_HANDLE;
            }
            if (descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, descriptorPool, nullptr);
                descriptorPool = VK_NULL_HANDLE;
            }
        }

        void destroySwapResources() noexcept {
            if (device == VK_NULL_HANDLE) {
                return;
            }
            for (VkFramebuffer fb : framebuffers) {
                if (fb != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device, fb, nullptr);
                }
            }
            framebuffers.clear();
            for (VkImageView v : imageViews) {
                if (v != VK_NULL_HANDLE) {
                    vkDestroyImageView(device, v, nullptr);
                }
            }
            imageViews.clear();
        }

        void rebuildSwapchainResources(const Renderer::SwapchainInfo &info) {
            if (device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(device);
            }

            device = info.device;
            physicalDevice = info.physicalDevice;
            instance = info.instance;
            graphicsQueue = info.graphicsQueue;
            graphicsQueueFamily = info.graphicsQueueFamily;
            colorFormat = info.colorFormat;
            imageCount = info.imageCount;
            width = info.width;
            height = info.height;

            if (descriptorPool == VK_NULL_HANDLE) {
                descriptorPool = createDescriptorPool(device);
            }
            if (renderPass == VK_NULL_HANDLE) {
                renderPass = createRenderPass(device, colorFormat);
            }

            destroySwapResources();
            imageViews.resize(imageCount, VK_NULL_HANDLE);
            framebuffers.resize(imageCount, VK_NULL_HANDLE);
            for (uint32_t i = 0; i < imageCount; ++i) {
                VkImageViewCreateInfo vci{};
                vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                vci.image = info.images[i];
                vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
                vci.format = colorFormat;
                vci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
                vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                check(vkCreateImageView(device, &vci, nullptr, &imageViews[i]), "vkCreateImageView");

                VkFramebufferCreateInfo fci{};
                fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fci.renderPass = renderPass;
                fci.attachmentCount = 1;
                fci.pAttachments = &imageViews[i];
                fci.width = width;
                fci.height = height;
                fci.layers = 1;
                check(vkCreateFramebuffer(device, &fci, nullptr, &framebuffers[i]), "vkCreateFramebuffer");
            }

            if (!vulkanBackendInitialized) {
                ImGui_ImplVulkan_InitInfo init{};
                init.ApiVersion = VK_API_VERSION_1_3;
                init.Instance = instance;
                init.PhysicalDevice = physicalDevice;
                init.Device = device;
                init.QueueFamily = graphicsQueueFamily;
                init.Queue = graphicsQueue;
                init.DescriptorPool = descriptorPool;
                init.MinImageCount = imageCount;
                init.ImageCount = imageCount;
                init.PipelineInfoMain.RenderPass = renderPass;
                init.PipelineInfoMain.Subpass = 0;
                init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
                init.CheckVkResultFn = &imguiVkResultCheck;
                if (!ImGui_ImplVulkan_Init(&init)) {
                    throw std::runtime_error("DebugUi: ImGui_ImplVulkan_Init failed");
                }
                vulkanBackendInitialized = true;
            } else {
                ImGui_ImplVulkan_SetMinImageCount(imageCount);
            }
        }

        void record(const Renderer::UiRenderContext &ctx) {
            if (!vulkanBackendInitialized) {
                return;
            }

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            lastScrollDelta = ImGui::GetIO().MouseWheel;

            if (builder) {
                builder();
            }

            ImGui::Render();
            ImDrawData *drawData = ImGui::GetDrawData();
            if (drawData == nullptr || drawData->TotalVtxCount == 0) {
                if (drawData == nullptr) {
                    return;
                }
            }

            VkClearValue dummyClear{};
            VkRenderPassBeginInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            bi.renderPass = renderPass;
            bi.framebuffer = framebuffers[ctx.swapImageIndex];
            bi.renderArea.offset = { 0, 0 };
            bi.renderArea.extent = { ctx.width, ctx.height };
            bi.clearValueCount = 0;
            bi.pClearValues = &dummyClear;
            vkCmdBeginRenderPass(ctx.cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
            ImGui_ImplVulkan_RenderDrawData(drawData, ctx.cmd);
            vkCmdEndRenderPass(ctx.cmd);
        }
    };

    DebugUi::DebugUi(VulkanContext & /*ctx*/, GLFWwindow *window)
        : impl_(std::make_unique<Impl>()) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
            ImGui::DestroyContext();
            throw std::runtime_error("DebugUi: ImGui_ImplGlfw_InitForVulkan failed");
        }
        impl_->imguiInitialized = true;
    }

    DebugUi::~DebugUi() = default;

    void DebugUi::attach(Renderer &renderer) {
        renderer.onSwapchainRebuilt(
            [this](const Renderer::SwapchainInfo &info) { impl_->rebuildSwapchainResources(info); });
        renderer.onUiRender([this](const Renderer::UiRenderContext &ctx) { impl_->record(ctx); });
    }

    void DebugUi::setBuilder(BuilderFn fn) { impl_->builder = std::move(fn); }

    void DebugUi::demoPanel(const char *title) {
        ImGui::Begin(title);
        ImGui::Text("RAGE debug UI active.");
        ImGui::End();
    }

    void DebugUi::beginPanel(const char *title) { ImGui::Begin(title); }

    void DebugUi::endPanel() { ImGui::End(); }

    bool DebugUi::button(const char *label) { return ImGui::Button(label); }

    bool DebugUi::checkbox(const char *label, bool *value) { return ImGui::Checkbox(label, value); }

    bool DebugUi::sliderInt(const char *label, int *value, int min, int max) {
        return ImGui::SliderInt(label, value, min, max);
    }

    bool DebugUi::sliderFloat(const char *label, float *value, float min, float max) {
        return ImGui::SliderFloat(label, value, min, max);
    }

    void DebugUi::separatorText(const char *label) { ImGui::SeparatorText(label); }

    void DebugUi::separator() { ImGui::Separator(); }

    float DebugUi::scrollDelta() const {
        return impl_->lastScrollDelta;
    }

    bool DebugUi::radio(const char *label, int *current, std::span<const char *const> options) {
        ImGui::TextUnformatted(label);
        bool changed = false;
        for (size_t i = 0; i < options.size(); ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            if (ImGui::RadioButton(options[i], current != nullptr && *current == static_cast<int>(i))) {
                if (current != nullptr) {
                    *current = static_cast<int>(i);
                }
                changed = true;
            }
        }
        return changed;
    }

    void DebugUi::text(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        ImGui::TextV(fmt, args);
        va_end(args);
    }

    void DebugUi::plot(const char *label, const float *samples, size_t bufferLength, size_t count,
                       size_t offset, const char *overlayFmt, float yMin, float yMax) {
        if (samples == nullptr || count == 0 || bufferLength == 0) {
            ImGui::Text("%s: (no data)", label);
            return;
        }
        // Latest sample = the position just before the writer's next slot.
        const size_t latestIdx = (offset + count + bufferLength - 1) % bufferLength;
        char overlay[64] = "";
        if (overlayFmt != nullptr) {
            std::snprintf(overlay, sizeof(overlay), overlayFmt, samples[latestIdx]);
        }
        ImGui::PlotLines(label, samples, static_cast<int>(count), static_cast<int>(offset),
                         overlay, yMin, yMax, ImVec2(0, 48));
    }

    bool DebugUi::wantsMouse() const {
        if (!impl_->imguiInitialized) {
            return false;
        }
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DebugUi::wantsKeyboard() const {
        if (!impl_->imguiInitialized) {
            return false;
        }
        return ImGui::GetIO().WantCaptureKeyboard;
    }
}
