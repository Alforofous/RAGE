#pragma once

#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace RAGE {
    class Camera;

    /**
     * Engine-wide, per-frame context handed to every renderable's prepareFrame hook.
     *
     * Fields are things that are constant across all renderables in a single frame: the active
     * camera, eventually time / frame index / lighting environment. Renderables read what they
     * need and ignore the rest.
     */
    struct FrameContext {
        const Camera *camera = nullptr;
    };

    /**
     * Small accumulator a renderable's prepareFrame hook writes its push-constant struct into.
     *
     * The renderer creates one builder per renderable per frame, hands it to prepareFrame, and
     * passes the resulting bytes to the pipeline's execute(). Renderables that don't need push
     * constants simply leave the builder untouched.
     *
     * write<T> overwrites any prior content; only the most-recent struct survives. The renderer
     * never reads from a partially-written or stale builder.
     */
    class PushConstantBuilder {
    public:
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        void write(const T &data) {
            const auto *src = reinterpret_cast<const std::byte *>(&data);
            bytes_.assign(src, src + sizeof(T));
        }

        void clear() { bytes_.clear(); }
        bool empty() const { return bytes_.empty(); }
        std::span<const std::byte> bytes() const { return bytes_; }

    private:
        std::vector<std::byte> bytes_;
    };
}
