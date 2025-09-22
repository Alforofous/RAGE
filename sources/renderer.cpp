#include "renderer.hpp"
#include "renderable_node3D.hpp"
#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "utils/buffer_utils.hpp"
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

    // Create ray tracing pipeline from material shaders
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

    // Let the material handle ALL its uniform setup (including camera data)
    auto setUniform = [this, descriptorSet](const std::string &name, const UniformBase &uniform) {
                          // For now, assume all uniforms go to binding 0 and 1
                          // This is a simplified approach - in a real system you'd need more sophisticated binding management
                          static uint32_t bindingIndex = 0;

                          // Create temporary buffer for this uniform
                          constexpr size_t MAX_UNIFORM_SIZE = 1024;
                          VkBuffer tempBuffer;
                          VkDeviceMemory tempMemory;

                          tempBuffer = BufferUtils::createBuffer(
                              this->context->device,
                              this->context->physicalDevice,
                              MAX_UNIFORM_SIZE,
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              tempMemory
                          );

                          // Copy uniform data to buffer
                          void *bufferData = nullptr;
                          vkMapMemory(this->context->device, tempMemory, 0, uniform.getSize(), 0, &bufferData);
                          uniform.copyTo(bufferData, uniform.getSize());
                          vkUnmapMemory(this->context->device, tempMemory);

                          // Update descriptor set - use appropriate binding based on uniform name
                          uint32_t binding = (name == "camera") ? 0 : 1;
                          this->descriptorManager->updateUniformBuffer(descriptorSet, binding, tempBuffer, uniform.getSize());

                          // Note: In production, you'd want to manage buffer lifetimes properly
                          // For now, we're accepting the memory leak for simplicity
                      };

    // Call material's setup with camera and object data
    const_cast<Material *>(material)->onRenderSetup(setUniform, this->camera,
                                                    const_cast<void *>(static_cast<const void *>(renderable)));

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