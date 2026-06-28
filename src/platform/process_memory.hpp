#pragma once

#include <cstdint>

namespace RAGE::Platform {
    /**
     * Current process's resident-set size in bytes — "how much memory is this
     * process using right now" from the OS's perspective. Used by the debug-UI
     * memory plot to surface the program's total footprint alongside the engine-
     * visible breakdown (brick pool + handle grids + SVDAG).
     *
     * Platform-specific under the hood:
     *   - Windows: GetProcessMemoryInfo + WorkingSetSize.
     *   - Other platforms: returns 0 (not implemented yet).
     *
     * Never throws. A zero return distinguishes "platform unsupported" from a
     * legitimate near-zero RSS (real processes never have RSS < a few MB).
     */
    uint64_t processResidentBytes() noexcept;
}
