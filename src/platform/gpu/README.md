# GPU

GPU resource allocation and management. Backend-agnostic concepts with Vulkan/VMA implementation.

## Overview

```
┌─────────────────────┐
│   VulkanAllocator   ├───createImage────┐
└──────────┬──────────┘                  │
     createBuffer                        │
           │                             │
           ▼                             ▼
┌─────────────────────┐       ┌─────────────────┐
│    VulkanBuffer     │       │   VulkanImage   │
└─────────────────────┘       └────────┬────────┘
                                  createView
                                       │
                                       ▼
                              ┌──────────────────────┐
                              │   VulkanImageView    │
                              └──────────────────────┘

Concepts: GpuAllocator, GpuBuffer, GpuImage, GpuImageView
Types: BufferUsage, ImageUsage, ImageFormat, MemoryLocation
```

## Components

- **GpuAllocator** — creates and manages GPU buffers and images
- **GpuBuffer** — RAII handle to allocated GPU memory
- **GpuImage** — RAII handle to a GPU image with auto-created default view
- **GpuImageView** — standalone view into an image's subresource range

## API

| Type | Key Methods |
|---|---|
| GpuAllocator | createBuffer(), createImage() |
| GpuBuffer | size(), usage(), mappedData(), deviceAddress() |
| GpuImage | view(), createView(), format(), extent() |
| GpuImageView | info() |

## Usage

```cpp
VulkanAllocator allocator({
    .instance = instance,
    .physicalDevice = physicalDevice,
    .device = device
});

auto buffer = allocator.createBuffer({
    .size = 1024,
    .usage = BufferUsage::Uniform,
    .memory = MemoryLocation::CpuToGpu
});
memcpy(buffer.mappedData(), &data, sizeof(data));

auto image = allocator.createImage({
    .width = 1920, .height = 1080,
    .format = ImageFormat::RGBA8_UNORM,
    .usage = ImageUsage::Storage
});
auto& defaultView = image.view();
```

## Files

| File | Description |
|---|---|
| gpu_types.hpp | Enums, create-info structs, flag operators |
| gpu_concepts.hpp | GpuAllocator, GpuBuffer, GpuImage, GpuImageView concepts |
| vulkan_allocator.hpp/.cpp | VMA-backed allocator implementation |
| vulkan_buffer.hpp | VulkanBuffer RAII handle |
| vulkan_image.hpp/.cpp | VulkanImage + VulkanImageView RAII handles |
| vulkan_type_map.hpp | RAGE enum to Vulkan type mapping |
