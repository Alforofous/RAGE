#include <gtest/gtest.h>
#include "shared/histogram.hpp"

using RAGE::Core::Histogram;

TEST(Histogram, DefaultIsEmpty) {
    Histogram<float, 8> h;
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.full());
    EXPECT_EQ(h.size(), 0u);
    EXPECT_EQ(h.capacity(), 8u);
    EXPECT_EQ(h.latest(), 0.0f);
    EXPECT_EQ(h.oldestOffset(), 0u);
}

TEST(Histogram, AggregatesOnEmptyReturnZero) {
    const Histogram<float, 8> h;
    EXPECT_EQ(h.min(), 0.0f);
    EXPECT_EQ(h.max(), 0.0f);
    EXPECT_EQ(h.mean(), 0.0f);
}

TEST(Histogram, PushPopulatesInOrderUntilFull) {
    Histogram<float, 4> h;
    h.push(1.0f);
    h.push(2.0f);
    h.push(3.0f);
    EXPECT_EQ(h.size(), 3u);
    EXPECT_FALSE(h.full());
    EXPECT_EQ(h.latest(), 3.0f);
    EXPECT_EQ(h.oldestOffset(), 0u);   // not full yet, ring starts at index 0
    EXPECT_EQ(h.min(), 1.0f);
    EXPECT_EQ(h.max(), 3.0f);
    EXPECT_EQ(h.mean(), 2.0f);
}

TEST(Histogram, ExactlyCapacityFillsRingWithoutWrap) {
    Histogram<float, 4> h;
    h.push(1.0f);
    h.push(2.0f);
    h.push(3.0f);
    h.push(4.0f);
    EXPECT_TRUE(h.full());
    EXPECT_EQ(h.size(), 4u);
    EXPECT_EQ(h.latest(), 4.0f);
    EXPECT_EQ(h.oldestOffset(), 0u);   // head wraps to 0 after fourth push; ring is full from idx 0
    EXPECT_EQ(h.min(), 1.0f);
    EXPECT_EQ(h.max(), 4.0f);
    EXPECT_FLOAT_EQ(h.mean(), 2.5f);
}

TEST(Histogram, PushBeyondCapacityWrapsAndOverwritesOldest) {
    Histogram<float, 4> h;
    h.push(1.0f);
    h.push(2.0f);
    h.push(3.0f);
    h.push(4.0f);
    h.push(5.0f);                       // overwrites the 1.0f at index 0
    EXPECT_TRUE(h.full());
    EXPECT_EQ(h.size(), 4u);
    EXPECT_EQ(h.latest(), 5.0f);
    EXPECT_EQ(h.oldestOffset(), 1u);   // oldest live sample now at index 1 (=2.0f)
    EXPECT_EQ(h.min(), 2.0f);
    EXPECT_EQ(h.max(), 5.0f);
    EXPECT_FLOAT_EQ(h.mean(), 3.5f);
}

TEST(Histogram, FullWraparoundReturnsToOffsetZero) {
    Histogram<float, 4> h;
    for (int32_t i = 1; i <= 8; ++i) {
        h.push(static_cast<float>(i));   // push 8 values into a 4-slot ring
    }
    EXPECT_EQ(h.latest(), 8.0f);
    EXPECT_EQ(h.oldestOffset(), 0u);   // head_ wrapped back to 0 after 8 pushes
    EXPECT_EQ(h.min(), 5.0f);          // [5, 6, 7, 8] live
    EXPECT_EQ(h.max(), 8.0f);
    EXPECT_FLOAT_EQ(h.mean(), 6.5f);
}

TEST(Histogram, ChronologicalReadViaDataAndOffset) {
    Histogram<float, 4> h;
    h.push(10.0f);
    h.push(20.0f);
    h.push(30.0f);
    h.push(40.0f);
    h.push(50.0f);                      // ring is now [50, 20, 30, 40] starting at head_=1
    EXPECT_EQ(h.oldestOffset(), 1u);

    // Walk live samples in chronological order: should yield 20, 30, 40, 50.
    const auto *ptr = h.data();
    const size_t offset = h.oldestOffset();
    const size_t cap = decltype(h)::capacity();
    EXPECT_EQ(ptr[(offset + 0) % cap], 20.0f);
    EXPECT_EQ(ptr[(offset + 1) % cap], 30.0f);
    EXPECT_EQ(ptr[(offset + 2) % cap], 40.0f);
    EXPECT_EQ(ptr[(offset + 3) % cap], 50.0f);
}

TEST(Histogram, ClearReturnsToEmptyState) {
    Histogram<float, 4> h;
    h.push(1.0f);
    h.push(2.0f);
    h.push(3.0f);
    h.push(4.0f);
    h.push(5.0f);
    h.clear();
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.full());
    EXPECT_EQ(h.size(), 0u);
    EXPECT_EQ(h.latest(), 0.0f);
    EXPECT_EQ(h.oldestOffset(), 0u);
    // Re-fill behaves as if fresh.
    h.push(7.0f);
    EXPECT_EQ(h.latest(), 7.0f);
    EXPECT_EQ(h.size(), 1u);
}

TEST(Histogram, WorksWithIntegerType) {
    Histogram<int32_t, 8> h;
    h.push(5);
    h.push(-3);
    h.push(10);
    h.push(0);
    EXPECT_EQ(h.min(), -3);
    EXPECT_EQ(h.max(), 10);
    EXPECT_EQ(h.mean(), 3);             // (5 + -3 + 10 + 0) / 4 = 3
    EXPECT_EQ(h.latest(), 0);
}

TEST(Histogram, WorksWithDoubleType) {
    Histogram<double, 4> h;
    h.push(1.5);
    h.push(2.5);
    h.push(3.5);
    EXPECT_DOUBLE_EQ(h.mean(), 2.5);
    EXPECT_DOUBLE_EQ(h.min(), 1.5);
    EXPECT_DOUBLE_EQ(h.max(), 3.5);
}

TEST(Histogram, AggregatesOverSingleSample) {
    Histogram<float, 8> h;
    h.push(42.0f);
    EXPECT_EQ(h.min(), 42.0f);
    EXPECT_EQ(h.max(), 42.0f);
    EXPECT_EQ(h.mean(), 42.0f);
    EXPECT_EQ(h.latest(), 42.0f);
}

TEST(Histogram, CapacityOneAlwaysHoldsLatestOnly) {
    Histogram<float, 1> h;
    h.push(100.0f);
    EXPECT_EQ(h.size(), 1u);
    EXPECT_TRUE(h.full());
    EXPECT_EQ(h.latest(), 100.0f);

    h.push(200.0f);
    EXPECT_EQ(h.size(), 1u);
    EXPECT_EQ(h.latest(), 200.0f);
    EXPECT_EQ(h.min(), 200.0f);
    EXPECT_EQ(h.max(), 200.0f);
    EXPECT_EQ(h.mean(), 200.0f);
}
