#include "streamer.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>
#include "engine/scene/node3d.hpp"
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

    Streamer::Streamer(ChunkStore &store, Node3D &parent)
        : store_(store)
        , parent_(parent) {
        worker_ = std::thread([this] { workerMain_(); });
    }

    Streamer::~Streamer() {
        {
            std::lock_guard lock(mtx_);
            shouldStop_ = true;
        }
        workerCv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void Streamer::workerMain_() {
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
                result = store_.chunkAt(coord);
            } catch (const std::exception &e) {
                const std::string msg = std::string("Streamer worker: chunk generation failed: ")
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

    void Streamer::update(IVec3 focusChunk, int32_t horizontalRadius) {
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
                           && hasUpdated_ && focusChunk == lastFocus_
                           && horizontalRadius == lastRadius_;
        lastFocus_ = focusChunk;
        lastRadius_ = horizontalRadius;
        hasUpdated_ = true;
        if (quiet) {
            return;
        }

        deferred_.clear();
        const ChunkStore::YRange y = store_.yRange();
        for (auto &entry : drained) {
            if (!inCylinder(entry.coord, focusChunk, horizontalRadius, y)) {
                continue;
            }
            applyResult_(entry.coord, std::move(entry.result));
        }
        evictOutOfRange_(focusChunk, horizontalRadius);
        repopulatePending_(focusChunk, horizontalRadius);
    }

    void Streamer::applyResult_(IVec3 coord, ChunkResult result) {
        if (loaded_.contains(coord)) {
            return;
        }
        if (result.status == ChunkStatus::Ready && result.chunk) {
            if (onPrepare_) {
                onPrepare_(*result.chunk, coord);
            }
            Voxel3D *raw = result.chunk.get();
            parent_.add(std::move(result.chunk));
            loaded_.emplace(coord, raw);
            if (onPlaced_) {
                onPlaced_(coord, *raw);
            }
        } else if (result.status == ChunkStatus::Empty || result.status == ChunkStatus::Missing) {
            skipped_.emplace(coord, result.status);
        } else if (result.status == ChunkStatus::Pending) {
            deferred_.insert(coord);
        }
    }

    void Streamer::evictOutOfRange_(IVec3 focusChunk, int32_t hRadius) {
        const ChunkStore::YRange y = store_.yRange();
        std::unordered_set<const Node3D *> evicted;
        for (auto it = loaded_.begin(); it != loaded_.end();) {
            if (!inCylinder(it->first, focusChunk, hRadius, y)) {
                if (onEvicted_) {
                    onEvicted_(it->first, *it->second);
                }
                evicted.insert(it->second);
                it = loaded_.erase(it);
            } else {
                ++it;
            }
        }
        if (!evicted.empty()) {
            parent_.removeChildrenIf(
                [&evicted](const Node3D &c) { return evicted.contains(&c); });
        }
        for (auto it = skipped_.begin(); it != skipped_.end();) {
            if (!inCylinder(it->first, focusChunk, hRadius, y)) {
                it = skipped_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Streamer::repopulatePending_(IVec3 focusChunk, int32_t hRadius) {
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

    size_t Streamer::pendingCount() const {
        std::lock_guard lock(mtx_);
        return pending_.size() + inFlight_.size() + ready_.size();
    }

    void Streamer::flushAsync(IVec3 focusChunk, int32_t horizontalRadius) {
        while (true) {
            update(focusChunk, horizontalRadius);
            std::unique_lock lock(mtx_);
            idleCv_.wait(lock, [this] { return pending_.empty() && inFlight_.empty(); });
            if (ready_.empty()) {
                return;
            }
        }
    }
}
