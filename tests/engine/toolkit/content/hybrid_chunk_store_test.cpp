#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <utility>
#include "engine/toolkit/content/chunk_store.hpp"
#include "engine/toolkit/content/hybrid_chunk_store.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"

using namespace RAGE;
using namespace RAGE::Toolkit::Content;

namespace RAGE::Mocks {
    class ScriptedStore : public ChunkStore {
    public:
        ScriptedStore(BrickPool &pool, IVec3 brickDims, ChunkStatus status, bool writable,
                      ChunkStore::YRange y)
            : pool_(pool)
            , brickDims_(brickDims)
            , status_(status)
            , writable_(writable)
            , yRange_(y) {}

        IVec3 chunkBrickDims() const override { return brickDims_; }
        YRange yRange() const override { return yRange_; }
        bool isWritable() const override { return writable_; }

        ChunkResult chunkAt(IVec3 coord) override {
            ++queries_;
            lastQueried_ = coord;
            if (status_ == ChunkStatus::Ready) {
                auto v = std::make_unique<Voxel3D>(pool_, brickDims_ * 8, 0.05f);
                return ChunkResult{ .status = ChunkStatus::Ready, .chunk = std::move(v) };
            }
            return ChunkResult{ .status = status_, .chunk = nullptr };
        }

        void putChunk(IVec3 coord, const Voxel3D & /*chunk*/) override {
            ++puts_;
            lastPut_ = coord;
        }

        int queries_ = 0;
        int puts_ = 0;
        IVec3 lastQueried_{};
        IVec3 lastPut_{};

    private:
        BrickPool &pool_;
        IVec3 brickDims_{};
        ChunkStatus status_ = ChunkStatus::Missing;
        bool writable_ = false;
        YRange yRange_{};
    };
}

namespace {
    constexpr IVec3 kBrickDims{ 2, 2, 2 };

    struct Fixture {
        BrickPool pool;
        Mocks::ScriptedStore *overlay = nullptr;
        Mocks::ScriptedStore *baseline = nullptr;
        std::unique_ptr<HybridChunkStore> store;

        Fixture(ChunkStatus overlayStatus, ChunkStatus baselineStatus,
                ChunkStore::YRange overlayY = { .min = 0, .max = 0 },
                ChunkStore::YRange baselineY = { .min = 0, .max = 0 }) {
            auto o = std::make_unique<Mocks::ScriptedStore>(pool, kBrickDims, overlayStatus, true,
                                                            overlayY);
            auto b = std::make_unique<Mocks::ScriptedStore>(pool, kBrickDims, baselineStatus,
                                                            false, baselineY);
            overlay = o.get();
            baseline = b.get();
            store = std::make_unique<HybridChunkStore>(std::move(o), std::move(b));
        }
    };
}

TEST(HybridChunkStore, OverlayReadyShadowsBaseline) {
    Fixture f(ChunkStatus::Ready, ChunkStatus::Ready);
    const ChunkResult r = f.store->chunkAt(IVec3{ 1, 2, 3 });
    EXPECT_EQ(r.status, ChunkStatus::Ready);
    EXPECT_NE(r.chunk, nullptr);
    EXPECT_EQ(f.overlay->queries_, 1);
    EXPECT_EQ(f.baseline->queries_, 0);
}

TEST(HybridChunkStore, OverlayEmptyMasksBaseline) {
    Fixture f(ChunkStatus::Empty, ChunkStatus::Ready);
    const ChunkResult r = f.store->chunkAt(IVec3{ 0, 0, 0 });
    EXPECT_EQ(r.status, ChunkStatus::Empty);
    EXPECT_EQ(r.chunk, nullptr);
    EXPECT_EQ(f.baseline->queries_, 0);
}

TEST(HybridChunkStore, OverlayPendingDefersWithoutBaselineQuery) {
    Fixture f(ChunkStatus::Pending, ChunkStatus::Ready);
    const ChunkResult r = f.store->chunkAt(IVec3{ 0, 0, 0 });
    EXPECT_EQ(r.status, ChunkStatus::Pending);
    EXPECT_EQ(f.baseline->queries_, 0);
}

TEST(HybridChunkStore, OverlayMissingFallsThroughToBaseline) {
    Fixture f(ChunkStatus::Missing, ChunkStatus::Ready);
    const ChunkResult r = f.store->chunkAt(IVec3{ 5, 0, -5 });
    EXPECT_EQ(r.status, ChunkStatus::Ready);
    EXPECT_NE(r.chunk, nullptr);
    EXPECT_EQ(f.overlay->queries_, 1);
    EXPECT_EQ(f.baseline->queries_, 1);
    EXPECT_EQ(f.baseline->lastQueried_, (IVec3{ 5, 0, -5 }));
}

TEST(HybridChunkStore, BothMissingIsMissing) {
    Fixture f(ChunkStatus::Missing, ChunkStatus::Missing);
    const ChunkResult r = f.store->chunkAt(IVec3{ 0, 0, 0 });
    EXPECT_EQ(r.status, ChunkStatus::Missing);
}

TEST(HybridChunkStore, PutChunkTargetsOverlay) {
    Fixture f(ChunkStatus::Missing, ChunkStatus::Missing);
    const Voxel3D chunk(f.pool, kBrickDims * 8, 0.05f);
    f.store->putChunk(IVec3{ 7, 0, 7 }, chunk);
    EXPECT_EQ(f.overlay->puts_, 1);
    EXPECT_EQ(f.overlay->lastPut_, (IVec3{ 7, 0, 7 }));
    EXPECT_EQ(f.baseline->puts_, 0);
}

TEST(HybridChunkStore, IsWritableMirrorsOverlay) {
    Fixture f(ChunkStatus::Missing, ChunkStatus::Missing);
    EXPECT_TRUE(f.store->isWritable());
}

TEST(HybridChunkStore, YRangeIsUnionOfBothStores) {
    Fixture f(ChunkStatus::Missing, ChunkStatus::Missing, { .min = -3, .max = 1 },
              { .min = -1, .max = 4 });
    const ChunkStore::YRange y = f.store->yRange();
    EXPECT_EQ(y.min, -3);
    EXPECT_EQ(y.max, 4);
}

TEST(HybridChunkStore, NullStoreThrows) {
    BrickPool pool;
    auto b = std::make_unique<Mocks::ScriptedStore>(pool, kBrickDims, ChunkStatus::Missing, false,
                                                    ChunkStore::YRange{ .min = 0, .max = 0 });
    EXPECT_THROW(HybridChunkStore(nullptr, std::move(b)), std::invalid_argument);
}

TEST(HybridChunkStore, MismatchedBrickDimsThrows) {
    BrickPool pool;
    auto o = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Missing,
                                                    true, ChunkStore::YRange{ .min = 0, .max = 0 });
    auto b = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 4, 4, 4 }, ChunkStatus::Missing,
                                                    false, ChunkStore::YRange{ .min = 0, .max = 0 });
    EXPECT_THROW(HybridChunkStore(std::move(o), std::move(b)), std::invalid_argument);
}

TEST(HybridChunkStore, WriteThroughCachesBaselineReadyIntoOverlay) {
    BrickPool pool;
    auto o = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Missing,
                                                    true, ChunkStore::YRange{ .min = 0, .max = 0 });
    auto b = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Ready,
                                                    false, ChunkStore::YRange{ .min = 0, .max = 0 });
    Mocks::ScriptedStore *overlay = o.get();
    HybridChunkStore store(std::move(o), std::move(b), WriteThrough::CacheBaselineReady);

    const ChunkResult r = store.chunkAt(IVec3{ 3, 0, -1 });
    EXPECT_EQ(r.status, ChunkStatus::Ready);
    EXPECT_EQ(overlay->puts_, 1);
    EXPECT_EQ(overlay->lastPut_, (IVec3{ 3, 0, -1 }));
}

TEST(HybridChunkStore, WriteThroughSkipsBaselineEmpty) {
    BrickPool pool;
    auto o = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Missing,
                                                    true, ChunkStore::YRange{ .min = 0, .max = 0 });
    auto b = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Empty,
                                                    false, ChunkStore::YRange{ .min = 0, .max = 0 });
    Mocks::ScriptedStore *overlay = o.get();
    HybridChunkStore store(std::move(o), std::move(b), WriteThrough::CacheBaselineReady);

    EXPECT_EQ(store.chunkAt(IVec3{ 0, 0, 0 }).status, ChunkStatus::Empty);
    EXPECT_EQ(overlay->puts_, 0);
}

TEST(HybridChunkStore, DisabledWriteThroughNeverWrites) {
    BrickPool pool;
    auto o = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Missing,
                                                    true, ChunkStore::YRange{ .min = 0, .max = 0 });
    auto b = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Ready,
                                                    false, ChunkStore::YRange{ .min = 0, .max = 0 });
    Mocks::ScriptedStore *overlay = o.get();
    HybridChunkStore store(std::move(o), std::move(b));

    EXPECT_EQ(store.chunkAt(IVec3{ 0, 0, 0 }).status, ChunkStatus::Ready);
    EXPECT_EQ(overlay->puts_, 0);
}

TEST(HybridChunkStore, WriteThroughWithNonWritableOverlayThrows) {
    BrickPool pool;
    auto o = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Missing,
                                                    false, ChunkStore::YRange{ .min = 0, .max = 0 });
    auto b = std::make_unique<Mocks::ScriptedStore>(pool, IVec3{ 2, 2, 2 }, ChunkStatus::Ready,
                                                    false, ChunkStore::YRange{ .min = 0, .max = 0 });
    EXPECT_THROW(HybridChunkStore(std::move(o), std::move(b), WriteThrough::CacheBaselineReady),
                 std::invalid_argument);
}
