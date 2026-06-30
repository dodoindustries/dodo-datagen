#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <limits>

#include "dodo/rng.hpp"

using dodo::Rng;

TEST_CASE("same seed produces an identical stream") {
    Rng a(12345);
    Rng b(12345);
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.next_u64() == b.next_u64());
    }
}

TEST_CASE("different seeds diverge") {
    Rng a(1);
    Rng b(2);
    bool any_difference = false;
    for (int i = 0; i < 10; ++i) {
        if (a.next_u64() != b.next_u64()) {
            any_difference = true;
        }
    }
    CHECK(any_difference);
}

TEST_CASE("next_int stays within the inclusive range") {
    Rng rng(99);
    for (int i = 0; i < 100000; ++i) {
        std::int64_t value = rng.next_int(18, 90);
        CHECK(value >= 18);
        CHECK(value <= 90);
    }
}

TEST_CASE("next_int with a single-value range is constant") {
    Rng rng(7);
    for (int i = 0; i < 100; ++i) {
        CHECK(rng.next_int(5, 5) == 5);
    }
}

TEST_CASE("next_double is in [0, 1)") {
    Rng rng(2024);
    for (int i = 0; i < 100000; ++i) {
        double value = rng.next_double();
        CHECK(value >= 0.0);
        CHECK(value < 1.0);
    }
}

TEST_CASE("next_bool honours the extremes") {
    Rng rng(55);
    for (int i = 0; i < 1000; ++i) {
        CHECK(rng.next_bool(1.0));
        CHECK_FALSE(rng.next_bool(0.0));
    }
}

TEST_CASE("next_int over the full int64 range is safe") {
    Rng rng(123);
    const std::int64_t lo = std::numeric_limits<std::int64_t>::min();
    const std::int64_t hi = std::numeric_limits<std::int64_t>::max();
    for (int i = 0; i < 10000; ++i) {
        std::int64_t v = rng.next_int(lo, hi); // must not overflow or divide by zero
        CHECK(v >= lo);
        CHECK(v <= hi);
    }
}

TEST_CASE("next_index(0) does not divide by zero") {
    Rng rng(1);
    CHECK(rng.next_index(0) == 0);
}
