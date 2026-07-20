#include "chunk_streamer.hpp"
#include "shared/profiling.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>
#include "engine/scene/voxel3d.hpp"
#include "shared/logger.hpp"

namespace RAGE::Toolkit::Content {
    namespace {
        bool inCylinder(IVec3 coord, IVec3 focus, int32_t hRadius, ChunkStore::YRange y) {
            const int32_t dx = coord.x - focus.x;
            const int32_t dz = coord.z - focus.z;
            const int32_t hr2 = hRadius * hRadius;
            return (dx * dx) + (dz * dz) <= hr2 && coord.y >= y.min && coord.y <= y.max;
        }
    }

    ChunkStreamer::ChunkStreamer(ChunkStore &store, Voxel3D &world, int32_t horizontalRadius)
        : store_(store)
        , world_(world)
        , hRadius_(horizontalRadius) {
        const ChunkStore::YRange y = store_.yRange();
        const IVec3 cb = store_.chunkBrickDims();
        const IVec3 expected{ ((2 * hRadius_) + 1) * cb.x, (y.max - y.min + 1) * cb.y,
                              ((2 * hRadius_) + 1) * cb.z };
        if (world_.voxelData()->brickDims() != expected) {
            throw std::invalid_argument(
                "ChunkStreamer: world volume window does not match radius/yRange "
                "(capacity injection mismatch)");
        }
        worker_ = std::thread([this] { workerMain_(); });
    }

    ChunkStreamer::~ChunkStreamer() {
        {
            std::lock_guard lock(mtx_);
            shouldStop_ = true;
        }
        workerCv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void ChunkStreamer::workerMain_() {
        Core::profileThreadName("ChunkLoader");
        while (true) {
            IVec3 coord{};
            {
                std::unique_lock lock(mtx_);
                workerCv_.wait(lock, [this] { return shouldStop_ || !pending_.empty(); });
                if (shouldStop_) {
                    return;
                }
                coord = pending_.front();
                pending_.pop_front();
                inFlight_.insert(coord);
            }

            ChunkResult result;
            try {
                const Core::ProfileZone zone("Chunk.Load");
                result = store_.chunkAt(coord);
            } catch (const std::exception &e) {
                const std::string msg = std::string("ChunkStreamer worker: chunk generation failed: ")
                                        + e.what();
                Core::log(Core::LogLevel::Error, msg.c_str());
                result = ChunkResult{ .status = ChunkStatus::Missing, .chunk = nullptr };
            }

            {
                std::lock_guard lock(mtx_);
                inFlight_.erase(coord);
                ready_.push_back(ReadyEntry{ .coord = coord, .result = std::move(result) });
            }
            idleCv_.notify_all();
        }
    }

    void ChunkStreamer::slideWindow_(IVec3 focusChunk) {
        const ChunkStore::YRange y = store_.yRange();
        const IVec3 cb = store_.chunkBrickDims();
        const IVec3 windowBricks = world_.voxelData()->brickDims();
        // Window min lands exactly on the cylinder's bounding box; Y is anchored to
        // the store's fixed vertical range, not the focus height.
        const IVec3 windowMinBrick{ (focusChunk.x - hRadius_) * cb.x, y.min * cb.y,
                                    (focusChunk.z - hRadius_) * cb.z };
        const IVec3 centerBrick = windowMinBrick
                                  + IVec3{ windowBricks.x / 2, windowBricks.y / 2,
                                           windowBricks.z / 2 };
        world_.setWindowCenter(
            IVec3{ centerBrick.x * 8, centerBrick.y * 8, centerBrick.z * 8 });
    }

    void ChunkStreamer::update(Vec3 worldFocus) {
        // Focus chunk from a world-space position; chunk world extent per axis is
        // chunkBricks * 8 voxels * voxelSize. (World volume transform is identity
        // by current contract — see api-north-star known gap.)
        const IVec3 cb = store_.chunkBrickDims();
        const float vs = world_.voxelSize();
        const Vec3 extent{ static_cast<float>(cb.x) * 8.0f * vs,
                           static_cast<float>(cb.y) * 8.0f * vs,
                           static_cast<float>(cb.z) * 8.0f * vs };
        update(IVec3{ static_cast<int32_t>(std::floor(worldFocus.x / extent.x)),
                      static_cast<int32_t>(std::floor(worldFocus.y / extent.y)),
                      static_cast<int32_t>(std::floor(worldFocus.z / extent.z)) });
    }

    void ChunkStreamer::update(IVec3 focusChunk) {
        std::deque<ReadyEntry> drained;
        bool queuesIdle = false;
        {
            std::lock_guard lock(mtx_);
            drained.swap(ready_);
            queuesIdle = pending_.empty() && inFlight_.empty();
        }

        // Settled: nothing arrived, nothing in flight, no Pending retries owed, focus
        // unmoved — the cylinder scan would be a no-op, skip the whole pass.
        const bool quiet = drained.empty() && queuesIdle && deferred_.empty()
                           && hasUpdated_ && focusChunk == lastFocus_;
        lastFocus_ = focusChunk;
        hasUpdated_ = true;
        if (quiet) {
            return;
        }

        // Window first: adoption below must land inside the window, and departing
        // cells free their bricks during this call.
        slideWindow_(focusChunk);

        deferred_.clear();
        {
            const Core::ProfileZone drainZone("ChunkStreamer.Drain");
            const ChunkStore::YRange y = store_.yRange();
            for (auto &entry : drained) {
                if (!inCylinder(entry.coord, focusChunk, hRadius_, y)) {
                    continue;
                }
                applyResult_(entry.coord, std::move(entry.result));
            }
        }
        {
            const Core::ProfileZone evictZone("ChunkStreamer.Evict");
            pruneOutOfWindow_(focusChunk);
        }
        {
            const Core::ProfileZone repopZone("ChunkStreamer.Repopulate");
            repopulatePending_(focusChunk);
        }
    }

    void ChunkStreamer::applyResult_(IVec3 coord, ChunkResult result) {
        if (loaded_.contains(coord)) {
            return;
        }
        if (result.status == ChunkStatus::Ready && result.chunk) {
            const IVec3 cb = store_.chunkBrickDims();
            world_.voxelData()->adoptBricksFrom(
                *result.chunk->voxelData(),
                IVec3{ coord.x * cb.x, coord.y * cb.y, coord.z * cb.z });
            loaded_.insert(coord);
            // result.chunk is brickless now; it dies here, releasing nothing.
        } else if (result.status == ChunkStatus::Empty || result.status == ChunkStatus::Missing) {
            skipped_.emplace(coord, result.status);
        } else if (result.status == ChunkStatus::Pending) {
            deferred_.insert(coord);
        }
    }

    void ChunkStreamer::pruneOutOfWindow_(IVec3 focusChunk) {
        // Eviction matches loading: the cylinder. Cells the window slide already
        // freed cost nothing to clear again; cells between the cylinder and the
        // window box are cleared explicitly so unloading looks like loading.
        const ChunkStore::YRange y = store_.yRange();
        const IVec3 cb = store_.chunkBrickDims();
        for (auto it = loaded_.begin(); it != loaded_.end();) {
            if (!inCylinder(*it, focusChunk, hRadius_, y)) {
                const IVec3 c = *it;
                world_.voxelData()->clearBricks(
                    IVec3{ c.x * cb.x, c.y * cb.y, c.z * cb.z }, cb);
                it = loaded_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = skipped_.begin(); it != skipped_.end();) {
            if (!inCylinder(it->first, focusChunk, hRadius_, y)) {
                it = skipped_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ChunkStreamer::repopulatePending_(IVec3 focusChunk) {
        const int32_t hRadius = hRadius_;
        struct Candidate {
            IVec3 coord;
            int32_t dist2;
        };

        const ChunkStore::YRange y = store_.yRange();
        const int32_t hr2 = hRadius * hRadius;
        std::vector<Candidate> candidates;
        for (int32_t cy = y.min; cy <= y.max; ++cy) {
            for (int32_t dz = -hRadius; dz <= hRadius; ++dz) {
                for (int32_t dx = -hRadius; dx <= hRadius; ++dx) {
                    const int32_t d2 = (dx * dx) + (dz * dz);
                    if (d2 > hr2) {
                        continue;
                    }
                    const IVec3 c{ focusChunk.x + dx, cy, focusChunk.z + dz };
                    if (loaded_.contains(c) || skipped_.contains(c) || deferred_.contains(c)) {
                        continue;
                    }
                    candidates.push_back({ .coord = c, .dist2 = d2 });
                }
            }
        }

        std::ranges::sort(candidates, {}, &Candidate::dist2);

        {
            std::lock_guard lock(mtx_);
            std::unordered_set<IVec3, IVec3Hash> readyCoords;
            for (const auto &entry : ready_) {
                readyCoords.insert(entry.coord);
            }
            pending_.clear();
            for (const auto &c : candidates) {
                if (inFlight_.contains(c.coord) || readyCoords.contains(c.coord)) {
                    continue;
                }
                pending_.push_back(c.coord);
            }
        }
        workerCv_.notify_all();
    }

    size_t ChunkStreamer::pendingCount() const {
        std::lock_guard lock(mtx_);
        return pending_.size() + inFlight_.size() + ready_.size();
    }

    void ChunkStreamer::flushAsync(IVec3 focusChunk) {
        while (true) {
            update(focusChunk);
            std::unique_lock lock(mtx_);
            idleCv_.wait(lock, [this] { return pending_.empty() && inFlight_.empty(); });
            if (ready_.empty()) {
                return;
            }
        }
    }
}
