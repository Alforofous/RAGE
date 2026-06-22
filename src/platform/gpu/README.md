# GPU

GPU context, resource allocation, command submission, and pipeline framework. Backend-agnostic concepts with a Vulkan implementation under `vulkan/`.

**Single-threaded contract.** All types in this folder assume exclusive use from a single thread. Concurrent access to any pool or queue is undefined behavior. Per-thread pools are the path to multi-threaded recording.

## Layers in this folder

```
Layer 2 — Pipeline framework   Shader modules, descriptor cache + pool + writer,
                               Pipeline base + ComputePipeline, RenderTarget
Layer 1 — Vulkan wrappers      Instance, device, queue, command chain, swapchain,
                               buffer, image, allocator, fence pool, semaphore pool
```

Layer 2 builds on Layer 1. Higher layers (scene graph, renderer) live elsewhere.

## Layer 1 — Vulkan wrappers

### Lifetime graph

```
VulkanContext                                           (value aggregate, non-movable)
  ├─ VulkanInstance      — VkInstance + VkDebugUtilsMessengerEXT
  ├─ VkPhysicalDevice    — trivial handle
  └─ VulkanDevice
        ├─ VulkanLogicalDevice   — RAII VkDevice (first member, partial-construction safety)
        ├─ VulkanSemaphorePool   — device-wide; refcounted handles
        ├─ VulkanFencePool       — device-wide; move-only handles (internal to submit)
        └─ VulkanQueue<Graphics> — one per queue family (MVP: graphics only)

VulkanAllocator(instance, physicalDevice, device)        (user-instantiated, non-movable)
  ├─ createBuffer(BufferCreateInfo)  → VulkanBuffer
  └─ createImage(ImageCreateInfo)    → VulkanImage   (no default view; call createView())

VulkanSwapchain(physicalDevice, device, surface, queueFamily, w, h, vsync)
  ├─ acquireNextImage(semaphore)  → {imageIndex, outOfDate}
  ├─ present(queue, semaphore, imageIndex) → outOfDate
  └─ recreate(w, h)

VulkanCommandPool<K>                  (user-instantiated, non-movable)
  └─ allocate()  → VulkanCommandBuffer<K>
                     └─ begin() &&   → VulkanRecorder<K>
                                         └─ end() &&   → VulkanExecutable<K>
                                                           └─ Queue::submit(move(exe), info)
                                                              ↓
                                                         VulkanPendingSubmission<K>
                                                           └─ wait() && → VulkanCommandBuffer<K>
                                                                          (reusable)
```

### Ownership rules

Ownership is expressed through declaration order; C++ destroys members in reverse. No `destroy()` methods; no `std::optional` workaround for deferred init.

- **`VulkanContext`** — non-movable. Members construct in initializer-list order; if `VulkanDevice` throws, already-built `VulkanInstance` still runs its destructor cleanly.
- **`VulkanDevice`** — owns the `VkDevice` plus every device-scoped pool (`Semaphore`, `Fence`) plus `VulkanQueue<K>`. Member order places pools after the logical device so they destruct first.
- **`VulkanCommandPool<K>`** — user-instantiated, non-movable. Holds raw pointer back from command-chain states.
- **`VulkanAllocator`** — user-instantiated, non-movable. **Lifetime contract**: the allocator and every `VulkanBuffer`/`VulkanImage` it produced must be destroyed before the context. Typical pattern: allocator is a local declared after the context.
- **`VulkanSwapchain`** — user-instantiated, non-movable. Borrows a `VkSurfaceKHR` it does not own (engine never creates surfaces; consumer does). Surface must outlive the swapchain.

### Type-state command chain

Each transition consumes the prior object by rvalue — **invalid transitions are compile errors**, not runtime asserts.

```
VulkanCommandBuffer<K>  ─move.begin()──▶  VulkanRecorder<K>
                                             │  move.end()
                                             ▼
                                          VulkanExecutable<K>
                                             │  queue.submit(move(exe), info)
                                             ▼
                                          VulkanPendingSubmission<K>   (owns fence + retained semaphores)
                                             │  move.wait()  (or dtor auto-drains)
                                             ▼
                                          VulkanCommandBuffer<K>  (Initial again, reusable)
```

`VulkanPendingSubmission`'s destructor calls `drainAndReset()` — waits fence, resets command-buffer slot, decrements pool's outstanding counter. Dropping without `wait()` is safe.

### Recording API (on VulkanRecorder<K>)

The compute-path command set lives on the recorder:

- `bindPipeline(PipelineBindPoint, VkPipeline)`
- `bindDescriptorSets(bindPoint, layout, firstSet, span<VkDescriptorSet>)`
- `pushConstants(layout, stages, offset, span<byte>)`
- `dispatch(x, y, z)` — gated to queues that satisfy `Supports<K, Compute>` at compile time
- `pipelineBarrier(span<VulkanImageBarrier>)`
- `copyBuffer(src, dst, span<BufferCopy>)`
- `copyBufferToImage(src, dst, layout, span<BufferImageCopy>)`
- `clearColorImage(image, layout, r, g, b, a)`
- `blitImage(src, srcLayout, srcW, srcH, dst, dstLayout, dstW, dstH)` — linear filter, color aspect

No raster commands. Adding rasterization at the engine layer is intentionally out of scope.

### Sync primitives

- **`VulkanFencePool`** — device-owned. `VulkanFenceHandle` is move-only. `submit()` acquires a handle internally; `PendingSubmission` holds it; on drain the handle returns to the pool and the fence is reset. **User never constructs a fence directly.**
- **`VulkanSemaphorePool`** — device-owned. `VulkanSemaphoreHandle` is copyable with refcount semantics. Callers `acquire()` a handle, pass copies into `SubmitInfo::wait` / `SubmitInfo::signal`; each copy bumps refcount; `PendingSubmission` retains its copies; when the last reference drops, slot returns to free list.

**Present-side semaphores are not pool-reusable across frames** — `vkQueuePresentKHR` has no fence, so the only guarantee that a present is done is the next `vkAcquireNextImageKHR` returning the same image. Hold one renderDone semaphore per swapchain image at the app layer; the pool's free-list logic is fine for `imageReady`-style semaphores covered by a submit fence.

### Queue-kind discrimination

```cpp
namespace queue_kind {
    struct Graphics { static constexpr uint32_t provides = caps::Graphics | caps::Compute | caps::Transfer; };
    struct Compute  { static constexpr uint32_t provides = caps::Compute  | caps::Transfer; };
    struct Transfer { static constexpr uint32_t provides = caps::Transfer; };
}

template <typename Have, typename Need>
concept Supports = QueueKind<Have> && QueueKind<Need>
                   && ((Have::provides & Need::provides) == Need::provides);
```

Graphics is treated as a superset of Compute to match desktop Vulkan reality (a graphics queue family almost always advertises `VK_QUEUE_COMPUTE_BIT`). This lets `pipeline.execute()` run compute dispatches on the graphics/present queue without a separate compute queue.

Function signatures use `Supports<Have, Need>` to express "any queue capable of X."

### Buffers and addressing

`VulkanBuffer` is the only buffer class. Device-address availability is a **capability flag** on the usage enum, not a separate type:

```cpp
auto storage = allocator.createBuffer({
    .size = 1024,
    .usage = BufferUsage::Storage | BufferUsage::ShaderDeviceAddress,
});
uint64_t addr = storage.deviceAddress();   // non-zero only if ShaderDeviceAddress was set
```

If `ShaderDeviceAddress` wasn't in the usage flags, `deviceAddress()` returns 0.

### Images and views

`VulkanImage` does **not** own a default view. Callers must call `createView({...})` explicitly to get a `VulkanImageView`. This avoids the case where users want multiple views of the same image (one mip, one cube face, one array slice).

`ImageViewType::Auto` deduces the view type from the image's shape (1D / 2D / 3D / array variants).

## Layer 2 — Pipeline framework

### Components

```
VulkanShaderModule              SPIR-V → VkShaderModule + reflection metadata
  └─ Built via fromFile(device, path) or (device, span<uint32_t> spirv)
     bindings(), pushConstants(), localWorkgroupSize(), stage()  via SPIRV-Reflect

VulkanDescriptorSetLayoutCache  Hashes a binding list → VkDescriptorSetLayout
  └─ Bindings are sorted before hashing; equivalent layouts share one VkDescriptorSetLayout

VulkanDescriptorPool            Per-frame VkDescriptorPool
  ├─ allocate(VkDescriptorSetLayout) → VkDescriptorSet
  └─ reset()  — bulk-frees every set allocated from this pool

VulkanDescriptorWriter          Fluent batcher for vkUpdateDescriptorSets
  ├─ writeStorageImage(set, binding, view-or-renderTarget, layout)
  ├─ writeUniformBuffer(set, binding, buffer, offset, range)
  ├─ writeStorageBuffer(set, binding, buffer, offset, range)
  └─ commit()  — flushes accumulated writes; clears for reuse

VulkanPipeline                  Abstract base; owns VkPipelineLayout
  └─ Virtual execute(recorder, PipelineExecuteContext)
        PipelineExecuteContext { descriptorSets, pushConstants, groupsX, groupsY, groupsZ }

VulkanComputePipeline : VulkanPipeline
  ├─ Ctor takes (device, layoutCache, shaderModule)
  ├─ Derives descriptor-set layouts from shader bindings via layoutCache
  ├─ Stores localWorkgroupSize from shader; groupCountsFor(w, h, d) helper
  └─ execute() = bindPipeline → bindDescriptorSets → pushConstants → dispatch

VulkanRenderTarget              VulkanImage + view + tracked layout
  └─ Ctor takes (allocator, RenderTargetCreateInfo)
     Lifetime category (per-frame vs persistent) is the owner's responsibility
```

### Pipeline-layout flow

Shader reflection (via SPIRV-Reflect) extracts `ShaderDescriptorBinding` and `ShaderPushConstantRange` lists from the SPIR-V bytecode. `VulkanComputePipeline`:

1. Groups the shader's bindings by `set` index.
2. For each set, calls `layoutCache.getOrCreate(bindings)` → `VkDescriptorSetLayout` (cached: equivalent layouts return the same handle).
3. Feeds the set layouts + push-constant ranges into the `VulkanPipeline` base, which creates the `VkPipelineLayout`.
4. Creates the `VkPipeline` via `vkCreateComputePipelines`.

No hand-written pipeline-layout descriptions. The shader is the single source of truth.

### Descriptor pool model

Pool-per-frame: one `VulkanDescriptorPool` per frame-in-flight. At the start of each frame iteration that owns a pool, call `reset()` to invalidate every set allocated last time around. No individual-set frees; bulk reset is cheap.

Capacities are fixed at pool construction via `VulkanDescriptorPoolSizes`. Exceeding any capacity throws.

### Render targets

`VulkanRenderTarget` is intentionally thin — a `VulkanImage` + default view + cached extent + caller-tracked layout. The lifetime category mentioned in the architecture doc (per-frame vs persistent) is **not encoded in the type** — it's how the owner uses it:

- Per-frame: drop and recreate on resize or scene change.
- Persistent: hold across frames for accumulation (path-tracer style).

`ImageUsage::Storage` is always included; combine with `TransferSrc`, `TransferDst`, `Sampled` as needed.

## Concepts (backend-agnostic interfaces)

```
GpuContext        GpuAllocator     GpuBuffer        GpuImage          GpuImageView
GpuQueue          GpuCommandPool   GpuCommandBuffer GpuRecorder       GpuExecutable
GpuPendingSubmission              GpuFenceHandle   GpuFencePool      GpuSemaphorePool
QueueKind         Supports<Have, Need>    MoveOnly  GpuCommandPoolFor<T, K>   GpuSubmitInfo
```

Each Vulkan implementation is `static_assert`ed to satisfy its corresponding concept. Mocks under `tests/platform/gpu/mocks/` do the same — components consuming a concept can be unit-tested without a GPU.

## Files

| File | Description |
|---|---|
| gpu_context.hpp | GpuContext concept |
| gpu_types.hpp | Enums (BufferUsage, ImageUsage, ImageFormat, ImageLayout, AccessFlags, PipelineStage, …), create-info structs, flag operators |
| gpu_concepts.hpp | GpuBuffer, GpuImage, GpuImageView, GpuAllocator |
| gpu_queue.hpp | MoveOnly, command-family concepts, GpuQueue, GpuSubmitInfo |
| gpu_queue_kind.hpp | queue_kind tags (Graphics/Compute/Transfer), QueueKind + Supports concepts |
| gpu_command_pool.hpp | GpuCommandPool + GpuCommandPoolFor<T, K> concepts |
| gpu_fence.hpp | GpuFenceHandle + GpuFencePool concepts |
| gpu_semaphore_pool.hpp | GpuSemaphorePool concept |
| vulkan/vulkan_instance.hpp/.cpp | RAII VkInstance + debug messenger |
| vulkan/vulkan_device.hpp/.cpp | RAII VkDevice; owns queue + semaphore pool + fence pool |
| vulkan/vulkan_context.hpp/.cpp | Aggregate {Instance, PhysicalDevice, Device} |
| vulkan/vulkan_allocator.hpp/.cpp | VMA-backed allocator |
| vulkan/vulkan_buffer.hpp | VulkanBuffer (single class; deviceAddress() via capability bit) |
| vulkan/vulkan_image.hpp/.cpp | VulkanImage + VulkanImageView (no default view) |
| vulkan/vulkan_queue.hpp | VulkanQueue<K> + VulkanSubmitInfo + VulkanSemaphoreWait |
| vulkan/vulkan_command_pool.hpp | VulkanCommandPool<K> + out-of-line state-machine impls + recording API impls |
| vulkan/vulkan_command_buffer.hpp | CommandBuffer/Recorder/Executable/PendingSubmission templates + VulkanImageBarrier |
| vulkan/vulkan_fence_pool.hpp/.cpp | VulkanFencePool + VulkanFenceHandle (move-only) |
| vulkan/vulkan_semaphore_pool.hpp/.cpp | VulkanSemaphorePool + VulkanSemaphoreHandle (refcounted) |
| vulkan/vulkan_swapchain.hpp/.cpp | VulkanSwapchain (borrows VkSurfaceKHR) |
| vulkan/vulkan_shader_module.hpp/.cpp | SPIR-V loader + SPIRV-Reflect reflection |
| vulkan/vulkan_descriptor_layout_cache.hpp/.cpp | Caches VkDescriptorSetLayout by binding list |
| vulkan/vulkan_descriptor_pool.hpp/.cpp | Per-frame VkDescriptorPool with reset() |
| vulkan/vulkan_descriptor_writer.hpp/.cpp | Fluent batcher for vkUpdateDescriptorSets |
| vulkan/vulkan_pipeline.hpp/.cpp | Abstract pipeline base + PipelineExecuteContext |
| vulkan/vulkan_compute_pipeline.hpp/.cpp | Concrete compute pipeline |
| vulkan/vulkan_render_target.hpp/.cpp | VulkanImage + view + layout-tracking wrapper |
| vulkan/vulkan_type_map.hpp | RAGE enums ↔ Vulkan type mapping (formats, layouts, stages, access flags, …) |
| vulkan/vma_impl.cpp | VMA implementation translation unit |

Backend lives under `gpu/vulkan/`. Future backends (Metal, D3D12) get sibling subfolders.

## Usage (compute-raycaster smoke test)

End-to-end flow as wired in `src/app/main.cpp`:

```cpp
VulkanContext ctx({.appName = "MyApp", .enableValidation = true,
                   .requiredExtensions = glfwRequiredInstanceExtensions()});
VulkanAllocator allocator(ctx.vkInstance(), ctx.physicalDevice(), ctx.vkDevice());

VkSurfaceKHR surface = /* glfwCreateWindowSurface(ctx.vkInstance(), window, ...); */;

const VulkanShaderModule shader =
    VulkanShaderModule::fromFile(ctx.vkDevice(), "shaders/raycast.comp.spv");

VulkanDescriptorSetLayoutCache layoutCache(ctx.vkDevice());
VulkanComputePipeline pipeline(ctx.vkDevice(), layoutCache, shader);

VulkanSwapchain swapchain({.physicalDevice = ctx.physicalDevice(),
                           .device = ctx.vkDevice(),
                           .surface = surface,
                           .graphicsQueueFamily = ctx.graphicsQueue().queueFamily(),
                           .width = w, .height = h, .vsync = true});

VulkanCommandPool<queue_kind::Graphics> cmdPool(ctx.vkDevice(), ctx.graphicsQueue().queueFamily());
VulkanDescriptorPool descPool(ctx.vkDevice(), {});

VulkanRenderTarget rt(allocator, {.width = w, .height = h,
                                  .format = ImageFormat::RGBA8_UNORM,
                                  .usage = ImageUsage::Storage | ImageUsage::TransferSrc});

VkDescriptorSet set = descPool.allocate(pipeline.descriptorSetLayouts()[0]);
VulkanDescriptorWriter writer(ctx.vkDevice());
writer.writeStorageImage(set, 0, rt, ImageLayout::General).commit();

// Per-image renderDone semaphores held in a vector, lifetime = swapchain
std::vector<VulkanSemaphoreHandle> renderDoneByImage;

// Frame:
auto imageReady = ctx.semaphores().acquire();
auto acq = swapchain.acquireNextImage(imageReady);

auto rec = std::move(cmdPool.allocate()).begin();
rec.pipelineBarrier(/* rt: Undefined → General */);
pipeline.execute(rec, {.descriptorSets = {set},
                       .groupsX = pipeline.groupCountsFor(w, h)[0], ...});
rec.pipelineBarrier(/* rt → TransferSrc, swap → TransferDst */);
rec.blitImage(rt.image().handle(), TransferSrcOptimal, w, h,
              swapchain.image(acq.imageIndex), TransferDstOptimal, w, h);
rec.pipelineBarrier(/* swap → PresentSrc */);
auto exe = std::move(rec).end();

auto pending = ctx.graphicsQueue().submit(std::move(exe),
                  {.wait = {{imageReady, PipelineStage::Transfer}},
                   .signal = {renderDoneByImage[acq.imageIndex]}});
swapchain.present(ctx.graphicsQueue(), renderDoneByImage[acq.imageIndex], acq.imageIndex);
// Next frame: std::move(pending).wait()
```

Expandable — new components added to this folder get documented here.
