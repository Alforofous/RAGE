#include "async_vox_loader.hpp"

#include <cstdio>
#include <utility>
#include "shared/profiling.hpp"

namespace RAGE::App {
    AsyncVoxLoader::~AsyncVoxLoader() { cancelAndJoin(); }

    std::unique_ptr<Voxel3D> AsyncVoxLoader::stage(const std::filesystem::path &voxPath,
                                                   float voxelSize) {
        auto job = std::make_unique<Job>();
        job->label = voxPath.filename().string();
        job->model = Toolkit::Content::loadVox(voxPath);
        auto volume = std::make_unique<Voxel3D>(job->model.dims, voxelSize);
        job->target = volume.get();
        volume->voxelData()->beginBulkLoad();
        volume->onLoadProgress([&jobRef = *job](float p) { jobRef.progress.store(p); });
        volume->onLoadShouldCancel([this]() { return cancel_.load(); });
        std::fprintf(stdout, "Staged %s (%dx%dx%d)\n", voxPath.string().c_str(),
                     job->model.dims.x, job->model.dims.y, job->model.dims.z);
        jobs_.push_back(std::move(job));
        return volume;
    }

    void AsyncVoxLoader::start() {
        done_.store(false);
        worker_ = std::thread([this]() { workerMain_(); });
    }

    void AsyncVoxLoader::cancelAndJoin() {
        cancel_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void AsyncVoxLoader::reset() {
        jobs_.clear();
        cancel_.store(false);
        done_.store(false);
        std::lock_guard lock(errorMutex_);
        error_.clear();
    }

    std::string AsyncVoxLoader::error() const {
        std::lock_guard lock(errorMutex_);
        return error_;
    }

    void AsyncVoxLoader::forEachJob(
        const std::function<void(const std::string &, float)> &fn) const {
        for (const auto &job : jobs_) {
            fn(job->label, job->progress.load());
        }
    }

    void AsyncVoxLoader::workerMain_() {
        Core::profileThreadName("AssetLoader");
        const Core::ProfileZone outerZone("LoaderThread");
        try {
            for (const auto &job : jobs_) {
                if (cancel_.load()) {
                    break;
                }
                job->target->fillFromPackedRGBA8(job->model.voxels.data(), job->model.dims);
                job->target->voxelData()->endBulkLoad();
                job->model.voxels.clear();
                job->model.voxels.shrink_to_fit();
            }
        } catch (const std::exception &e) {
            recordError_(std::string("loader: ") + e.what());
        }
        for (const auto &job : jobs_) {
            job->target->voxelData()->endBulkLoad();
        }
        done_.store(true);
    }

    void AsyncVoxLoader::recordError_(const std::string &what) {
        std::lock_guard lock(errorMutex_);
        error_ = what;
        std::fprintf(stderr, "[scene] %s\n", what.c_str());
    }
}
