#include "renderer.hpp"
#include "renderable_node3D.hpp"
#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "voxel3D.hpp"
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
    , renderTarget(std::make_unique<VulkanRenderTarget>(context,
                                                        context->swapchainExtent.width,
                                                        context->swapchainExtent.height)) {
    // Initialize descriptor sets
    descriptorSets = {};

    // Initialize all Vulkan resources in order
    std::cout << "Initializing uniform buffers..." << std::endl;
    initializeUniformBuffers();

    std::cout << "Initializing storage image..." << std::endl;
    initializeStorageImage();
}

Renderer::~Renderer() {
    // Wait for all operations to complete before destroying resources
    vkDeviceWaitIdle(context->device);

    // Clean up static resources from bindStorageImageDescriptorSet
    cleanupStaticResources();

    // Clean up descriptor pool first (before destroying buffers it references)
    if (descriptorSets.pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context->device, descriptorSets.pool, nullptr);
        descriptorSets.pool = VK_NULL_HANDLE;
    }

    // Clean up pipeline cache (this will dispose all cached pipelines)
    pipelineCache.clear();

    // Clean up render target
    if (renderTarget) {
        renderTarget->dispose();
    }

    // Clean up uniform buffers
    destroyBuffer(context, cameraBuffer, cameraMemory);

    // Note: VulkanSwapchainManager handles its own cleanup automatically
}

void Renderer::initializeUniformBuffers() {
    std::cout << "Creating uniform buffers..." << std::endl;

    // Create camera uniform buffer
    VkDeviceSize cameraBufferSize = sizeof(CameraProperties);
    this->cameraBuffer = createDeviceAddressBuffer(
        this->context,
        cameraBufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        this->cameraMemory
    );

    // Initialize camera buffer with identity matrices
    CameraProperties cameraData{};
    cameraData.viewInverse = glm::mat4(1.0f);
    cameraData.projInverse = glm::mat4(1.0f);
    this->copyToDeviceMemory(this->cameraMemory, &cameraData, cameraBufferSize);
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

                // Create and bind descriptor set for the storage image (Set 1, Binding 4)
                this->bindStorageImageDescriptorSet(commandBuffer, pipeline);

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

void Renderer::copyToDeviceMemory(VkDeviceMemory memory, const void *data, VkDeviceSize size) {
    void *mapped = nullptr;
    vkMapMemory(context->device, memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(context->device, memory);
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

void Renderer::usePipeline(PipelineType type) {
    switch (type) {
        case PipelineType::RayTracing:
            // Note: This method is now deprecated in favor of getPipelineForMaterial
            throw std::runtime_error("Use getPipelineForMaterial instead of usePipeline");
            break;

        case PipelineType::Raster:
            throw std::runtime_error("Raster pipeline not implemented yet");
            break;
    }
}

void Renderer::bindStorageImageDescriptorSet(VkCommandBuffer cmdBuffer, VulkanPipeline *pipeline) {
    // Check if we already have a descriptor set for this pipeline
    if (this->cachedDescriptorSet != VK_NULL_HANDLE) {
        // Update uniform buffers with current frame data
        this->updateCameraBuffer(this->cachedCameraBuffer, this->cachedCameraMemory);
        this->updateCubeBuffer(this->cachedCubeBuffer, this->cachedCubeMemory);

        // Reuse existing descriptor set
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipeline->getLayout(), 0, 1, &this->cachedDescriptorSet, 0, nullptr);

        return;
    }

    // Create descriptor pool if needed
    if (descriptorSets.pool == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSizes[2];
        // Storage image for ray tracing output
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = 10;
        // Uniform buffers for camera and cube data
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 20;  // Camera + cube buffers

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 10;

        if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &descriptorSets.pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool");
        }
    }

    // Get descriptor set layout for set 1 (where the image is bound)
    // The shader uses set=1, so we need to find the layout for set 1
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;

    // Find the correct descriptor set layout for set 1
    for (uint32_t i = 0; i < pipeline->getDescriptorSetLayoutCount(); i++) {
        // For now, assume the first layout corresponds to set 1
        // TODO: Improve this to properly map set numbers to layout indices
        setLayout = pipeline->getDescriptorSetLayout(i);
        break;
    }

    if (setLayout == VK_NULL_HANDLE) {
        throw std::runtime_error("No descriptor set layout found for storage image");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorSets.pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &setLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(context->device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    // Create uniform buffers
    this->cachedCameraBuffer = createBuffer(this->context, sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, this->cachedCameraMemory);
    this->cachedCubeBuffer = createBuffer(this->context, sizeof(CubeData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, this->cachedCubeMemory);

    // Update buffers with initial data
    this->updateCameraBuffer(this->cachedCameraBuffer, this->cachedCameraMemory);
    this->updateCubeBuffer(this->cachedCubeBuffer, this->cachedCubeMemory);

    // Prepare descriptor writes
    VkWriteDescriptorSet descriptorWrites[3];

    // Camera uniform buffer (binding 0)
    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = this->cachedCameraBuffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(CameraData);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].pNext = nullptr;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &cameraBufferInfo;
    descriptorWrites[0].pImageInfo = nullptr;
    descriptorWrites[0].pTexelBufferView = nullptr;

    // Cube uniform buffer (binding 1)
    VkDescriptorBufferInfo cubeBufferInfo{};
    cubeBufferInfo.buffer = this->cachedCubeBuffer;
    cubeBufferInfo.offset = 0;
    cubeBufferInfo.range = sizeof(CubeData);

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].pNext = nullptr;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &cubeBufferInfo;
    descriptorWrites[1].pImageInfo = nullptr;
    descriptorWrites[1].pTexelBufferView = nullptr;

    // Storage image (binding 4)
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = renderTarget->getImageView();
    imageInfo.sampler = VK_NULL_HANDLE;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].pNext = nullptr;
    descriptorWrites[2].dstSet = descriptorSet;
    descriptorWrites[2].dstBinding = 4;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = nullptr;
    descriptorWrites[2].pImageInfo = &imageInfo;
    descriptorWrites[2].pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(context->device, 3, descriptorWrites, 0, nullptr);

    // Cache the descriptor set for reuse
    this->cachedDescriptorSet = descriptorSet;

    // Bind descriptor set for ray tracing to index 0 (which corresponds to shader's set 1)
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipeline->getLayout(), 0, 1, &descriptorSet, 0, nullptr);
}

void Renderer::drawBindedMaterial() {
    renderFrame();
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

void Renderer::cleanupStaticResources() {
    // Clean up cached descriptor set resources
    if (this->cachedDescriptorSet != VK_NULL_HANDLE) {
        this->cachedDescriptorSet = VK_NULL_HANDLE;
    }

    if (this->cachedCameraBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(this->context->device, this->cachedCameraBuffer, nullptr);
        this->cachedCameraBuffer = VK_NULL_HANDLE;
    }

    if (this->cachedCameraMemory != VK_NULL_HANDLE) {
        vkFreeMemory(this->context->device, this->cachedCameraMemory, nullptr);
        this->cachedCameraMemory = VK_NULL_HANDLE;
    }

    if (this->cachedCubeBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(this->context->device, this->cachedCubeBuffer, nullptr);
        this->cachedCubeBuffer = VK_NULL_HANDLE;
    }

    if (this->cachedCubeMemory != VK_NULL_HANDLE) {
        vkFreeMemory(this->context->device, this->cachedCubeMemory, nullptr);
        this->cachedCubeMemory = VK_NULL_HANDLE;
    }
}