#include <gtest/gtest.h>
#include "math/color.hpp"

using namespace RAGE;

TEST(Color, DefaultIsOpaqueBlack) {
    const Color c;
    EXPECT_FLOAT_EQ(c.r, 0.0f);
    EXPECT_FLOAT_EQ(c.g, 0.0f);
    EXPECT_FLOAT_EQ(c.b, 0.0f);
    EXPECT_FLOAT_EQ(c.a, 1.0f);
}

TEST(Color, ComponentConstruct) {
    const Color c(0.25f, 0.5f, 0.75f, 0.5f);
    EXPECT_FLOAT_EQ(c.r, 0.25f);
    EXPECT_FLOAT_EQ(c.g, 0.5f);
    EXPECT_FLOAT_EQ(c.b, 0.75f);
    EXPECT_FLOAT_EQ(c.a, 0.5f);
}

TEST(Color, AlphaDefaultsToOne) {
    const Color c(1.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(c.a, 1.0f);
}

TEST(Color, StaticFactories) {
    EXPECT_EQ(Color::transparent(), Color(0.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_EQ(Color::black(), Color(0.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(Color::white(), Color(1.0f, 1.0f, 1.0f, 1.0f));
    EXPECT_EQ(Color::red(), Color(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(Color::green(), Color(0.0f, 1.0f, 0.0f, 1.0f));
    EXPECT_EQ(Color::blue(), Color(0.0f, 0.0f, 1.0f, 1.0f));
}

TEST(Color, ToRGBA8RedIsFFin_Byte0) {
    const uint32_t packed = Color::red().toRGBA8();
    EXPECT_EQ(packed & 0xFFu, 0xFFu);
    EXPECT_EQ((packed >> 8u) & 0xFFu, 0x00u);
    EXPECT_EQ((packed >> 16u) & 0xFFu, 0x00u);
    EXPECT_EQ((packed >> 24u) & 0xFFu, 0xFFu);
}

TEST(Color, ToRGBA8GreenIsFFin_Byte1) {
    const uint32_t packed = Color::green().toRGBA8();
    EXPECT_EQ((packed >> 8u) & 0xFFu, 0xFFu);
}

TEST(Color, ToRGBA8BlueIsFFin_Byte2) {
    const uint32_t packed = Color::blue().toRGBA8();
    EXPECT_EQ((packed >> 16u) & 0xFFu, 0xFFu);
}

TEST(Color, ToRGBA8TransparentBlackIsZero) {
    EXPECT_EQ(Color::transparent().toRGBA8(), 0u);
}

TEST(Color, ToRGBA8ClampsOutOfRangeValues) {
    EXPECT_EQ(Color(2.0f, 0.0f, 0.0f, 1.0f).toRGBA8() & 0xFFu, 0xFFu);
    EXPECT_EQ(Color(-1.0f, 0.0f, 0.0f, 1.0f).toRGBA8() & 0xFFu, 0x00u);
}

TEST(Color, ToRGBA8WhiteIsAllFF) {
    EXPECT_EQ(Color::white().toRGBA8(), 0xFFFFFFFFu);
}

TEST(Color, FromRGBA8RoundTrip) {
    const Color original(0.25f, 0.5f, 0.75f, 1.0f);
    const Color roundTrip = Color::fromRGBA8(original.toRGBA8());
    EXPECT_NEAR(roundTrip.r, original.r, 0.01f);
    EXPECT_NEAR(roundTrip.g, original.g, 0.01f);
    EXPECT_NEAR(roundTrip.b, original.b, 0.01f);
    EXPECT_NEAR(roundTrip.a, original.a, 0.01f);
}

TEST(Color, EqualityOperator) {
    EXPECT_EQ(Color::red(), Color(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_NE(Color::red(), Color::blue());
}
