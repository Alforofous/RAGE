#include "process_memory.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

namespace RAGE::Platform {
    uint64_t processResidentBytes() noexcept {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS info{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)) != 0) {
            return static_cast<uint64_t>(info.WorkingSetSize);
        }
        return 0;
#else
        return 0;
#endif
    }
}
