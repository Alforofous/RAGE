#include "debug_panel.hpp"

#include <array>
#include <cfloat>
#include <cstdio>
#include <functional>
#include <span>
#include <GLFW/glfw3.h>
#include "app/build_paths.hpp"
#include "engine/scene/svdag.hpp"
#include "engine/scene/voxel3d.hpp"
#include "platform/process_memory.hpp"
#include "window.hpp"

namespace RAGE::App {
    DebugPanel::DebugPanel(Toolkit::VoxelPipeline &pipeline, Window &window, Profiler &profiler,
                           AsyncVoxLoader &loader, PlayerController &player, Node3D &root,
                           std::optional<Toolkit::Content::ChunkStreamer> &streamer,
                           StreamInfo info, std::function<void(bool)> resetScene)
        : pipeline_(pipeline)
        , window_(window)
        , profiler_(profiler)
        , loader_(loader)
        , player_(player)
        , root_(root)
        , streamer_(streamer)
        , info_(info)
        , resetScene_(std::move(resetScene))
        , ui_(pipeline.context(), window.glfwHandle()) {
        Renderer &renderer = pipeline_.renderer();
        ui_.attach(renderer);
        mipSkipEnabled_ = renderer.mipSkipEnabled();
        heatmapMode_ = renderer.heatmapMode();
        heatmapMaxSteps_ = renderer.heatmapMaxSteps();
        useSvdag_ = renderer.useSvdag();
        gridTexture_ = renderer.useGridTexture();
        brickDedup_ = renderer.brickPool().isDedupEnabled();
        fpsLastUpdate_ = glfwGetTime();
        ui_.setBuilder([this]() { buildPanel_(); });
    }

    void DebugPanel::frame(float dt) {
        Renderer &renderer = pipeline_.renderer();
        frameMsHistory_.push(dt * 1000.0f);
        gpuMemoryMBHistory_.push(static_cast<float>(pipeline_.gpuMemoryUsedBytes())
                                 / (1024.0f * 1024.0f));
        const float brickPoolMB =
            static_cast<float>(renderer.brickPool().allocatedBytes()) / (1024.0f * 1024.0f);
        const float handleGridMB = static_cast<float>(renderer.worldBrickGrid().handles().size()
                                                      * sizeof(BrickHandle))
                                   / (1024.0f * 1024.0f);
        brickmapMBHistory_.push(brickPoolMB + handleGridMB);
        const uint64_t rss = Platform::processResidentBytes();
        if (rss > 0) {
            processRSSMBHistory_.push(static_cast<float>(rss) / (1024.0f * 1024.0f));
        }

        fpsAccumDt_ += dt;
        ++fpsAccumFrames_;
        const double now = glfwGetTime();
        if (now - fpsLastUpdate_ > 0.25) {
            const double avgMs = (fpsAccumDt_ / static_cast<double>(fpsAccumFrames_)) * 1000.0;
            const double fps = static_cast<double>(fpsAccumFrames_) / (now - fpsLastUpdate_);
            uiFps_ = fps;
            uiFrameMs_ = avgMs;
            std::array<char, 128> titleBuf{};
            std::snprintf(titleBuf.data(), titleBuf.size(), "RAGE Smoke | FPS %.1f | %.2f ms",
                          fps, avgMs);
            window_.setTitle(titleBuf.data());
            profiler_.plot("fps", fps);
            profiler_.plot("frame_ms", avgMs);
            profiler_.plot("brick_count", static_cast<double>(renderer.brickPool().allocated()));
            profiler_.plot("brick_pool_mb",
                           static_cast<double>(renderer.brickPool().allocatedBytes())
                               / (1024.0 * 1024.0));
            if (streamer_.has_value()) {
                profiler_.plot("chunks_loaded", static_cast<double>(streamer_->loadedCount()));
                profiler_.plot("chunks_pending", static_cast<double>(streamer_->pendingCount()));
            }
            fpsLastUpdate_ = now;
            fpsAccumDt_ = 0.0;
            fpsAccumFrames_ = 0;
        }
    }

    void DebugPanel::pushRenderMs(float ms) { renderMsHistory_.push(ms); }

    void DebugPanel::buildPanel_() {
        Renderer &renderer = pipeline_.renderer();
        ui_.beginPanel("RAGE Debug");
        if (!loader_.done()) {
            ui_.text("Loading assets...");
            loader_.forEachJob([this](const std::string &label, float progress) {
                ui_.text("  %s: load %.0f%%", label.c_str(), progress * 100.0f);
            });
        }
        const std::string loadError = loader_.error();
        if (!loadError.empty()) {
            ui_.text("! %s", loadError.c_str());
        }

        std::array<char, 64> header{};
        std::snprintf(header.data(), header.size(), "FPS %.0f  (%.2f ms)###fps", uiFps_,
                      uiFrameMs_);
        if (ui_.collapsingHeader(header.data())) {
            ui_.plot("Frame", frameMsHistory_.data(), frameMsHistory_.capacity(),
                     frameMsHistory_.size(), frameMsHistory_.oldestOffset(), "%.2f ms", 0.0f,
                     FLT_MAX);
            ui_.plot("Render", renderMsHistory_.data(), renderMsHistory_.capacity(),
                     renderMsHistory_.size(), renderMsHistory_.oldestOffset(), "%.2f ms", 0.0f,
                     FLT_MAX);
        }

        const double rssMB = static_cast<double>(Platform::processResidentBytes())
                             / (1024.0 * 1024.0);
        std::snprintf(header.data(), header.size(), "Memory %.0f MB###mem", rssMB);
        if (ui_.collapsingHeader(header.data())) {
            ui_.text("Brick dedup:    %s", brickDedup_ ? "on" : "off");
            if (ui_.button(brickDedup_ ? "Restart with no dedup" : "Restart with dedup")) {
                resetScene_(!brickDedup_);
                brickDedup_ = !brickDedup_;
            }
            const auto bricksUnique = renderer.brickPool().allocated();
            const auto bricksLogical = renderer.brickPool().logicalBricks();
            const double poolMB = static_cast<double>(renderer.brickPool().allocatedBytes())
                                  / (1024.0 * 1024.0);
            size_t handleGridBytes = 0;
            size_t denseBytes = 0;
            std::function<void(const Node3D &)> walkScene = [&](const Node3D &node) {
                if (const auto *v = dynamic_cast<const Voxel3D *>(&node)) {
                    if (const VoxelData *vd = v->voxelData()) {
                        handleGridBytes += vd->handleGridBytes();
                        denseBytes += vd->denseEquivalentBytes();
                    }
                }
                for (const auto &child : node.children()) {
                    walkScene(*child);
                }
            };
            walkScene(root_);
            const double handleGridKB = static_cast<double>(handleGridBytes) / 1024.0;
            const double brickmapTotalMB = poolMB + (handleGridKB / 1024.0);
            const double denseMB = static_cast<double>(denseBytes) / (1024.0 * 1024.0);
            const double savingsMB = denseMB - brickmapTotalMB;
            const double savingsPct = denseMB > 0.0 ? (savingsMB / denseMB) * 100.0 : 0.0;
            const double dedupRatio = bricksUnique > 0
                ? static_cast<double>(bricksLogical) / static_cast<double>(bricksUnique)
                : 1.0;

            ui_.text("Pool:           %.2f MB", poolMB);
            ui_.text("Handle grids:   %.2f KB", handleGridKB);
            ui_.text("Bricks unique:  %zu / %zu", bricksUnique, renderer.brickPool().maxBricks());
            ui_.text("Bricks logical: %zu", bricksLogical);
            ui_.text("Dedup ratio:    %.2fx", dedupRatio);
            ui_.text("Dense:          %.2f MB", denseMB);
            ui_.separator();
            ui_.text("Total:          %.2f MB  (saved %.2f MB / %.0f%%)", brickmapTotalMB,
                     savingsMB, savingsPct);
            ui_.plot("Brickmap", brickmapMBHistory_.data(), brickmapMBHistory_.capacity(),
                     brickmapMBHistory_.size(), brickmapMBHistory_.oldestOffset(), "%.2f MB",
                     0.0f, FLT_MAX);
            ui_.plot("Process RSS", processRSSMBHistory_.data(), processRSSMBHistory_.capacity(),
                     processRSSMBHistory_.size(), processRSSMBHistory_.oldestOffset(), "%.0f MB",
                     0.0f, FLT_MAX);
        }

        const double gpuMB = static_cast<double>(pipeline_.gpuMemoryUsedBytes())
                             / (1024.0 * 1024.0);
        std::snprintf(header.data(), header.size(), "GPU %.0f MB###gpu", gpuMB);
        if (ui_.collapsingHeader(header.data())) {
            ui_.plot("GPU memory", gpuMemoryMBHistory_.data(), gpuMemoryMBHistory_.capacity(),
                     gpuMemoryMBHistory_.size(), gpuMemoryMBHistory_.oldestOffset(), "%.1f MB",
                     0.0f, FLT_MAX);
            if (renderer.useSvdag() && !renderer.svdag().nodes.empty()) {
                ui_.separatorText("SVDAG (live)");
                const Svdag &sv = renderer.svdag();
                const size_t flatGridBytes =
                    renderer.worldBrickGrid().handles().size() * sizeof(BrickHandle);
                const double svdagMB = static_cast<double>(svdagBytes(sv)) / (1024.0 * 1024.0);
                const double flatGridMB = static_cast<double>(flatGridBytes) / (1024.0 * 1024.0);
                const double svdagSavedMB = flatGridMB - svdagMB;
                const double svdagSavedPct =
                    flatGridMB > 0.0 ? (svdagSavedMB / flatGridMB) * 100.0 : 0.0;
                ui_.text("Nodes:          %zu", sv.nodes.size());
                ui_.text("Levels:         %d (paddedDim=%d)", sv.levels, sv.paddedDim);
                ui_.text("SVDAG size:     %.3f MB", svdagMB);
                ui_.text("Flat grid:      %.3f MB", flatGridMB);
                ui_.text("Saved vs grid:  %.3f MB (%.0f%%)", svdagSavedMB, svdagSavedPct);
            }
        }

        if (streamer_.has_value()) {
            std::snprintf(header.data(), header.size(), "Streaming  %zu chunks###stream",
                          streamer_->loadedCount());
            if (ui_.collapsingHeader(header.data())) {
                ui_.text("Loaded:  %zu chunks", streamer_->loadedCount());
                ui_.text("Skipped: %zu chunks", streamer_->skippedCount());
                ui_.text("Radius:  h=%d  Y=[%d,%d]", info_.hRadius, info_.yRange.min,
                         info_.yRange.max);
            }
        }

        if (ui_.collapsingHeader("Controls")) {
            if constexpr (App::kIsDevBuild) {
                if (Profiler::isLinked()) {
                    if (profiler_.isProfilerGuiRunning()) {
                        ui_.text("Tracy: running (close to relaunch)");
                    } else if (ui_.button("Launch Tracy")) {
                        profiler_.launchProfilerGui();
                    }
                }
            }
            if (ui_.checkbox("Mip skip", &mipSkipEnabled_)) {
                renderer.setMipSkipEnabled(mipSkipEnabled_);
            }
            if (ui_.checkbox("SVDAG traversal", &useSvdag_)) {
                renderer.setUseSvdag(useSvdag_);
            }
            if (ui_.checkbox("Grid 3D texture", &gridTexture_)) {
                renderer.setUseGridTexture(gridTexture_);
            }
            static const char *const kHeatmapOpts[] = { "Off", "Step count" };
            if (ui_.radio("Heatmap", &heatmapMode_,
                          std::span<const char *const>(kHeatmapOpts, std::size(kHeatmapOpts)))) {
                renderer.setHeatmapMode(heatmapMode_);
            }
            if (heatmapMode_ != 0
                && ui_.sliderInt("Heatmap max steps", &heatmapMaxSteps_, 32, 4096)) {
                renderer.setHeatmapMaxSteps(heatmapMaxSteps_);
            }
            ui_.separatorText("Camera");
            ui_.text("Walk mode (V): %s%s", player_.walkMode() ? "on" : "off",
                     (player_.walkMode() && player_.grounded()) ? " - grounded" : "");
            float speedMult = player_.speedMultiplier();
            if (ui_.sliderFloat("Speed (x)", &speedMult, 0.1f, 10.0f)) {
                player_.setSpeedMultiplier(speedMult);
            }
        }

        ui_.endPanel();
    }
}
