#include <gtest/gtest.h>
#include "platform/process_memory.hpp"

TEST(ProcessMemory, ReturnsPositiveResidentSize) {
#if defined(_WIN32)
    EXPECT_GT(RAGE::Platform::processResidentBytes(), 1u * 1024u * 1024u);
#else
    EXPECT_EQ(RAGE::Platform::processResidentBytes(), 0u);
#endif
}

TEST(ProcessMemory, ConsecutiveCallsReturnRoughlySimilarValues) {
#if defined(_WIN32)
    const uint64_t a = RAGE::Platform::processResidentBytes();
    const uint64_t b = RAGE::Platform::processResidentBytes();
    const uint64_t delta = (a > b) ? (a - b) : (b - a);
    EXPECT_LT(delta, 256u * 1024u * 1024u);
#endif
}
