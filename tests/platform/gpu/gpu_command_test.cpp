#include <gtest/gtest.h>
#include <array>
#include "mocks/allocator.hpp"
#include "mocks/command_pool.hpp"
#include "mocks/queue.hpp"
#include "mocks/semaphore_pool.hpp"

using namespace RAGE;
using namespace RAGE::Mocks;

using GraphicsPool = MockCommandPool<queue_kind::Graphics>;
using GraphicsQueue = MockQueue<queue_kind::Graphics>;
using ComputePool = MockCommandPool<queue_kind::Compute>;
using TransferPool = MockCommandPool<queue_kind::Transfer>;

TEST(MockCommandPool, AllocateIncrementsCount) {
    GraphicsPool pool;
    EXPECT_EQ(pool.allocCount(), 0);
    [[maybe_unused]] auto cmd = pool.allocate();
    EXPECT_EQ(pool.allocCount(), 1);
}

TEST(MockCommandPool, ResetIncrementsCount) {
    GraphicsPool pool;
    EXPECT_EQ(pool.resetCount(), 0);
    pool.reset();
    pool.reset();
    EXPECT_EQ(pool.resetCount(), 2);
}

TEST(MockCommandBuffer, TypeStateFlow) {
    GraphicsPool pool;
    auto cmd = pool.allocate();
    auto rec = std::move(cmd).begin();
    [[maybe_unused]] auto exe = std::move(rec).end();
}

TEST(MockQueue, SubmitIncrementsCount) {
    GraphicsPool pool;
    GraphicsQueue queue;

    auto exe = std::move(pool.allocate()).begin().end();

    EXPECT_EQ(queue.submitCount(), 0);
    [[maybe_unused]] auto pending = queue.submit(std::move(exe));
    EXPECT_EQ(queue.submitCount(), 1);
}

TEST(MockQueue, SubmitIncrementsOutstandingSubmissions) {
    GraphicsPool pool;
    GraphicsQueue queue;

    auto exe = std::move(pool.allocate()).begin().end();
    EXPECT_EQ(pool.outstandingSubmissions(), 0);

    {
        [[maybe_unused]] auto pending = queue.submit(std::move(exe));
        EXPECT_EQ(pool.outstandingSubmissions(), 1);
    }

    EXPECT_EQ(pool.outstandingSubmissions(), 0);
}

TEST(MockQueue, WaitIdleIncrementsCount) {
    GraphicsQueue queue;
    EXPECT_EQ(queue.idleCount(), 0);
    queue.waitIdle();
    EXPECT_EQ(queue.idleCount(), 1);
}

TEST(MockPendingSubmission, WaitReturnsReusableCommandBuffer) {
    GraphicsPool pool;
    GraphicsQueue queue;

    auto exe = std::move(pool.allocate()).begin().end();
    auto pending = queue.submit(std::move(exe));

    auto cmdAgain = std::move(pending).wait();
    [[maybe_unused]] auto exe2 = std::move(cmdAgain).begin().end();
    EXPECT_EQ(pool.outstandingSubmissions(), 0);
}

TEST(MockPendingSubmission, WaitOnStaleAfterPoolResetThrows) {
    GraphicsPool pool;
    GraphicsQueue queue;

    auto exe = std::move(pool.allocate()).begin().end();
    auto pending = queue.submit(std::move(exe));

    pool.reset();

    EXPECT_THROW({ [[maybe_unused]] auto cb = std::move(pending).wait(); }, std::runtime_error);
    EXPECT_EQ(pool.outstandingSubmissions(), 0);
}

TEST(MockSemaphorePool, HandleLifetimeRefcounts) {
    MockSemaphorePool semPool;

    EXPECT_EQ(semPool.activeSlotCount(), 0u);

    {
        auto s = semPool.acquire();
        EXPECT_EQ(semPool.activeSlotCount(), 1u);

        auto s2 = s;
        EXPECT_EQ(semPool.activeSlotCount(), 1u);

        s2 = MockSemaphoreHandle{};
        EXPECT_EQ(semPool.activeSlotCount(), 1u);
    }

    EXPECT_EQ(semPool.activeSlotCount(), 0u);
}

TEST(MockQueue, SubmitRetainsSemaphoresAcrossDropScope) {
    GraphicsPool pool;
    GraphicsQueue queue;
    MockSemaphorePool semPool;

    auto exe = std::move(pool.allocate()).begin().end();
    std::optional<MockPendingSubmission<queue_kind::Graphics>> pending;

    {
        auto s = semPool.acquire();
        const std::array<MockSemaphoreWait, 1> waits{ { { .semaphore = s, .stage = 0 } } };
        pending.emplace(queue.submit(std::move(exe), { .wait = waits }));
        EXPECT_EQ(semPool.activeSlotCount(), 1u);
    }

    EXPECT_EQ(semPool.activeSlotCount(), 1u);

    pending.reset();
    EXPECT_EQ(semPool.activeSlotCount(), 0u);
}

template <GpuQueue Q> void submitOne(Q &queue, typename Q::Executable exe) {
    [[maybe_unused]] auto pending = queue.submit(std::move(exe));
}

TEST(GpuQueueConcept, GenericFunctionWorksWithMock) {
    GraphicsPool pool;
    GraphicsQueue queue;
    auto exe = std::move(pool.allocate()).begin().end();
    submitOne(queue, std::move(exe));
    EXPECT_EQ(queue.submitCount(), 1);
}

void supportsCompiletimeCheck() {
    static_assert(Supports<queue_kind::Graphics, queue_kind::Transfer>);
    static_assert(Supports<queue_kind::Compute, queue_kind::Transfer>);
    static_assert(Supports<queue_kind::Graphics, queue_kind::Graphics>);
    static_assert(Supports<queue_kind::Graphics, queue_kind::Compute>);
    static_assert(!Supports<queue_kind::Transfer, queue_kind::Graphics>);
    static_assert(!Supports<queue_kind::Transfer, queue_kind::Compute>);
    static_assert(!Supports<queue_kind::Compute, queue_kind::Graphics>);
}

TEST(MockRecorder, BindPipelineIncrementsCount) {
    ComputePool pool;
    auto rec = pool.allocate().begin();
    rec.bindPipeline(PipelineBindPoint::Compute, MockPipelineHandle{ .id = 42 });
    [[maybe_unused]] auto exe = std::move(rec).end();
    EXPECT_EQ(pool.bindPipelineCount(), 1u);
}

TEST(MockRecorder, BindDescriptorSetsRecordsSetCount) {
    ComputePool pool;
    auto rec = pool.allocate().begin();
    const std::array<MockDescriptorSetHandle, 3> sets{ { { 1 }, { 2 }, { 3 } } };
    rec.bindDescriptorSets(PipelineBindPoint::Compute, MockPipelineLayoutHandle{ .id = 7 }, 0, sets);
    [[maybe_unused]] auto exe = std::move(rec).end();
    EXPECT_EQ(pool.bindDescriptorSetsCount(), 1u);
    EXPECT_EQ(pool.lastBoundDescriptorCount(), 3u);
}

TEST(MockRecorder, DispatchRecordsGroupCounts) {
    ComputePool pool;
    auto rec = pool.allocate().begin();
    rec.dispatch(8, 4, 1);
    [[maybe_unused]] auto exe = std::move(rec).end();
    EXPECT_EQ(pool.dispatchCount(), 1u);
    EXPECT_EQ(pool.lastDispatchX(), 8u);
    EXPECT_EQ(pool.lastDispatchY(), 4u);
    EXPECT_EQ(pool.lastDispatchZ(), 1u);
}

template <typename Rec>
concept HasDispatch = requires(Rec r) { r.dispatch(1u, 1u, 1u); };

void dispatchConstraintCompiletimeCheck() {
    static_assert(HasDispatch<MockRecorder<queue_kind::Compute>>);
    static_assert(HasDispatch<MockRecorder<queue_kind::Graphics>>);
    static_assert(!HasDispatch<MockRecorder<queue_kind::Transfer>>);
}

TEST(MockRecorder, PipelineBarrierRecordsImageCount) {
    GraphicsPool pool;
    MockAllocator alloc;
    auto img1 = alloc.createImage({ .width = 64, .height = 64, .format = ImageFormat::RGBA8_UNORM });
    auto img2 = alloc.createImage({ .width = 64, .height = 64, .format = ImageFormat::RGBA8_UNORM });
    auto rec = pool.allocate().begin();
    const std::array<MockImageBarrier, 2> barriers{ { { .image = &img1,
                                                        .oldLayout = ImageLayout::Undefined,
                                                        .newLayout = ImageLayout::General,
                                                        .srcStage = PipelineStage::TopOfPipe,
                                                        .dstStage = PipelineStage::ComputeShader,
                                                        .srcAccess = AccessFlags::None,
                                                        .dstAccess = AccessFlags::ShaderWrite },
                                                      { .image = &img2,
                                                        .oldLayout = ImageLayout::General,
                                                        .newLayout = ImageLayout::PresentSrc,
                                                        .srcStage = PipelineStage::ComputeShader,
                                                        .dstStage = PipelineStage::BottomOfPipe,
                                                        .srcAccess = AccessFlags::ShaderWrite,
                                                        .dstAccess = AccessFlags::None } } };
    rec.pipelineBarrier(barriers);
    [[maybe_unused]] auto exe = std::move(rec).end();
    EXPECT_EQ(pool.pipelineBarrierCount(), 1u);
    EXPECT_EQ(pool.lastBarrierImageCount(), 2u);
}

TEST(MockRecorder, EmptyBarrierStillCountsAsCall) {
    GraphicsPool pool;
    auto rec = pool.allocate().begin();
    rec.pipelineBarrier({});
    [[maybe_unused]] auto exe = std::move(rec).end();
    EXPECT_EQ(pool.pipelineBarrierCount(), 1u);
    EXPECT_EQ(pool.lastBarrierImageCount(), 0u);
}

TEST(MockRecorder, CopyBufferIncrementsCount) {
    TransferPool pool;
    MockAllocator alloc;
    auto src = alloc.createBuffer({ .size = 1024, .usage = BufferUsage::TransferSrc });
    auto dst = alloc.createBuffer({ .size = 1024, .usage = BufferUsage::TransferDst });
    auto rec = pool.allocate().begin();
    const std::array<BufferCopy, 1> regions{ { { .srcOffset = 0, .dstOffset = 0, .size = 1024 } } };
    rec.copyBuffer(src, dst, regions);
    [[maybe_unused]] auto exe = std::move(rec).end();
    EXPECT_EQ(pool.copyBufferCount(), 1u);
}

TEST(MockRecorder, CopyBufferToImageIncrementsCount) {
    TransferPool pool;
    MockAllocator alloc;
    auto src = alloc.createBuffer({ .size = uint64_t{ 4 } * 64 * 64, .usage = BufferUsage::TransferSrc });
    auto dst = alloc.createImage({ .width = 64,
                                   .height = 64,
                                   .format = ImageFormat::RGBA8_UNORM,
                                   .usage = ImageUsage::Storage | ImageUsage::TransferDst });
    auto rec = pool.allocate().begin();
    const std::array<BufferImageCopy, 1> regions{ { { .imageWidth = 64, .imageHeight = 64, .imageDepth = 1 } } };
    rec.copyBufferToImage(src, dst, ImageLayout::TransferDstOptimal, regions);
    [[maybe_unused]] auto exe = std::move(rec).end();
    EXPECT_EQ(pool.copyBufferToImageCount(), 1u);
}

TEST(MockRecorder, FullVoxelRaycastFrameRecordsAllCommands) {
    ComputePool pool;
    MockAllocator alloc;
    auto target = alloc.createImage(
        { .width = 1920, .height = 1080, .format = ImageFormat::RGBA8_UNORM, .usage = ImageUsage::Storage });
    const std::array<MockDescriptorSetHandle, 1> sets{ { { 1 } } };

    auto rec = pool.allocate().begin();
    const std::array<MockImageBarrier, 1> toGeneral{ { { .image = &target,
                                                         .oldLayout = ImageLayout::Undefined,
                                                         .newLayout = ImageLayout::General,
                                                         .dstStage = PipelineStage::ComputeShader,
                                                         .dstAccess = AccessFlags::ShaderWrite } } };
    rec.pipelineBarrier(toGeneral);
    rec.bindPipeline(PipelineBindPoint::Compute, MockPipelineHandle{ .id = 1 });
    rec.bindDescriptorSets(PipelineBindPoint::Compute, MockPipelineLayoutHandle{ .id = 1 }, 0, sets);
    rec.dispatch(1920 / 8, 1080 / 8, 1);
    const std::array<MockImageBarrier, 1> toPresent{ { { .image = &target,
                                                         .oldLayout = ImageLayout::General,
                                                         .newLayout = ImageLayout::PresentSrc,
                                                         .srcStage = PipelineStage::ComputeShader,
                                                         .srcAccess = AccessFlags::ShaderWrite } } };
    rec.pipelineBarrier(toPresent);
    [[maybe_unused]] auto exe = std::move(rec).end();

    EXPECT_EQ(pool.pipelineBarrierCount(), 2u);
    EXPECT_EQ(pool.bindPipelineCount(), 1u);
    EXPECT_EQ(pool.bindDescriptorSetsCount(), 1u);
    EXPECT_EQ(pool.dispatchCount(), 1u);
    EXPECT_EQ(pool.lastDispatchX(), 240u);
    EXPECT_EQ(pool.lastDispatchY(), 135u);
}
