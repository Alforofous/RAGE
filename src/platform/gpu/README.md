# GPU

GPU context, resource allocation, and command submission. Backend-agnostic concepts with Vulkan implementation.

Single-threaded contract. All types in this folder assume exclusive use from a single thread. Concurrent access to any pool or queue is undefined behavior. Per-thread pools are the path to multi-threaded recording.

## Overview

```
VulkanContext                                           (value aggregate, non-movable, no custom dtor)
  ├─ VulkanInstance    — VkInstance + VkDebugUtilsMessengerEXT (destroyed together)
  ├─ VkPhysicalDevice  — trivial handle
  └─ VulkanDevice
        ├─ VulkanLogicalDevice   — RAII VkDevice (first member, partial-construction safety)
        ├─ VulkanSemaphorePool   — device-wide; refcounted handles
        ├─ VulkanFencePool       — device-wide; move-only handles (internal to submit)
        └─ VulkanQueue<Graphics> — one per queue family (MVP: graphics only)

VulkanAllocator (taken VulkanContext&)
  ├─ createBuffer()             → VulkanBuffer
  ├─ createAddressableBuffer()  → VulkanAddressableBuffer   (has deviceAddress())
  └─ createImage()              → VulkanImage  (+ default VulkanImageView)

VulkanCommandPool<K>      (user-instantiated, non-movable, owns Slot storage + free list + generation)
  └─ allocate()  → VulkanCommandBuffer<K>  (Initial ref)
                     └─ begin() &&   → VulkanRecorder<K>       (Recording)
                                         └─ end() &&   → VulkanExecutable<K>    (Executable)
                                                           └─ consumed by
                                                              VulkanQueue<K>::submit(exe, info)
                                                              ↓
                                                         VulkanPendingSubmission<K>
                                                           └─ wait() && → VulkanCommandBuffer<K>
                                                                          (Initial, reusable)

Concepts:
  GpuContext, GpuAllocator, GpuBuffer, GpuAddressableBuffer, GpuImage, GpuImageView
  GpuQueue, GpuCommandPool, GpuCommandBuffer, GpuRecorder,
  GpuExecutable, GpuPendingSubmission
  GpuFenceHandle, GpuFencePool, GpuSemaphorePool
  QueueKind, Supports<Have, Need>, MoveOnly

Queue kinds:   queue_kind::Graphics | Compute | Transfer
Capabilities:  provides = bitwise-OR of VkQueueFlags. Graphics/Compute both include Transfer.
```

## Ownership model

The ownership graph is expressed in declaration order; C++ destroys members in reverse. No `destroy()` methods; no `std::optional` workaround for deferred init.

- **`VulkanContext`** is a non-movable value aggregate. Its members construct in initializer-list order, guaranteeing partial-construction safety: if `VulkanDevice`'s constructor throws, already-built `VulkanInstance` runs its destructor cleanly.
- **`VulkanDevice`** owns the logical `VkDevice` and every device-scoped resource whose destruction requires the device alive: `VulkanSemaphorePool`, `VulkanFencePool`, `VulkanQueue<K>`. Member order places pools after the device so they destruct first.
- **`VulkanCommandPool<K>`** is user-instantiated. Non-movable. Holds raw pointer back — the user must not outlive the pool with any `CommandBuffer` / `Recorder` / `Executable` / `PendingSubmission` ref.
- **`VulkanAllocator`** is user-instantiated, non-movable. Caches raw `VkDevice` and `VmaAllocator` handles. **Lifetime contract: the allocator and every `VulkanBuffer`/`VulkanAddressableBuffer`/`VulkanImage` it produced must be destroyed before the `VulkanContext` they depend on.** Violating this calls `vmaDestroyBuffer`/`vmaDestroyAllocator` on a dead device. Typical pattern: allocator is a local variable declared after the context.

## Type-state command chain

Each transition consumes the prior object by rvalue — **invalid transitions are compile errors**, not runtime asserts.

```
VulkanCommandBuffer<K>  ─move.begin()──▶  VulkanRecorder<K>
                                             │  move.end()
                                             ▼
                                          VulkanExecutable<K>
                                             │  queue.submit(move(exe), info)
                                             ▼
                                          VulkanPendingSubmission<K>  (owns fence + retained semaphores)
                                             │  move.wait()  (or destructor auto-drains)
                                             ▼
                                          VulkanCommandBuffer<K>  (Initial again, reusable)
```

`VulkanPendingSubmission`'s destructor calls `drainAndReset()` — waits fence, resets command buffer slot, decrements pool's outstanding counter. Dropping a submission without `wait()` is safe: retained semaphores decrement refcount on vector destruction; fence returns to pool on handle destruction; GPU is fully drained before any of that runs.

## Sync primitives

- **`VulkanFencePool`** — device-owned. `VulkanFenceHandle` is move-only (fences are single-owner per submission). `submit()` acquires a handle internally; `PendingSubmission` holds it; on drain the handle returns to the pool and the fence is reset. **User never constructs a fence.**
- **`VulkanSemaphorePool`** — device-owned. `VulkanSemaphoreHandle` is copyable with refcount semantics. The user acquires a handle, passes copies by value into `SubmitInfo::wait` / `SubmitInfo::signal`; each copy bumps refcount; `PendingSubmission` retains its copies in a `std::vector`; when the last submission referencing the slot drains, refcount hits 0 and the slot returns to free list.

The UAF pattern where a user-held fence or semaphore outlives its submission is **impossible by construction** — neither primitive can be created outside its pool.

## Command pool reuse

`VulkanCommandPool<K>` uses a free-index vector populated by `reset()`:

- `allocate()` pops from `free_` first; only calls `vkAllocateCommandBuffers` if empty.
- `reset()` bumps `generation_` (invalidates outstanding refs of every state — `CommandBuffer`, `Recorder`, `Executable`, `PendingSubmission`), then re-populates `free_` with every slot index.
- `reset()` and destructor: if `outstandingSubmissions_ > 0`, drain via `vkDeviceWaitIdle` + log warning. Counter is zeroed in `reset()`; the stale `PendingSubmission`s detect the generation bump on their own `drainAndReset()` and become no-ops.
- **Caller contract:** `reset()` invalidates **all** live refs, not just submitted ones. Holding a live `Recorder`/`Executable` across a `reset()` call means that ref is effectively dead — its next use asserts on generation mismatch. Drop or `wait()` all refs before calling `reset()`.

No VkCommandBuffer handle leak across reset cycles.

## Queue-kind discrimination

```cpp
namespace queue_kind {
    struct Graphics { static constexpr VkQueueFlags provides = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT; };
    struct Compute  { static constexpr VkQueueFlags provides = VK_QUEUE_COMPUTE_BIT  | VK_QUEUE_TRANSFER_BIT; };
    struct Transfer { static constexpr VkQueueFlags provides = VK_QUEUE_TRANSFER_BIT; };
}

template <typename Have, typename Need>
concept Supports = QueueKind<Have> && QueueKind<Need>
                   && ((Have::provides & Need::provides) == Need::provides);
```

Function signature for "any queue capable of X":

```cpp
template <typename Q> requires Supports<typename Q::Kind, queue_kind::Graphics>
void render(const Q &queue, ...);
```

## Addressable buffers

`VulkanAddressableBuffer` is a distinct type from `VulkanBuffer`. Device-address availability is a **static** property — encoded in the type, not a runtime flag.

```cpp
auto uniform   = allocator.createBuffer({.size = 256, .usage = BufferUsage::Uniform});
// uniform.deviceAddress();  // compile error — VulkanBuffer has no such method

auto storage   = allocator.createAddressableBuffer({.size = 1024, .usage = BufferUsage::Storage});
uint64_t addr  = storage.deviceAddress();   // always valid; type guarantees it
```

`BufferUsage::DeviceAddress` is removed from the flag enum — use the right factory instead.

## API

| Type | Key Methods |
|---|---|
| GpuContext | instance(), physicalDevice(), device() |
| VulkanContext (extends) | graphicsQueue(), semaphores() |
| VulkanDevice | graphicsQueue(), semaphores(), fences() |
| GpuAllocator | createBuffer(), createAddressableBuffer(), createImage() |
| GpuBuffer | size(), usage(), mappedData() |
| GpuAddressableBuffer | GpuBuffer + deviceAddress() |
| GpuImage | view(), createView(), format(), extent() |
| GpuImageView | info() |
| GpuQueue\<K\> | submit(Executable, SubmitInfo = {}) → PendingSubmission, waitIdle() |
| GpuCommandPool\<K\> | allocate() → CommandBuffer, reset() |
| GpuCommandBuffer\<K\> | begin() && → Recorder\<K\> |
| GpuRecorder\<K\> | end() && → Executable\<K\> |
| GpuPendingSubmission\<K\> | wait() && → CommandBuffer\<K\> |
| GpuFenceHandle | wait(), isSignaled() |
| GpuFencePool | acquire() → Handle |
| VulkanSemaphoreHandle | copyable value; no user-facing ops |
| GpuSemaphorePool | acquire() → Handle |

## Usage

```cpp
VulkanContext ctx({ .appName = "MyApp", .enableValidation = true });

VulkanAllocator allocator(ctx);

auto uniform = allocator.createBuffer({
    .size = 256,
    .usage = BufferUsage::Uniform,
    .memory = MemoryLocation::CpuToGpu,
});
memcpy(uniform.mappedData(), &data, sizeof(data));

auto storage = allocator.createAddressableBuffer({
    .size = 1024,
    .usage = BufferUsage::Storage,
});
uint64_t gpuAddress = storage.deviceAddress();

auto image = allocator.createImage({
    .width = 1920, .height = 1080,
    .format = ImageFormat::RGBA8_UNORM,
    .usage = ImageUsage::Storage,
});

// Command pool for the graphics queue family
VulkanCommandPool<queue_kind::Graphics> pool(ctx.device().handle(), ctx.graphicsQueue().queueFamily());

// Simple submit
auto exe     = std::move(pool.allocate()).begin().end();
auto pending = ctx.graphicsQueue().submit(std::move(exe));
auto cmd     = std::move(pending).wait();

// Submit with cross-queue sync via semaphore
auto uploadDone = ctx.semaphores().acquire();
VulkanSemaphoreWait waits[]     = { { .semaphore = uploadDone,
                                      .stage     = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT } };
VulkanSemaphoreHandle signals[] = { uploadDone };

auto drawExe     = std::move(pool.allocate()).begin().end();
auto drawPending = ctx.graphicsQueue().submit(std::move(drawExe), { .wait = waits });
```

## Files

| File | Description |
|---|---|
| gpu_context.hpp | GpuContext concept |
| gpu_types.hpp | Enums, create-info structs, flag operators |
| gpu_concepts.hpp | GpuBuffer, GpuAddressableBuffer, GpuImage, GpuImageView, GpuAllocator |
| gpu_queue.hpp | MoveOnly, command-family concepts, GpuQueue |
| gpu_queue_kind.hpp | queue_kind tags, QueueKind + Supports concepts |
| gpu_command_pool.hpp | GpuCommandPool concept |
| gpu_fence.hpp | GpuFenceHandle + GpuFencePool concepts |
| gpu_semaphore_pool.hpp | GpuSemaphorePool concept |
| vulkan/vulkan_instance.hpp/.cpp | RAII VkInstance + debug messenger |
| vulkan/vulkan_device.hpp/.cpp | RAII VkDevice, owns queue + semaphore pool + fence pool |
| vulkan/vulkan_context.hpp/.cpp | Thin aggregate {Instance, PhysicalDevice, Device} |
| vulkan/vulkan_allocator.hpp/.cpp | VMA-backed allocator; takes VulkanContext& |
| vulkan/vulkan_buffer.hpp | VulkanBuffer + VulkanAddressableBuffer RAII |
| vulkan/vulkan_image.hpp/.cpp | VulkanImage + VulkanImageView RAII |
| vulkan/vulkan_queue.hpp | VulkanQueue\<K\> + VulkanSubmitInfo + VulkanSemaphoreWait |
| vulkan/vulkan_command_pool.hpp | VulkanCommandPool\<K\> + type-state chain impls |
| vulkan/vulkan_command_buffer.hpp | VulkanCommandBuffer/Recorder/Executable/PendingSubmission templates |
| vulkan/vulkan_fence_pool.hpp/.cpp | VulkanFencePool + VulkanFenceHandle (move-only RAII) |
| vulkan/vulkan_semaphore_pool.hpp/.cpp | VulkanSemaphorePool + VulkanSemaphoreHandle (refcount RAII) |
| vulkan/vulkan_type_map.hpp | RAGE enum to Vulkan type mapping |

Backend implementation lives under `gpu/vulkan/`. Future backends (Metal, D3D12) get sibling subfolders (`gpu/metal/`, `gpu/d3d12/`).

Expandable — new components added to this folder get documented here.
