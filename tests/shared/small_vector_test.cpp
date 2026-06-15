#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include "shared/small_vector.hpp"

using RAGE::Core::SmallVector;

TEST(SmallVector, DefaultConstructIsEmpty) {
    const SmallVector<int, 4> v;
    EXPECT_EQ(v.size(), 0u);
    EXPECT_TRUE(v.empty());
}

TEST(SmallVector, DestructorRunsForAllElements) {
    static int liveCount = 0;
    struct Tracker {
        Tracker() { liveCount++; }
        Tracker(const Tracker &other) {
            (void)other;
            liveCount++;
        }
        Tracker(Tracker &&other) noexcept {
            (void)other;
            liveCount++;
        }
        ~Tracker() { liveCount--; }
    };
    liveCount = 0;
    {
        SmallVector<Tracker, 4> v;
        v.push_back(Tracker{});
        v.push_back(Tracker{});
        v.push_back(Tracker{});
        EXPECT_EQ(liveCount, 3);
    }
    EXPECT_EQ(liveCount, 0);
}

TEST(SmallVector, DestructorRunsForAllElementsAfterSpill) {
    static int liveCount = 0;
    struct Tracker {
        Tracker() { liveCount++; }
        Tracker(const Tracker &other) {
            (void)other;
            liveCount++;
        }
        Tracker(Tracker &&other) noexcept {
            (void)other;
            liveCount++;
        }
        ~Tracker() { liveCount--; }
    };
    liveCount = 0;
    {
        SmallVector<Tracker, 2> v;
        for (int i = 0; i < 10; i++) {
            v.push_back(Tracker{});
        }
        EXPECT_EQ(liveCount, 10);
    }
    EXPECT_EQ(liveCount, 0);
}

TEST(SmallVector, PushBackInlineCopyKeepsValues) {
    SmallVector<int, 4> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST(SmallVector, PushBackInlineMoveTransfersOwnership) {
    SmallVector<std::unique_ptr<int>, 4> v;
    v.push_back(std::make_unique<int>(42));
    v.push_back(std::make_unique<int>(7));

    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(*v[0], 42);
    EXPECT_EQ(*v[1], 7);
}

TEST(SmallVector, PushBackBeyondInlineCapacitySpillsToHeap) {
    SmallVector<int, 2> v;
    for (int i = 0; i < 10; i++) {
        v.push_back(i);
    }

    EXPECT_EQ(v.size(), 10u);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(v[i], i);
    }
}

TEST(SmallVector, PushBackPreservesValuesAcrossInlineToHeapTransition) {
    SmallVector<std::string, 2> v;
    v.push_back("alpha");
    v.push_back("beta");
    v.push_back("gamma");
    v.push_back("delta");

    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], "alpha");
    EXPECT_EQ(v[1], "beta");
    EXPECT_EQ(v[2], "gamma");
    EXPECT_EQ(v[3], "delta");
}

TEST(SmallVector, ReserveBelowInlineIsNoOp) {
    SmallVector<int, 8> v;
    v.reserve(4);
    v.push_back(1);
    v.push_back(2);
    EXPECT_EQ(v.size(), 2u);
}

TEST(SmallVector, ReserveAboveInlineSpillsToHeap) {
    SmallVector<int, 2> v;
    v.reserve(100);
    for (int i = 0; i < 50; i++) {
        v.push_back(i);
    }
    EXPECT_EQ(v.size(), 50u);
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(v[i], i);
    }
}

TEST(SmallVector, ClearRunsDestructorsAndResetsSize) {
    static int liveCount = 0;
    struct Tracker {
        Tracker() { liveCount++; }
        Tracker(const Tracker &other) {
            (void)other;
            liveCount++;
        }
        Tracker(Tracker &&other) noexcept {
            (void)other;
            liveCount++;
        }
        ~Tracker() { liveCount--; }
    };
    liveCount = 0;
    SmallVector<Tracker, 4> v;
    v.push_back(Tracker{});
    v.push_back(Tracker{});
    EXPECT_EQ(liveCount, 2);

    v.clear();
    EXPECT_EQ(liveCount, 0);
    EXPECT_EQ(v.size(), 0u);
    EXPECT_TRUE(v.empty());
}

TEST(SmallVector, ClearAllowsReuse) {
    SmallVector<int, 2> v;
    v.push_back(1);
    v.push_back(2);
    v.clear();
    v.push_back(99);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 99);
}

TEST(SmallVector, MoveConstructFromInlineCopiesElementsToDestInline) {
    SmallVector<int, 4> src;
    src.push_back(1);
    src.push_back(2);

    const SmallVector<int, 4> dst(std::move(src));
    EXPECT_EQ(dst.size(), 2u);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[1], 2);
}

TEST(SmallVector, MoveConstructFromHeapTransfersPointerNoCopy) {
    SmallVector<std::unique_ptr<int>, 2> src;
    for (int i = 0; i < 10; i++) {
        src.push_back(std::make_unique<int>(i * 10));
    }

    auto *firstAddr = src[0].get();
    const SmallVector<std::unique_ptr<int>, 2> dst(std::move(src));

    EXPECT_EQ(dst.size(), 10u);
    EXPECT_EQ(dst[0].get(), firstAddr);
    EXPECT_EQ(*dst[3], 30);
}

TEST(SmallVector, MoveAssignReplacesPriorContents) {
    static int liveCount = 0;
    struct Tracker {
        Tracker() { liveCount++; }
        Tracker(const Tracker &other) {
            (void)other;
            liveCount++;
        }
        Tracker(Tracker &&other) noexcept {
            (void)other;
            liveCount++;
        }
        ~Tracker() { liveCount--; }
    };
    liveCount = 0;
    SmallVector<Tracker, 4> a;
    a.push_back(Tracker{});
    a.push_back(Tracker{});
    EXPECT_EQ(liveCount, 2);

    SmallVector<Tracker, 4> b;
    b.push_back(Tracker{});
    EXPECT_EQ(liveCount, 3);

    a = std::move(b);
    EXPECT_EQ(liveCount, 1);
    EXPECT_EQ(a.size(), 1u);
}

TEST(SmallVector, SelfMoveAssignmentIsSafe) {
    SmallVector<int, 4> v;
    v.push_back(1);
    v.push_back(2);

    SmallVector<int, 4> &alias = v;
    v = std::move(alias);

    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}

TEST(SmallVector, DataReturnsInlineAddressBelowCapacity) {
    SmallVector<int, 4> v;
    v.push_back(1);

    const auto *vAddr = reinterpret_cast<const std::byte *>(&v);
    const auto *dataAddr = reinterpret_cast<const std::byte *>(v.data());
    EXPECT_GE(dataAddr, vAddr);
    EXPECT_LT(dataAddr, vAddr + sizeof(v));
}

TEST(SmallVector, DataReturnsHeapAddressAfterSpill) {
    SmallVector<int, 2> v;
    for (int i = 0; i < 10; i++) {
        v.push_back(i);
    }

    const auto *vAddr = reinterpret_cast<const std::byte *>(&v);
    const auto *dataAddr = reinterpret_cast<const std::byte *>(v.data());
    EXPECT_TRUE(dataAddr < vAddr || dataAddr >= vAddr + sizeof(v));
}

TEST(SmallVector, RangeBasedForVisitsEveryElement) {
    SmallVector<int, 2> v;
    for (int i = 0; i < 5; i++) {
        v.push_back(i);
    }

    int sum = 0;
    for (const int x : v) {
        sum += x;
    }
    EXPECT_EQ(sum, 0 + 1 + 2 + 3 + 4);
}

TEST(SmallVector, BeginEndDistanceMatchesSize) {
    SmallVector<int, 4> v;
    v.push_back(10);
    v.push_back(20);
    EXPECT_EQ(v.end() - v.begin(), 2);
}

TEST(SmallVector, AlignmentForOverAlignedTypes) {
    struct alignas(64) Big {
        int x = 0;
    };
    SmallVector<Big, 2> v;
    v.push_back(Big{});
    v.push_back(Big{});

    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(v.data()) % alignof(Big), 0u);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(v.data() + 1) % alignof(Big), 0u);
}

TEST(SmallVector, MovedFromDestructorDoesNotDoubleDestroy) {
    static int liveCount = 0;
    struct Tracker {
        Tracker() { liveCount++; }
        Tracker(const Tracker &other) {
            (void)other;
            liveCount++;
        }
        Tracker(Tracker &&other) noexcept {
            (void)other;
            liveCount++;
        }
        ~Tracker() { liveCount--; }
    };
    liveCount = 0;
    {
        SmallVector<Tracker, 2> src;
        src.push_back(Tracker{});
        src.push_back(Tracker{});
        src.push_back(Tracker{});
        const int afterPush = liveCount;
        EXPECT_EQ(afterPush, 3);

        const SmallVector<Tracker, 2> dst(std::move(src));
        EXPECT_EQ(liveCount, afterPush);
    }
    EXPECT_EQ(liveCount, 0);
}

TEST(SmallVector, ReserveBelowCurrentCapacityDoesNotShrink) {
    SmallVector<int, 2> v;
    for (int i = 0; i < 20; i++) {
        v.push_back(i);
    }
    const int *dataBefore = v.data();

    v.reserve(4);

    EXPECT_EQ(v.size(), 20u);
    EXPECT_EQ(v.data(), dataBefore);
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(v[i], i);
    }
}

namespace {
    struct ThrowOnNthMove {
        static int totalMoves;
        static int throwAtMove;
        int value = 0;

        ThrowOnNthMove() = default;
        explicit ThrowOnNthMove(int v)
            : value(v) {}
        ThrowOnNthMove(const ThrowOnNthMove &other) = default;
        ThrowOnNthMove(ThrowOnNthMove &&other) noexcept(false)
            : value(other.value) {
            if (++totalMoves == throwAtMove) {
                throw std::runtime_error("scheduled move throw");
            }
            other.value = -1;
        }
        ThrowOnNthMove &operator=(const ThrowOnNthMove &other) = default;
        ThrowOnNthMove &operator=(ThrowOnNthMove &&other) noexcept(false) {
            if (this != &other) {
                value = other.value;
                if (++totalMoves == throwAtMove) {
                    throw std::runtime_error("scheduled move throw");
                }
                other.value = -1;
            }

            return *this;
        }
    };
    int ThrowOnNthMove::totalMoves = 0;
    int ThrowOnNthMove::throwAtMove = -1;
}

TEST(SmallVector, ThrowingMoveDuringGrowDoesNotCorruptCaller) {
    ThrowOnNthMove::totalMoves = 0;
    ThrowOnNthMove::throwAtMove = -1;

    SmallVector<ThrowOnNthMove, 2> v;
    v.push_back(ThrowOnNthMove{ 10 });
    v.push_back(ThrowOnNthMove{ 20 });

    ThrowOnNthMove::totalMoves = 0;
    ThrowOnNthMove::throwAtMove = 1;
    EXPECT_THROW(v.push_back(ThrowOnNthMove{ 30 }), std::runtime_error);

    ThrowOnNthMove::throwAtMove = -1;
}
