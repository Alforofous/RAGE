#pragma once

#include <concepts>
#include <cstdint>

namespace RAGE::queue_kind {
    namespace caps {
        inline constexpr uint32_t Graphics = 1u << 0;
        inline constexpr uint32_t Compute = 1u << 1;
        inline constexpr uint32_t Transfer = 1u << 2;
    }

    struct Graphics {
        static constexpr uint32_t provides = caps::Graphics | caps::Compute | caps::Transfer;
    };

    struct Compute {
        static constexpr uint32_t provides = caps::Compute | caps::Transfer;
    };

    struct Transfer {
        static constexpr uint32_t provides = caps::Transfer;
    };
}

namespace RAGE {
    template <typename T>
    concept QueueKind = requires { T::provides; } && std::is_same_v<decltype(T::provides), const uint32_t>;

    template <typename Have, typename Need>
    concept Supports = QueueKind<Have> && QueueKind<Need> && ((Have::provides & Need::provides) == Need::provides);
}
