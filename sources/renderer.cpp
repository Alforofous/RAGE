#include "renderer.hpp"
#include "renderable_node3D.hpp"
#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "materials/ray_tracing_material.hpp"
#include "command_buffer_recorder.hpp"
#include <stdexcept>
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
    ),
    commandBufferRecorder(std::make_unique<CommandBufferRecorder>(context, this->renderTarget.get())
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
    this->commandBufferRecorder->recordFrame(commandBuffer, imageIndex, [this](VkCommandBuffer cmdBuffer) {
        this->renderScene(cmdBuffer);
    });
    this->swapchainManager->submitAndPresent(commandBuffer, imageIndex);
    this->swapchainManager->advanceFrame();
}

void Renderer::renderScene(VkCommandBuffer commandBuffer) {
    this->scene->traverse([this, commandBuffer](const Node3D *node) {
        const auto *renderable = dynamic_cast<const RenderableNode3D *>(node);
        if (renderable != nullptr && renderable->getMaterial() != nullptr) {
            Material *material = renderable->getMaterial();
            VulkanPipeline *pipeline = this->getPipeline(material);
            this->renderMaterial(commandBuffer, pipeline, material, renderable);
        }
    });
}

VulkanPipeline *Renderer::getPipeline(const Material *material) {
    if (material == nullptr) {
        throw std::runtime_error("Material cannot be null");
    }

    std::string shaderHash = Renderer::generateShaderHash(material);
    auto it = this->pipelineCache.find(shaderHash);
    if (it != this->pipelineCache.end()) {
        return it->second.get();
    }

    const auto *rayTracingMaterial = dynamic_cast<const RayTracingMaterial *>(material);
    if (rayTracingMaterial != nullptr) {
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

void Renderer::renderMaterial(
    VkCommandBuffer commandBuffer,
    VulkanPipeline *pipeline,
    const Material *material,
    const RenderableNode3D *renderable
) {
    VkDescriptorSetLayout setLayout = pipeline->getDescriptorSetLayout(0);
    
    struct DescriptorCacheKey {
        const Material* material;
        VkDescriptorSetLayout layout;
        VkImageView renderTargetView;
        uint32_t frameIndex;
    };
    
    DescriptorCacheKey cacheKey = {
        material,
        setLayout,
        this->renderTarget->getImageView(),
        static_cast<uint32_t>(this->swapchainManager->getCurrentFrameIndex())
    };
    
    VkDescriptorSet descriptorSet = this->descriptorManager->getOrCreateCachedDescriptorSet(
        setLayout,
        &cacheKey,
        sizeof(cacheKey)
    );

    // Setup material uniforms (high-level logic stays in renderer)
    auto setUniform = [pipeline](uint32_t binding, const void *data, size_t size) {
        pipeline->setUniform(binding, data, size);
    };
    material->onRenderSetup(setUniform, this->camera, static_cast<const void *>(renderable));

    // Use pipeline helper to automatically bind uniform buffers from reflection
    pipeline->updateDescriptorSetFromReflection(descriptorSet, this->descriptorManager.get());

    // Handle render target storage image (renderer-specific knowledge)
    this->descriptorManager->updateStorageImage(descriptorSet, 4, this->renderTarget->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    // Use pipeline helper for binding
    pipeline->bindWithDescriptors(commandBuffer, descriptorSet);

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