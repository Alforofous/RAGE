#include "phase_tracker.hpp"

#include "engine/rendering/renderer.hpp"

namespace RAGE::App {
    void PhaseTracker::attach(Renderer &renderer) {
        renderer.onPhaseBegin([this](const char *name) { onBegin(name); });
        renderer.onPhaseEnd([this](const char *name) { onEnd(name); });
    }

    void PhaseTracker::onBegin(const char *name) {
        phases_[name].startedAt = std::chrono::steady_clock::now();
    }

    void PhaseTracker::onEnd(const char *name) {
        const auto it = phases_.find(name);
        if (it == phases_.end()) {
            return;
        }
        const auto elapsed = std::chrono::steady_clock::now() - it->second.startedAt;
        const float ms = std::chrono::duration<float, std::milli>(elapsed).count();
        it->second.history.push(ms);
    }

    void PhaseTracker::forEachPhase(
        const std::function<void(const std::string &, const PhaseHistory &)> &fn) const {
        for (const auto &[name, state] : phases_) {
            fn(name, state.history);
        }
    }
}
