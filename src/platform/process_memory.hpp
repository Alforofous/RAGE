#pragma once

#include <cstdint>

namespace RAGE::Platform {
    /** @brief Process resident-set size in bytes, or 0 if unsupported on this platform. */
    uint64_t processResidentBytes() noexcept;
}
