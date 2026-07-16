#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "engine/content/chunk_store.hpp"
#include "engine/content/streamer.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/voxel3d.hpp"

using namespace RAGE;
using namespace RAGE::Content;

namespace RAGE::Mocks {
    class StreamerMockStore : public ChunkStore {
    public:
        StreamerMockStore(BrickPool &pool, IVec3 chunkBrickDims, float voxelSize)
            : pool_(pool)
            , chunkBrickDims_(chunkBrickDims)
            , voxelSize_(voxelSize) {}

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
}

TEST(Streamer, EmptyStartLoadsCylinderOfReadyChunks) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.loadedCount(), 5u);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 1, 0, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, -1 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 1, 0, 1 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 0, 1, 0 }));
}

TEST(Streamer, YRangeStacksLayers) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);
    store.setYRange({ .min = -1, .max = 1 });

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.loadedCount(), 15u);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 1, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, -1, 0 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 0, 2, 0 }));
}

TEST(Streamer, YRangeIsAbsoluteNotRelativeToFocus) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 100, 0 }, 0);

    EXPECT_EQ(s.loadedCount(), 1u);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 0, 100, 0 }));
}

TEST(Streamer, RepeatUpdateAtSameFocusDoesNotReQueryLoadedChunks) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);
    EXPECT_EQ(store.callsFor(IVec3{ 1, 0, 0 }), 1);
    EXPECT_EQ(root.childCount(), 5u);
}

TEST(Streamer, FocusShiftEvictsOutOfRangeAndLoadsNewSlice) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 1, 0, 0 }, 1);

    EXPECT_EQ(s.loadedCount(), 5u);
    EXPECT_FALSE(s.isLoaded(IVec3{ -1, 0, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 2, 0, 0 }));
}

TEST(Streamer, FlyingUpDoesNotEvictGroundChunks) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));

    s.flushAsync(IVec3{ 0, 99, 0 }, 1);
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));
    EXPECT_EQ(s.loadedCount(), 5u);
}

TEST(Streamer, EmptyStatusIsRememberedAndNotReQueried) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Empty);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.skippedCount(), 5u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);
}

TEST(Streamer, MissingStatusIsRememberedAndNotReQueried) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Missing);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    EXPECT_EQ(s.skippedCount(), 5u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);
}

TEST(Streamer, PendingStatusIsRetriedOnNextUpdate) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Empty);
    store.seed(IVec3{ 0, 0, 0 }, ChunkStatus::Pending);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(s.loadedCount(), 0u);
    EXPECT_EQ(s.skippedCount(), 0u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 1);

    store.seed(IVec3{ 0, 0, 0 }, ChunkStatus::Ready);
    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(s.loadedCount(), 1u);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 2);
}

TEST(Streamer, ChurningFocusNeverOrphansSceneTreeChildren) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    for (int32_t i = 0; i < 500; ++i) {
        s.update(IVec3{ i % 7, 0, (i * 3) % 5 }, 2);
    }
    s.flushAsync(IVec3{ 0, 0, 0 }, 2);

    EXPECT_EQ(root.childCount(), s.loadedCount());
}

TEST(Streamer, OnPrepareHookRunsForEachReadyChunkBeforeAttach) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    std::mutex seenMtx;
    std::unordered_set<IVec3, IVec3Hash> seen;
    Streamer s(store, root);
    s.setOnChunkPrepare([&seenMtx, &seen](Voxel3D &, IVec3 c) {
        std::lock_guard lock(seenMtx);
        seen.insert(c);
    });
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    std::lock_guard lock(seenMtx);
    EXPECT_EQ(seen.size(), 5u);
    EXPECT_TRUE(seen.contains(IVec3{ 0, 0, 0 }));
    EXPECT_TRUE(seen.contains(IVec3{ 1, 0, 0 }));
}

TEST(Streamer, EvictionForgetsSkippedSoReentryReQueries) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Empty);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(s.skippedCount(), 1u);

    s.flushAsync(IVec3{ 10, 0, 0 }, 0);
    EXPECT_FALSE(s.isSkipped(IVec3{ 0, 0, 0 }));

    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    EXPECT_EQ(store.callsFor(IVec3{ 0, 0, 0 }), 2);
}

TEST(Streamer, DestructorCleansUpWorkerWhileChunksInFlight) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    s.update(IVec3{ 0, 0, 0 }, 1);
}

TEST(Streamer, PlacedEventFiresPerAttachedChunkWithCoord) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    std::mutex mtx;
    std::unordered_map<IVec3, const Voxel3D *, IVec3Hash> placed;
    Streamer s(store, root);
    s.setOnChunkPlaced([&mtx, &placed](IVec3 c, Voxel3D &v) {
        std::lock_guard lock(mtx);
        placed.emplace(c, &v);
    });
    s.flushAsync(IVec3{ 0, 0, 0 }, 1);

    std::lock_guard lock(mtx);
    EXPECT_EQ(placed.size(), 5u);
    ASSERT_TRUE(placed.contains(IVec3{ 0, 0, 0 }));
    EXPECT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));
}

TEST(Streamer, EvictedEventFiresForOutOfRangeChunksBeforeDestruction) {
    BrickPool pool;
    Node3D root;
    Mocks::StreamerMockStore store(pool, kChunkDims, kVoxelSize);
    store.setDefault(ChunkStatus::Ready);

    Streamer s(store, root);
    s.flushAsync(IVec3{ 0, 0, 0 }, 0);
    ASSERT_TRUE(s.isLoaded(IVec3{ 0, 0, 0 }));

    std::mutex mtx;
    std::vector<IVec3> evicted;
    s.setOnChunkEvicted([&mtx, &evicted](IVec3 c, Voxel3D &v) {
        std::lock_guard lock(mtx);
        evicted.push_back(c);
        EXPECT_EQ(v.dimensions(), kChunkDims * 8);
    });
    s.flushAsync(IVec3{ 10, 0, 0 }, 0);

    std::lock_guard lock(mtx);
    ASSERT_EQ(evicted.size(), 1u);
    EXPECT_EQ(evicted[0], (IVec3{ 0, 0, 0 }));
    EXPECT_FALSE(s.isLoaded(IVec3{ 0, 0, 0 }));
}
