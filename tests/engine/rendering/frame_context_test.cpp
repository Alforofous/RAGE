#include <gtest/gtest.h>
#include <cstring>
#include "engine/rendering/frame_context.hpp"

using namespace RAGE;

namespace {
    struct Sample {
        float a;
        float b;
        int32_t c;
    };
}

TEST(PushConstantBuilder, DefaultIsEmpty) {
    const PushConstantBuilder pc;
    EXPECT_TRUE(pc.empty());
    EXPECT_EQ(pc.bytes().size(), 0u);
}

TEST(PushConstantBuilder, WriteStoresSizeofT) {
    PushConstantBuilder pc;
    pc.write(Sample{ .a = 1.0f, .b = 2.0f, .c = 3 });
    EXPECT_EQ(pc.bytes().size(), sizeof(Sample));
    EXPECT_FALSE(pc.empty());
}

TEST(PushConstantBuilder, WriteRoundTrips) {
    PushConstantBuilder pc;
    pc.write(Sample{ .a = 1.5f, .b = 2.5f, .c = 42 });
    Sample restored{};
    std::memcpy(&restored, pc.bytes().data(), sizeof(Sample));
    EXPECT_FLOAT_EQ(restored.a, 1.5f);
    EXPECT_FLOAT_EQ(restored.b, 2.5f);
    EXPECT_EQ(restored.c, 42);
}

TEST(PushConstantBuilder, WriteOverwritesPrior) {
    PushConstantBuilder pc;
    pc.write(Sample{ .a = 1.0f, .b = 2.0f, .c = 3 });
    pc.write(Sample{ .a = 9.0f, .b = 8.0f, .c = 7 });

    Sample restored{};
    std::memcpy(&restored, pc.bytes().data(), sizeof(Sample));
    EXPECT_FLOAT_EQ(restored.a, 9.0f);
    EXPECT_EQ(restored.c, 7);
}

TEST(PushConstantBuilder, ClearEmptiesBuffer) {
    PushConstantBuilder pc;
    pc.write(Sample{ .a = 1.0f, .b = 2.0f, .c = 3 });
    pc.clear();
    EXPECT_TRUE(pc.empty());
}
