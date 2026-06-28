#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include "shared/histogram.hpp"

namespace RAGE {
    class Renderer;
}

namespace RAGE::App {
    /**
     * @brief Accumulates per-phase render-time histories from the renderer's PhaseHook
     *        callbacks. Each named phase keeps a sliding window of recent durations;
     *        consumers iterate via forEachPhase to render plots.
     */
    class PhaseTracker {
    public:
        static constexpr size_t kHistoryDepth = 128;
        using PhaseHistory = Core::Histogram<float, kHistoryDepth>;

        void attach(Renderer &renderer);

        void forEachPhase(
            const std::function<void(const std::string &name, const PhaseHistory &history)> &fn) const;

    private:
        struct PhaseState {
            std::chrono::steady_clock::time_point startedAt{};
            PhaseHistory history;
        };

        void onBegin(const char *name);
        void onEnd(const char *name);

        std::unordered_map<std::string, PhaseState> phases_;
    };
}
