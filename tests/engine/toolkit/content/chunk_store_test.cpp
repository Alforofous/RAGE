#include <gtest/gtest.h>
#include <memory>
#include <unordered_set>
#include "engine/toolkit/content/chunk_generator.hpp"
#include "engine/toolkit/content/chunk_generators.hpp"
#include "engine/toolkit/content/chunk_store.hpp"
#include "engine/toolkit/content/procedural_chunk_store.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"

using namespace RAGE;
using namespace RAGE::Toolkit::Content;

namespace RAGE::Mocks {
    class MapChunkStore : public ChunkStore {
    public:
        MapChunkStore(BrickPool &pool, IVec3 chunkBrickDims, float voxelSize)
            : pool_(pool)
            , chunkBrickDims_(chunkBrickDims)
            , voxelSize_(voxelSize) {}

        IVec3 chunkBrickDims() const override { return chunkBrickDims_; }
        YRange yRange() const override { return { .min = 0, .max = 0 }; }

        ChunkResult chunkAt(IVec3 coord) override {
            ++callsToChunkAt;
            if (!present_.contains(packCoord(coord))) {
                return ChunkResult{ .status = ChunkStatus::Missing, .chunk = nullptr };
            }
            auto out = std::make_unique<Voxel3D>(pool_, chunkBrickDims_ * 8, voxelSize_);
            return ChunkResult{ .status = ChunkStatus::Ready, .chunk = std::move(out) };
        }

        bool isWritable() const override { return writable_; }
        void setWritable(bool w) { writable_ = w; }

        void seed(IVec3 coord) { present_.insert(packCoord(coord)); }

        int callsToChunkAt = 0;

    private:
        static uint64_t packCoord(IVec3 c) {
            const uint64_t x = static_cast<uint32_t>(c.x);
            const uint64_t y = static_cast<uint32_t>(c.y);
            const uint64_t z = static_cast<uint32_t>(c.z);
            return (x & 0x1FFFFFull) | ((y & 0x1FFFFFull) << 21) | ((z & 0x1FFFFFull) << 42);
        }

        BrickPool &pool_;
        IVec3 chunkBrickDims_{};
        float voxelSize_ = 0.f;
        bool writable_ = false;
        std::unordered_set<uint64_t> present_;
    };

    class HybridChunkStore : public ChunkStore {
    public:
        HybridChunkStore(ChunkStore &overlay, ChunkStore &baseline)
            : overlay_(overlay)
            , baseline_(baseline) {}

        IVec3 chunkBrickDims() const override { return baseline_.chunkBrickDims(); }
        YRange yRange() const override { return baseline_.yRange(); }

        ChunkResult chunkAt(IVec3 coord) override {
            ChunkResult r = overlay_.chunkAt(coord);
            if (r.status == ChunkStatus::Ready || r.status == ChunkStatus::Empty
                || r.status == ChunkStatus::Pending) {
                return r;
            }
            return baseline_.chunkAt(coord);
        }

        bool isWritable() const override { return overlay_.isWritable(); }
        void putChunk(IVec3 coord, const Voxel3D &chunk) override { overlay_.putChunk(coord, chunk); }

    private:
        ChunkStore &overlay_;
        ChunkStore &baseline_;
    };
}

namespace {
    constexpr float kVoxelSize = 0.05f;
    constexpr ChunkStore::YRange kY{ .min = 0, .max = 0 };
}

TEST(ProceduralChunkStore, ForwardsChunkBrickDimsFromGenerator) {
    BrickPool pool;
    ProceduralChunkStore store(std::make_unique<TerrainChunkGenerator>(IVec3{ 2, 4, 2 }), pool,
                                kVoxelSize, kY);
    EXPECT_EQ(store.chunkBrickDims(), IVec3(2, 4, 2));
}

TEST(ProceduralChunkStore, ChunkAtReturnsReadyWithMatchingDimensions) {
    BrickPool pool;
    ProceduralChunkStore store(std::make_unique<TerrainChunkGenerator>(IVec3{ 4, 4, 4 }), pool,
                                kVoxelSize, kY);
    const ChunkResult r = store.chunkAt(IVec3{ 0, 0, 0 });
    EXPECT_EQ(r.status, ChunkStatus::Ready);
    ASSERT_NE(r.chunk, nullptr);
    EXPECT_EQ(r.chunk->dimensions(), IVec3(32, 32, 32));
}

TEST(ProceduralChunkStore, IsNotWritableByDefault) {
    BrickPool pool;
    ProceduralChunkStore store(std::make_unique<TerrainChunkGenerator>(), pool, kVoxelSize, kY);
    EXPECT_FALSE(store.isWritable());
}

TEST(HybridChunkStoreSemantics, OverlayHitShortCircuitsBaseline) {
    BrickPool pool;
    Mocks::MapChunkStore overlay(pool, IVec3{ 4, 4, 4 }, kVoxelSize);
    Mocks::MapChunkStore baseline(pool, IVec3{ 4, 4, 4 }, kVoxelSize);
    overlay.seed(IVec3{ 1, 0, 1 });

    Mocks::HybridChunkStore hybrid(overlay, baseline);
    const ChunkResult r = hybrid.chunkAt(IVec3{ 1, 0, 1 });
    EXPECT_EQ(r.status, ChunkStatus::Ready);
    EXPECT_EQ(overlay.callsToChunkAt, 1);
    EXPECT_EQ(baseline.callsToChunkAt, 0);
}

TEST(HybridChunkStoreSemantics, OverlayMissFallsThroughToBaseline) {
    BrickPool pool;
    Mocks::MapChunkStore overlay(pool, IVec3{ 4, 4, 4 }, kVoxelSize);
    Mocks::MapChunkStore baseline(pool, IVec3{ 4, 4, 4 }, kVoxelSize);
    baseline.seed(IVec3{ 2, 0, 3 });

    Mocks::HybridChunkStore hybrid(overlay, baseline);
    const ChunkResult r = hybrid.chunkAt(IVec3{ 2, 0, 3 });
    EXPECT_EQ(r.status, ChunkStatus::Ready);
    EXPECT_EQ(overlay.callsToChunkAt, 1);
    EXPECT_EQ(baseline.callsToChunkAt, 1);
}

TEST(HybridChunkStoreSemantics, BothMissReturnsMissing) {
    BrickPool pool;
    Mocks::MapChunkStore overlay(pool, IVec3{ 4, 4, 4 }, kVoxelSize);
    Mocks::MapChunkStore baseline(pool, IVec3{ 4, 4, 4 }, kVoxelSize);

    Mocks::HybridChunkStore hybrid(overlay, baseline);
    const ChunkResult r = hybrid.chunkAt(IVec3{ 9, 9, 9 });
    EXPECT_EQ(r.status, ChunkStatus::Missing);
    EXPECT_EQ(r.chunk, nullptr);
}

TEST(HybridChunkStoreSemantics, WritabilityFollowsOverlay) {
    BrickPool pool;
    Mocks::MapChunkStore overlay(pool, IVec3{ 4, 4, 4 }, kVoxelSize);
    Mocks::MapChunkStore baseline(pool, IVec3{ 4, 4, 4 }, kVoxelSize);
    overlay.setWritable(true);

    Mocks::HybridChunkStore hybrid(overlay, baseline);
    EXPECT_TRUE(hybrid.isWritable());
}
