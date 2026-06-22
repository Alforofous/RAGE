#include <gtest/gtest.h>
#include "math/ivec.hpp"

using namespace RAGE;

TEST(IVec3, DefaultIsZero) {
    const IVec3 v;
    EXPECT_EQ(v.x, 0);
    EXPECT_EQ(v.y, 0);
    EXPECT_EQ(v.z, 0);
}

TEST(IVec3, ComponentConstruct) {
    const IVec3 v(1, 2, 3);
    EXPECT_EQ(v.x, 1);
    EXPECT_EQ(v.y, 2);
    EXPECT_EQ(v.z, 3);
}

TEST(IVec3, ZeroFactory) {
    EXPECT_EQ(IVec3::zero(), IVec3(0, 0, 0));
}

TEST(IVec3, Addition) {
    EXPECT_EQ(IVec3(1, 2, 3) + IVec3(4, 5, 6), IVec3(5, 7, 9));
}

TEST(IVec3, Subtraction) {
    EXPECT_EQ(IVec3(10, 10, 10) - IVec3(1, 2, 3), IVec3(9, 8, 7));
}

TEST(IVec3, ScalarMultiplication) {
    EXPECT_EQ(IVec3(1, 2, 3) * 3, IVec3(3, 6, 9));
}

TEST(IVec3, UnaryNegate) {
    EXPECT_EQ(-IVec3(1, -2, 3), IVec3(-1, 2, -3));
}

TEST(IVec3, EqualityOperator) {
    EXPECT_EQ(IVec3(1, 2, 3), IVec3(1, 2, 3));
    EXPECT_NE(IVec3(1, 2, 3), IVec3(1, 2, 4));
}

TEST(IVec3, VolumeProductOfComponents) {
    EXPECT_EQ(IVec3(2, 3, 4).volume(), 24);
    EXPECT_EQ(IVec3::zero().volume(), 0);
}

TEST(IVec3, VolumeFitsInInt64ForLargeGrids) {
    EXPECT_EQ(IVec3(2000, 2000, 2000).volume(), 8'000'000'000LL);
}
