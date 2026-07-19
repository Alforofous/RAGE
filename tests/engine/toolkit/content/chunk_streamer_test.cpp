#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include "engine/toolkit/content/chunk_store.hpp"
#include "engine/toolkit/content/chunk_streamer.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"

using namespace RAGE;
using namespace RAGE::Toolkit::Content;

namespace RAGE::Mocks {
    /// Chunks carry one coord-derived marker voxel at their local origin so adoption
    /// into the world volume is observable (and bricks stay dedup-unique).
    class StreamerMockStore : public ChunkStore {
    public:
        StreamerMockStore(BrickPool &pool, IVec3 chunkBrickDims, float voxelSize)
            : pool_(pool)
            , chunkBrickDims_(chunkBrickDims)
            , voxelSize_(voxelSize) {}

        static uint32_t marker(IVec3 coord) {
            return 0xFF000000u | (static_cast<uint32_t>(coord.x & 0xFF) << 16)
                   | (static_cast<uint32_t>(coord.y & 0xFF) << 8)
                   | static_cast<uint32_t>(coord.z & 0xFF);
        }

        IVec3 chunkBrickDims() const override { return chunkBrickDims_; }
        YRange yRange() const override {
            std::lock_guard lock(mtx_);
            return yRange_;
        }
        void setYRange(YRange y) {
            std::lock_guard lock(mtx_);
            yRange_ = y;
        }

        ChunkResult chunkAt(IVec3 coord) override {
            {
                std::lock_guard lock(mtx_);
                ++callCount_[coord];
            }
            ChunkStatus s = ChunkStatus::Empty;
            {
                std::lock_guard lock(mtx_);
                auto it = forced_.find(coord);
                s = (it == forced_.end()) ? defaultStatus_ : it->second;
            }
            if (s == ChunkStatus::Ready) {
                auto v = std::make_unique<Voxel3D>(pool_, chunkBrickDims_ * 8, voxelSize_);
                v->setVoxel(IVec3{ 0, 0, 0 }, Color::fromRGBA8(marker(coord)));
                return ChunkResult{ .status = ChunkStatus::Ready, .chunk = std::move(v) };
            }
            return ChunkResult{ .status = s, .chunk = nullptr };
        }

        void setDefault(ChunkStatus s) {
            std::lock_guard lock(mtx_);
            defaultStatus_ = s;
        }
        void seed(IVec3 coord, ChunkStatus s) {
            std::lock_guard lock(mtx_);
            forced_[coord] = s;
        }
        int callsFor(IVec3 coord) const {
            std::lock_guard lock(mtx_);
            auto it = callCount_.find(coord);
            return it == callCount_.end() ? 0 : it->second;
        }

    private:
        BrickPool &pool_;
        IVec3 chunkBrickDims_{};
        float voxelSize_ = 0.f;
        mutable std::mutex mtx_;
        ChunkStatus defaultStatus_ = ChunkStatus::Empty;
        std::unordered_map<IVec3, ChunkStatus, IVec3Hash> forced_;
        std::unordered_map<IVec3, int, IVec3Hash> callCount_;
        YRange yRange_{ .min = 0, .max = 0 };
    };
}

namespace {
    constexpr float kVoxelSize = 0.05f;
    constexpr IVec3 kChunkDims{ 4, 4, 4 };

    /// A world volume sized exactly for (radius, yRange) — the streamer validates this.
    std::unique_ptr<Voxel3D> makeWorld(BrickPool &pool, int32_t radius,
                                       ChunkStore::YRange y) {
        const int32_t diameter = (2 * radius) + 1;
        const int32_t yChunks = y.max - y.min + 1;
        const IVec3 dims{ diameter * kChunkDims.x * 8, yChunks * kChunkDims.y * 8,
                          diameter * kChunkDims.z * 8 };
        return std::make_unique<Voxel3D>(pool, dims, kVoxelSize);
    }

    /// The chunk's marker voxel sits at the chunk's world-min corner.
    uint32_t markerAt(const Voxel3D &world, IVec3 chunkCoord) {
        const IVec3 v{ chunkCoord.x * kChunkDims.x * 8, chunkCoord.y * kChunkDims.y * 8,
                       chunkCoord.z * kChunkDims.z * 8 };
        return world.voxelData()->voxel(v);
    }
}

TEST(ChunkStreamer, EmptyStartLoadsCylinderOfReadyChunks) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.loadedCount(), 5u);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 1, 0, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, -1 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 1, 0, 1 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 0, 1, 0 }));
}

TEST(ChunkStreamer, AdoptedChunksAreReadableInTheWorldVolume) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(markerAt(*world, IVec3{ 0, 0, 0 }),
              Mocks::StreamerMockStore::marker(IVec3{ 0, 0, 0 }));
    EXPECT_EQ(markerAt(*world, IVec3{ 0, 0, -1 }),
              Mocks::StreamerMockStore::marker(IVec3{ 0, 0, -1 }));
    // One marker brick per loaded chunk, all owned by the world volume now.
    EXPECT_EQ(pool.allocated(), s.loadedCount());
}

TEST(ChunkStreamer, YRangeStacksLayers) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    store.setYRange({ .min = -1, .max = 1 });
    auto world = makeWorld(pool, 1, { .min = -1, .max = 1 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.loadedCount(), 15u);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 1, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, -1, 0 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 0, 2, 0 }));
}

TEST(ChunkStreamer, YRangeIsAbsoluteNotRelativeToFocus) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 0, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 100, 0 }, 0);

    EXPECT_EQ(s.loadedCount(), 1u);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 0, 100, 0 }));
}

TEST(ChunkStreamer, RepeatUpdateAtSameFocusDoesNotReQueryLoadedChunks) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);
    EXPECT_EQ(store.callsFor(IVec3{ 1, 0, 0 }), 1);
    EXPECT_EQ(pool.allocated(), s.loadedCount());
}

TEST(ChunkStreamer, FocusShiftDropsDepartedAndLoadsNewSlice) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 1, 0, 0 }, 1);

    // {-1,0,0} left the window: bookkeeping dropped it and its voxels are gone.
    EXPECT_FALSE(s.isLoaded(IVec3{ -1, 0, 0 }));
    EXPECT_EQ(markerAt(*world, IVec3{ -1, 0, 0 }), 0u);
    EXPECT_TRUE(s.isLoaded(IVec3{ 2, 0, 0 }));
    // Chunks between the cylinder and the window box stay resident by design:
    // old {0,0,±1} remain inside the new window and stay loaded.
    EXPECT_EQ(s.loadedCount(), 7u);
    EXPECT_EQ(markerAt(*world, IVec3{ 0, 0, 1 }),
              Mocks::StreamerMockStore::marker(IVec3{ 0, 0, 1 }));
}

TEST(ChunkStreamer, FlyingUpDoesNotEvictGroundChunks) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));

    s.flushAsync(IVec3{ 0, 99, 0 }, 1);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));
    EXPECT_EQ(s.loadedCount(), 5u);
    EXPECT_EQ(markerAt(*world, IVec3{ 0, 0, 0 }),
              Mocks::StreamerMockStore::marker(IVec3{ 0, 0, 0 }));
}

TEST(ChunkStreamer, EmptyStatusIsRememberedAndNotReQueried) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Empty);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.skippedCount(), 5u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);
}

TEST(ChunkStreamer, MissingStatusIsRememberedAndNotReQueried) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Missing);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.skippedCount(), 5u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);
}

TEST(ChunkStreamer, PendingStatusIsRetriedOnNextUpdate) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Empty);
    store.seed(IVec3{ 0, 0, 0 }, ChunkStatus::Pending);
    auto world = makeWorld(pool, 0, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(s.loadedCount(), 0u);
    EXPECT_EQ(s.skippedCount(), 0u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);

    store.seed(IVec3{ 0, 0, 0 }, ChunkStatus::Ready);
    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(s.loadedCount(), 1u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 2);
}

TEST(ChunkStreamer, ChurningFocusNeverLeaksBricks) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 2, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    for (int32_t i = 0; i < 500; ++i) {
        s.update(IVec3{ i % 7, 0, (i * 3) % 5 }, 2);
    }
    s.flushAsync(IVec3{ 0, 0, 0 }, 2);

    // Every loaded chunk owns exactly its one marker brick; nothing leaked.
    EXPECT_EQ(pool.allocated(), s.loadedCount());
}

TEST(ChunkStreamer, WindowSlideForgetsDepartedContentForReload) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Empty);
    auto world = makeWorld(pool, 0, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(s.skippedCount(), 1u);

    s.flushAsync(IVec3{ 10, 0, 0 }, 0);
    EXPECT_FALSE(s.isSkipped(IVec3{ 0, 0, 0 }));

    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 2);
}

TEST(ChunkStreamer, MismatchedWorldWindowThrows) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });   // sized for radius 1

    ChunkStreamer s(store, *world);
    EXPECT_THROW(s.update(IVec3{ 0, 0, 0 }, 3), std::invalid_argument);
}

TEST(ChunkStreamer, DestructorCleansUpWorkerWhileChunksInFlight) {
    BrickPool pool;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    auto world = makeWorld(pool, 1, { .min = 0, .max = 0 });

    ChunkStreamer s(store, *world);
    s.update(IVec3{ 0, 0, 0 }, 1);
}
