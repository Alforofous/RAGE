#include "command_buffer_recorder.hpp"
#include "vulkan_render_target.hpp"
#include "vulkan_utils.hpp"
#include <stdexcept>

CommandBufferRecorder::CommandBufferRecorder(
    const VulkanContext *context,
    VulkanRenderTarget *renderTarget
)
    : context(context), renderTarget(renderTarget) {
    if (context == nullptr) {
        throw std::runtime_error("VulkanContext cannot be null");
    }
    if (renderTarget == nullptr) {
        throw std::runtime_error("VulkanRenderTarget cannot be null");
    }
}

void CommandBufferRecorder::recordFrame(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    const std::function<void(VkCommandBuffer)> &renderSceneCallback
) {
    CommandBufferRecorder::beginRecording(commandBuffer);
    this->prepareRenderTarget(commandBuffer);
    renderSceneCallback(commandBuffer);
    this->finalizeRendering(commandBuffer, imageIndex);
    CommandBufferRecorder::endRecording(commandBuffer);
}

void CommandBufferRecorder::beginRecording(VkCommandBuffer commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }
}

void CommandBufferRecorder::endRecording(VkCommandBuffer commandBuffer) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

void CommandBufferRecorder::prepareRenderTarget(VkCommandBuffer commandBuffer) {
}

void CommandBufferRecorder::finalizeRendering(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    this->transitionRenderTargetForCopy(commandBuffer);
    this->transitionSwapchainForCopy(commandBuffer, imageIndex);
    this->copyRenderTargetToSwapchain(commandBuffer, imageIndex);
    this->transitionSwapchainForPresentation(commandBuffer, imageIndex);
}

void CommandBufferRecorder::transitionRenderTargetForRendering(VkCommandBuffer commandBuffer) {
    this->renderTarget->transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );
}

void CommandBufferRecorder::transitionRenderTargetForCopy(VkCommandBuffer commandBuffer) {
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );

    this->renderTarget->transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT
    );
}

void CommandBufferRecorder::transitionSwapchainForCopy(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkImageMemoryBarrier swapchainBarrier{};
    swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.image = this->context->swapchainImages[imageIndex];
    swapchainBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    swapchainBarrier.srcAccessMask = 0;
    swapchainBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &swapchainBarrier
    );
}

void CommandBufferRecorder::copyRenderTargetToSwapchain(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstSubresource = copyRegion.srcSubresource;
    copyRegion.extent = { this->context->swapchainExtent.width, this->context->swapchainExtent.height, 1 };

    vkCmdCopyImage(
        commandBuffer,
        this->renderTarget->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        this->context->swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion
    );
}

void CommandBufferRecorder::transitionSwapchainForPresentation(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkImageMemoryBarrier swapchainBarrier{};
    swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapchainBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.image = this->context->swapchainImages[imageIndex];
    swapchainBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    swapchainBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &swapchainBarrier
    );
}