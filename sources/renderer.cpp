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
    autoClear(true) {
    
    uint32_t maxFramesInFlight = this->swapchainManager->getMaxFramesInFlight();
    this->descriptorManager = std::make_unique<VulkanDescriptorManager>(context, maxFramesInFlight);
    this->uniformBufferManager = std::make_unique<UniformBufferManager>(context, maxFramesInFlight);
    
    this->renderTargets.resize(maxFramesInFlight);
    this->commandBufferRecorders.resize(maxFramesInFlight);
    
    for (uint32_t i = 0; i < maxFramesInFlight; i++) {
        this->renderTargets[i] = std::make_unique<VulkanRenderTarget>(
            context,
            context->swapchainExtent.width,
            context->swapchainExtent.height
        );
        this->commandBufferRecorders[i] = std::make_unique<CommandBufferRecorder>(context, this->renderTargets[i].get());
    }
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(this->context->device);
    this->pipelineCache.clear();
    for (auto& renderTarget : this->renderTargets) {
        if (renderTarget) {
            renderTarget->dispose();
        }
    }
}

void Renderer::renderFrame() {
    this->swapchainManager->waitForCurrentFrame();
    
    uint32_t currentFrame = this->swapchainManager->getCurrentFrameIndex();
    this->descriptorManager->resetDescriptorPoolForFrame(currentFrame);
    
    uint32_t imageIndex = this->swapchainManager->acquireNextImage();
    VkCommandBuffer commandBuffer = this->swapchainManager->getCurrentCommandBuffer();
    
    VulkanRenderTarget* currentRenderTarget = this->renderTargets[currentFrame].get();
    CommandBufferRecorder* currentRecorder = this->commandBufferRecorders[currentFrame].get();
    
    currentRecorder->recordFrame(commandBuffer, imageIndex, [this, currentRenderTarget, currentFrame](VkCommandBuffer cmdBuffer) {
        VkImageLayout currentLayout = currentRenderTarget->getCurrentLayout();
        
        if (currentLayout != VK_IMAGE_LAYOUT_GENERAL) {
            currentRenderTarget->transitionLayout(
                cmdBuffer,
                currentLayout,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT
            );
        }
        
        if (this->autoClear) {
            currentRenderTarget->clearImage(cmdBuffer);
        }
        
        currentRenderTarget->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_WRITE_BIT
        );
        
        this->renderScene(cmdBuffer, currentRenderTarget, currentFrame);
    });
    this->swapchainManager->submitAndPresent(commandBuffer, imageIndex);
    this->swapchainManager->advanceFrame();
}

void Renderer::renderScene(VkCommandBuffer commandBuffer, VulkanRenderTarget* renderTarget, uint32_t currentFrame) {
    bool firstMaterial = true;
    this->scene->traverse([this, commandBuffer, renderTarget, &firstMaterial, currentFrame](const Node3DRef &node) {
        const auto *renderable = dynamic_cast<const RenderableNode3D *>(node.get());
        if (renderable != nullptr && renderable->getMaterial() != nullptr) {
            if (!firstMaterial) {
                VkMemoryBarrier memoryBarrier{};
                memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    0,
                    1, &memoryBarrier,
                    0, nullptr,
                    0, nullptr
                );
            }
            firstMaterial = false;
            
            Material *material = renderable->getMaterial();
            VulkanPipeline *pipeline = this->getPipeline(material);
            this->renderMaterial(commandBuffer, pipeline, material, renderable, renderTarget, currentFrame);
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

        VkDescriptorSetLayout setLayout = pipelinePtr->getDescriptorSetLayout(0);
        this->uniformBufferManager->registerLayout(setLayout, pipelinePtr->getBindingsBySetNumber());

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
    const RenderableNode3D *renderable,
    VulkanRenderTarget* renderTarget,
    uint32_t currentFrame
) {
    VkDescriptorSetLayout setLayout = pipeline->getDescriptorSetLayout(0);

    VkDescriptorSet descriptorSet = this->descriptorManager->createDescriptorSet(currentFrame, setLayout);

    UniformBufferManager::BufferSet* bufferSet = this->uniformBufferManager->getBufferSet(renderable, currentFrame, setLayout);

    auto setUniform = [this, bufferSet](uint32_t binding, const void *data, size_t size) {
                          this->uniformBufferManager->updateBuffer(bufferSet, binding, data, size);
                      };
    material->onRenderSetup(setUniform, this->camera, static_cast<const void *>(renderable));

    std::map<uint32_t, VkBuffer> externalBuffers;
    std::map<uint32_t, size_t> bufferSizes;
    const auto& bindings = pipeline->getBindingsBySetNumber();
    for (const auto& setBindings : bindings) {
        for (const auto& binding : setBindings.second) {
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                VkBuffer buffer = this->uniformBufferManager->getBuffer(bufferSet, binding.binding);
                size_t size = this->uniformBufferManager->getBufferSize(bufferSet, binding.binding);
                externalBuffers[binding.binding] = buffer;
                bufferSizes[binding.binding] = size;
            }
        }
    }

    pipeline->updateDescriptorSetFromReflection(descriptorSet, this->descriptorManager.get(), externalBuffers, bufferSizes);

    this->descriptorManager->updateStorageImage(descriptorSet, 4, renderTarget->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    pipeline->bindWithDescriptors(commandBuffer, descriptorSet);

    auto *rayTracingPipeline = dynamic_cast<VulkanRayTracingPipeline *>(pipeline);
    if (rayTracingPipeline != nullptr) {
        rayTracingPipeline->dispatch(commandBuffer,
                                     this->context->swapchainExtent.width,
                                     this->context->swapchainExtent.height);
    }
}