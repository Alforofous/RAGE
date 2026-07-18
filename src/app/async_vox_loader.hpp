#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "engine/scene/voxel3d.hpp"
#include "engine/toolkit/content/vox_loader.hpp"

namespace RAGE::App {
    /**
     * @brief Background .vox filler: stages models against freshly-constructed
     *        Voxel3Ds, then fills them on a worker thread so the main loop never
     *        blocks on asset decode. Progress and cancellation ride the volume's
     *        own load hooks; errors are captured and readable from the UI thread.
     *
     * Lifecycle: stage(...) any number of volumes → start() → poll done()/error().
     * cancelAndJoin() + reset() rewind everything for a scene rebuild (the caller
     * must destroy the staged volumes BEFORE resetting the brick pool — brick
     * handles return to their issuing pool).
     */
    class AsyncVoxLoader {
    public:
        AsyncVoxLoader() = default;
        ~AsyncVoxLoader();

        AsyncVoxLoader(const AsyncVoxLoader &) = delete;
        AsyncVoxLoader &operator=(const AsyncVoxLoader &) = delete;
        AsyncVoxLoader(AsyncVoxLoader &&) = delete;
        AsyncVoxLoader &operator=(AsyncVoxLoader &&) = delete;

        /**
         * @brief Parse `voxPath` (throws on IO/format errors) and build a Voxel3D of
         *        the model's dimensions, wired for progress + cancellation. The fill
         *        itself is deferred to the worker; call before start().
         */
        std::unique_ptr<Voxel3D> stage(const std::filesystem::path &voxPath, BrickPool &pool,
                                       float voxelSize);

        /// Launch the worker over everything staged so far.
        void start();

        /// Request cancellation and wait for the worker to exit. Idempotent.
        void cancelAndJoin();

        /// Forget all jobs and clear the error. Only valid after cancelAndJoin().
        void reset();

        /// True once the worker has finished (or was cancelled).
        bool done() const { return done_.load(); }

        /// Last recorded load error, empty when none. Safe from any thread.
        std::string error() const;

        /// Visit (label, progress 0..1) of every staged job — for the debug UI.
        void forEachJob(const std::function<void(const std::string &, float)> &fn) const;

    private:
        struct Job {
            std::string label;
            Toolkit::Content::VoxModel model;
            Voxel3D *target = nullptr;
            std::atomic<float> progress{ 0.0f };
        };

        void workerMain_();
        void recordError_(const std::string &what);

        std::vector<std::unique_ptr<Job>> jobs_;
        std::thread worker_;
        std::atomic<bool> cancel_{ false };
        std::atomic<bool> done_{ false };
        mutable std::mutex errorMutex_;
        std::string error_;
    };
}
