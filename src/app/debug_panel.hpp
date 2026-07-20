#pragma once

#include <cstdint>
#include <functional>
#include "async_vox_loader.hpp"
#include "debug_ui.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/toolkit/content/chunk_streamer.hpp"
#include "engine/toolkit/pipeline/voxel_pipeline.hpp"
#include "player_controller.hpp"
#include "profiler.hpp"
#include "shared/histogram.hpp"

namespace RAGE::App {
    class Window;

    /**
     * @brief The whole debug overlay: owns the ImGui DebugUi, the stat histograms,
     *        and the panel builder (FPS/memory/GPU/streaming/controls sections).
     *        main.cpp calls frame(dt) once per loop iteration and pushRenderMs()
     *        after the render call; everything else is internal.
     */
    class DebugPanel {
    public:
        /// Static facts for the streaming section (derived in main's world config).
        struct StreamInfo {
            int32_t hRadius = 0;
            Toolkit::Content::ChunkStore::YRange yRange{};
        };

        DebugPanel(Toolkit::VoxelPipeline &pipeline, Window &window, Profiler &profiler,
                   AsyncVoxLoader &loader, PlayerController &player, Node3D &root,
                   Toolkit::Content::ChunkStreamer &streamer, StreamInfo info);

        DebugPanel(const DebugPanel &) = delete;
        DebugPanel &operator=(const DebugPanel &) = delete;
        DebugPanel(DebugPanel &&) = delete;
        DebugPanel &operator=(DebugPanel &&) = delete;

        /// Per-frame sampling: histograms, window-title FPS, profiler plots,
        /// right-click pixel-pick handling (queue before render, print results).
        void frame(float dt);
        /// Timing of the render call itself, pushed after pipeline.render().
        void pushRenderMs(float ms);

        bool wantsMouse() const { return ui_.wantsMouse(); }
        bool wantsKeyboard() const { return ui_.wantsKeyboard(); }
        float scrollDelta() { return ui_.scrollDelta(); }

    private:
        void buildPanel_();
        void pollPixelPick_();
        void sectionLoader_();
        void sectionFps_();
        void sectionMemory_();
        void sectionGpu_();
        void sectionStreaming_();
        void sectionControls_();

        Toolkit::VoxelPipeline &pipeline_;
        Window &window_;
        Profiler &profiler_;
        AsyncVoxLoader &loader_;
        PlayerController &player_;
        Node3D &root_;
        Toolkit::Content::ChunkStreamer &streamer_;
        StreamInfo info_;
        DebugUi ui_;

        // Renderer-toggle mirrors (ImGui needs mutable state between frames).
        bool mipSkipEnabled_ = false;
        int heatmapMode_ = 0;
        int heatmapMaxSteps_ = 0;
        bool useSvdag_ = false;
        bool gridTexture_ = false;
        bool brickDedup_ = false;

        Core::Histogram<float, 128> frameMsHistory_;
        Core::Histogram<float, 128> renderMsHistory_;
        Core::Histogram<float, 128> brickmapMBHistory_;
        Core::Histogram<float, 128> gpuMemoryMBHistory_;
        Core::Histogram<float, 128> processRSSMBHistory_;
        double uiFps_ = 0.0;
        double uiFrameMs_ = 0.0;
        double fpsAccumDt_ = 0.0;
        uint32_t fpsAccumFrames_ = 0;
        double fpsLastUpdate_ = 0.0;
        bool prevPickClick_ = false;
    };
}
