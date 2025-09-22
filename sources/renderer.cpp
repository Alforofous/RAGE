#include "renderer.hpp"
#include "renderable_node3D.hpp"
#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "materials/ray_tracing_material.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <sstream>
#include <functional>

Renderer::Renderer(const VulkanContext *context, Scene *scene, Camera *camera)
    : context(context),
    scene(scene),
    camera(camera),
    swapchainManager(std::make_unique<VulkanSwapchainManager>(context)),
    descriptorManager(std::make_unique<VulkanDescriptorManager>(context)),
    renderTarget(
        std::make_unique<VulkanRenderTarget>(
            context,
            context->swapchainExtent.width,
            context->swapchainExtent.height
        )
    ) {
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(this->context->device);
    this->pipelineCache.clear();
    if (this->renderTarget) {
        this->renderTarget->dispose();
    }
}

void Renderer::renderFrame() {
    this->swapchainManager->waitForCurrentFrame();
    uint32_t imageIndex = this->swapchainManager->acquireNextImage();
    VkCommandBuffer commandBuffer = this->swapchainManager->getCurrentCommandBuffer();
    this->recordCommandBuffer(commandBuffer, imageIndex);
    this->swapchainManager->submitAndPresent(commandBuffer, imageIndex);
    this->swapchainManager->advanceFrame();
}

void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }

    // Transition render target to general layout for ray tracing
    this->renderTarget->transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );

    // Traverse scene and render all materials
    this->scene->traverse([this, commandBuffer](const Node3D *node) {
        const auto *renderable = dynamic_cast<const RenderableNode3D *>(node);
        if (renderable != nullptr && renderable->getMaterial() != nullptr) {
            Material *material = renderable->getMaterial();
            VulkanPipeline *pipeline = this->getOrCreatePipeline(material);
            this->renderMaterial(commandBuffer, pipeline, material, renderable);
        }
    });

    // Transition render target for copy to swapchain
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

    // Transition swapchain image for copy
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

    // Copy render target to swapchain
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

    // Transition swapchain image for presentation
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

VulkanPipeline *Renderer::getOrCreatePipeline(const Material *material) {
    if (material == nullptr) {
        throw std::runtime_error("Material cannot be null");
    }

    std::string shaderHash = Renderer::generateShaderHash(material);

    auto it = this->pipelineCache.find(shaderHash);
    if (it != this->pipelineCache.end()) {
        return it->second.get();
    }

    std::cout << "Creating new pipeline for shader hash: " << shaderHash << std::endl;

    // Check material type and create appropriate pipeline
    const auto *rayTracingMaterial = dynamic_cast<const RayTracingMaterial *>(material);
    if (rayTracingMaterial != nullptr) {
        // Create ray tracing pipeline
        auto pipeline = std::make_unique<VulkanRayTracingPipeline>(
            this->context,
            material->getShader("raygen"),
            material->getShader("miss"),
            material->getShader("closestHit")
        );

        VulkanPipeline *pipelinePtr = pipeline.get();
        this->pipelineCache[shaderHash] = std::move(pipeline);
        return pipelinePtr;
    }

    // Add more pipeline types here as needed
    // else if (const ComputeMaterial *computeMaterial = dynamic_cast<const ComputeMaterial *>(material)) {
    //     // Create compute pipeline
    // }
    // else if (const GraphicsMaterial *graphicsMaterial = dynamic_cast<const GraphicsMaterial *>(material)) {
    //     // Create graphics pipeline
    // }

    throw std::runtime_error("Unsupported material type - cannot create pipeline");
}

std::string Renderer::generateShaderHash(const Material *material) {
    std::stringstream ss;
    for (const auto &shaderPair : material->getAllShaders()) {
        const std::string &name = shaderPair.first;
        const GLSLShader &shader = shaderPair.second;
        ss << name << ":" << static_cast<int>(shader.kind) << ":" << shader.source << ";";
    }

    std::hash<std::string> hasher;
    size_t hashValue = hasher(ss.str());

    return std::to_string(hashValue);
}

void Renderer::renderMaterial(VkCommandBuffer commandBuffer, VulkanPipeline *pipeline,
                              const Material *material, const RenderableNode3D *renderable) {
    // Bind the pipeline
    pipeline->bind(commandBuffer);

    // Create descriptor set for this material
    VkDescriptorSetLayout setLayout = pipeline->getDescriptorSetLayout(0);
    VkDescriptorSet descriptorSet = this->descriptorManager->createDescriptorSet(setLayout);

    // Set up render target in descriptor set
    this->descriptorManager->updateStorageImage(descriptorSet, 4, this->renderTarget->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    // Let material set uniforms directly in pipeline's pre-allocated buffers
    const_cast<Material *>(material)->onRenderSetup(pipeline, this->camera, const_cast<void *>(static_cast<const void *>(renderable)));

    // Bind all uniform buffers from pipeline to descriptor set
    // Note: We bind buffers for all possible bindings since materials have already filled them
    for (uint32_t binding = 0; binding < 8; ++binding) { // Check up to 8 bindings (common range)
        try {
            VkBuffer uniformBuffer = pipeline->getUniformBuffer(binding);
            // Use VK_WHOLE_SIZE to bind the entire buffer
            this->descriptorManager->updateUniformBuffer(descriptorSet, binding, uniformBuffer, VK_WHOLE_SIZE);
        } catch (const std::exception &) {
            // This binding doesn't exist, skip it
        }
    }

    // Bind descriptor sets
    VulkanDescriptorManager::bindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                                pipeline->getLayout(), 0, descriptorSet);

    // Dispatch ray tracing if this is a ray tracing pipeline
    auto *rayTracingPipeline = dynamic_cast<VulkanRayTracingPipeline *>(pipeline);
    if (rayTracingPipeline != nullptr) {
        rayTracingPipeline->dispatch(commandBuffer,
                                     this->context->swapchainExtent.width,
                                     this->context->swapchainExtent.height);
    }

    // Add memory barrier between materials
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
}