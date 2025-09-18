#include "renderer.hpp"
#include "renderable_node3D.hpp"
#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "voxel3D.hpp"
#include "utils/buffer_utils.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <sstream>
#include <functional>

Renderer::Renderer(const VulkanContext *context, Scene *scene, Camera *camera)
    : context(context)
    , scene(scene)
    , camera(camera)
    , swapchainManager(std::make_unique<VulkanSwapchainManager>(context))
    , descriptorManager(std::make_unique<VulkanDescriptorManager>(context))
    , renderTarget(std::make_unique<VulkanRenderTarget>(context,
                                                        context->swapchainExtent.width,
                                                        context->swapchainExtent.height)) {
    // Initialize all Vulkan resources in order
    std::cout << "Initializing uniform buffers..." << std::endl;
    initializeUniformBuffers();

    std::cout << "Initializing storage image..." << std::endl;
    initializeStorageImage();
}

Renderer::~Renderer() {
    // Wait for all operations to complete before destroying resources
    vkDeviceWaitIdle(context->device);

    // Clean up cached uniform buffers
    if (cachedCameraBuffer != VK_NULL_HANDLE) {
        BufferUtils::destroyBuffer(context->device, cachedCameraBuffer, cachedCameraMemory);
    }
    if (cachedCubeBuffer != VK_NULL_HANDLE) {
        BufferUtils::destroyBuffer(context->device, cachedCubeBuffer, cachedCubeMemory);
    }

    // Clean up pipeline cache (this will dispose all cached pipelines)
    pipelineCache.clear();

    // Clean up render target
    if (renderTarget) {
        renderTarget->dispose();
    }

    // Clean up uniform buffers
    BufferUtils::destroyBuffer(context->device, cameraBuffer, cameraMemory);

    // Note: VulkanSwapchainManager and VulkanDescriptorManager handle their own cleanup automatically
}

void Renderer::initializeUniformBuffers() {
    std::cout << "Creating uniform buffers..." << std::endl;

    // Create camera uniform buffer
    VkDeviceSize cameraBufferSize = sizeof(CameraProperties);
    this->cameraBuffer = BufferUtils::createDeviceAddressBuffer(
        this->context->device,
        this->context->physicalDevice,
        cameraBufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        this->cameraMemory
    );

    // Initialize camera buffer with identity matrices
    CameraProperties cameraData{};
    cameraData.viewInverse = glm::mat4(1.0f);
    cameraData.projInverse = glm::mat4(1.0f);
    BufferUtils::copyToDeviceMemory(this->context->device, this->cameraMemory, &cameraData, cameraBufferSize);
}

void Renderer::initializeStorageImage() {
    std::cout << "Creating storage image..." << std::endl;

    // The storage image will be transitioned during the first render frame
    // No need for initial transition since we'll handle it in recordCommandBuffer
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
    // Begin command buffer recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }

    // Pipeline binding is now handled per material in the scene traversal

    // Ensure render target is in general layout for ray tracing write
    renderTarget->transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );

    // Render each renderable node with its material-specific pipeline
    this->scene->traverse([this, commandBuffer](const Node3D *node) {
        const auto *renderable = dynamic_cast<const RenderableNode3D *>(node);
        if (renderable != nullptr) {
            // Get or create pipeline for this material
            Material *material = renderable->getMaterial();
            if (material != nullptr) {
                VulkanPipeline *pipeline = this->getPipelineForMaterial(material);

                // Bind the material-specific pipeline
                pipeline->bind(commandBuffer);

                // Create and bind descriptor sets for this pipeline
                this->setupDescriptorSets(commandBuffer, pipeline);

                // Dispatch ray tracing for this material
                auto *rayTracingPipeline = dynamic_cast<VulkanRayTracingPipeline *>(pipeline);
                if (rayTracingPipeline != nullptr) {
                    rayTracingPipeline->dispatch(commandBuffer,
                                                 this->context->swapchainExtent.width,
                                                 this->context->swapchainExtent.height);
                }

                // Add memory barrier to ensure operations are complete
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
        }
    });

    // Wait for ray tracing to complete and transition storage image to transfer source
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

    // Transition render target to transfer source
    renderTarget->transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT
    );

    // Transition swapchain image to transfer destination
    VkImageMemoryBarrier swapchainBarrier{};
    swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainBarrier.image = context->swapchainImages[imageIndex];
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

    // Copy storage image to swapchain image
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstSubresource = copyRegion.srcSubresource;
    copyRegion.extent = { context->swapchainExtent.width, context->swapchainExtent.height, 1 };

    vkCmdCopyImage(
        commandBuffer,
        renderTarget->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        context->swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion
    );

    // Transition swapchain image to present layout
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

std::string Renderer::generateShaderHash(const Material *material) const {
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

VulkanPipeline *Renderer::getPipelineForMaterial(const Material *material) {
    if (material == nullptr) {
        throw std::runtime_error("Material cannot be null");
    }

    // Generate hash for this material's shaders
    std::string shaderHash = this->generateShaderHash(material);

    // Check if we already have a pipeline for these shaders
    auto it = this->pipelineCache.find(shaderHash);
    if (it != this->pipelineCache.end()) {
        std::cout << "Using cached pipeline for shader hash: " << shaderHash << std::endl;

        return it->second.get();
    }

    // Create new pipeline for this material
    std::cout << "Creating new pipeline for shader hash: " << shaderHash << std::endl;

    // Convert material shaders to vector for pipeline creation
    std::vector<GLSLShader> shaders;
    for (const auto &shaderPair : material->getAllShaders()) {
        shaders.push_back(shaderPair.second);
    }

    // Create ray tracing pipeline (assuming all materials are ray tracing for now)
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

void Renderer::setupDescriptorSets(VkCommandBuffer cmdBuffer, VulkanPipeline *pipeline) {
    // Create uniform buffers if they don't exist
    if (this->cachedCameraBuffer == VK_NULL_HANDLE) {
        this->cachedCameraBuffer = BufferUtils::createBuffer(
            this->context->device,
            this->context->physicalDevice,
            sizeof(CameraData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            this->cachedCameraMemory
        );
    }

    if (this->cachedCubeBuffer == VK_NULL_HANDLE) {
        this->cachedCubeBuffer = BufferUtils::createBuffer(
            this->context->device,
            this->context->physicalDevice,
            sizeof(CubeData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            this->cachedCubeMemory
        );
    }

    // Update uniform buffers with current frame data
    this->updateCameraBuffer(this->cachedCameraBuffer, this->cachedCameraMemory);
    this->updateCubeBuffer(this->cachedCubeBuffer, this->cachedCubeMemory);

    // Get descriptor set layout (assume first layout for now)
    VkDescriptorSetLayout setLayout = pipeline->getDescriptorSetLayout(0);

    // Create descriptor set using VulkanDescriptorManager
    VkDescriptorSet descriptorSet = this->descriptorManager->createDescriptorSet(setLayout);

    // Update descriptor set with our resources
    this->descriptorManager->updateUniformBuffer(descriptorSet, 0, this->cachedCameraBuffer, sizeof(CameraData));
    this->descriptorManager->updateUniformBuffer(descriptorSet, 1, this->cachedCubeBuffer, sizeof(CubeData));
    this->descriptorManager->updateStorageImage(descriptorSet, 4, this->renderTarget->getImageView(), VK_IMAGE_LAYOUT_GENERAL);

    // Bind descriptor set
    this->descriptorManager->bindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->getLayout(), 0, descriptorSet);
}

void Renderer::updateCameraBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    CameraData cameraData{};

    // Get camera matrices
    Matrix4 view = this->camera->getView();
    Matrix4 proj = this->camera->getProjection();

    // For now, just use the matrices as-is (we'll implement inverse later)
    // TODO: Implement proper matrix inverse
    cameraData.viewInverse = view;
    cameraData.projInverse = proj;
    cameraData.cameraPos = this->camera->getPosition();

    void *data;
    vkMapMemory(this->context->device, memory, 0, sizeof(CameraData), 0, &data);
    memcpy(data, &cameraData, sizeof(CameraData));
    vkUnmapMemory(this->context->device, memory);
}

void Renderer::updateCubeBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    CubeData cubeData{};

    // Get voxel data from scene (assuming first renderable node is our voxel)
    if (!this->scene->getChildren().empty()) {
        auto *voxel = dynamic_cast<Voxel3D *>(this->scene->getChildren()[0]);
        if (voxel) {
            // Get voxel properties and convert IntVector3 to Vector3
            IntVector3 intPos = voxel->getPosition();
            IntVector3 intSize = voxel->getSize();
            IntVector3 intColor = voxel->getColor();

            cubeData.position = Vector3(static_cast<float>(intPos.getX()), static_cast<float>(intPos.getY()), static_cast<float>(intPos.getZ()));
            cubeData.size = Vector3(static_cast<float>(intSize.getX()), static_cast<float>(intSize.getY()), static_cast<float>(intSize.getZ()));
            cubeData.color = Vector3(static_cast<float>(intColor.getX()) / 255.0f,
                                     static_cast<float>(intColor.getY()) / 255.0f,
                                     static_cast<float>(intColor.getZ()) / 255.0f);
        }
    }

    void *data;
    vkMapMemory(this->context->device, memory, 0, sizeof(CubeData), 0, &data);
    memcpy(data, &cubeData, sizeof(CubeData));
    vkUnmapMemory(this->context->device, memory);
}