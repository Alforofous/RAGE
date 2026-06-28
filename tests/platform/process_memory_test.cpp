#include <gtest/gtest.h>
#include "platform/process_memory.hpp"

TEST(ProcessMemory, ReturnsPositiveResidentSize) {
#if defined(_WIN32)
    const uint64_t rss = RAGE::Platform::processResidentBytes();
    // Any live process holds at least a few MB of RSS — the test process itself
    // is well above this. A zero return would mean the syscall failed.
    EXPECT_GT(rss, 1u * 1024u * 1024u);
#else
    // Non-Windows currently unimplemented; expects 0.
    EXPECT_EQ(RAGE::Platform::processResidentBytes(), 0u);
#endif
}

TEST(ProcessMemory, ConsecutiveCallsReturnRoughlySimilarValues) {
#if defined(_WIN32)
    const uint64_t a = RAGE::Platform::processResidentBytes();
    const uint64_t b = RAGE::Platform::processResidentBytes();
    // Drift between two back-to-back calls in a single-threaded test should be
    // small — definitely no more than a few hundred MB (allocator slop).
    const uint64_t delta = (a > b) ? (a - b) : (b - a);
    EXPECT_LT(delta, 256u * 1024u * 1024u);
#endif
}
