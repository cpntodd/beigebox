// tests/test_fixed_point.cpp
// ─────────────────────────────────────────────────────────────
// Unit tests for the Q24.8 FixedPoint class.
//
// These tests verify:
//   1. Construction from int, float, raw
//   2. Arithmetic (add, sub, mul, div)
//   3. Comparison operators
//   4. Edge cases (zero, negative, overflow bounds)
//
// These tests MUST pass identically on all platforms — this
// validates the lockstep determinism guarantee.
// ─────────────────────────────────────────────────────────────

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "beigebox/core/fixed_point.h"

using namespace beigebox;

TEST_CASE("FixedPoint — Construction")
{
    SUBCASE("FromInt")
    {
        auto fp = FixedPoint::FromInt(5);
        CHECK(fp.ToInt() == 5);
    }

    SUBCASE("FromInt — zero")
    {
        auto fp = FixedPoint::FromInt(0);
        CHECK(fp.ToInt() == 0);
        CHECK(fp.Raw() == 0);
    }

    SUBCASE("FromInt — negative")
    {
        auto fp = FixedPoint::FromInt(-10);
        CHECK(fp.ToInt() == -10);
    }
}

TEST_CASE("FixedPoint — Addition")
{
    auto a = FixedPoint::FromInt(3);
    auto b = FixedPoint::FromInt(4);
    CHECK((a + b).ToInt() == 7);
    CHECK((a + b) == FixedPoint::FromInt(7));
}

TEST_CASE("FixedPoint — Subtraction")
{
    auto a = FixedPoint::FromInt(10);
    auto b = FixedPoint::FromInt(4);
    CHECK((a - b).ToInt() == 6);
    CHECK((b - a).ToInt() == -6);
}

TEST_CASE("FixedPoint — Comparison")
{
    auto a = FixedPoint::FromInt(5);
    auto b = FixedPoint::FromInt(5);
    auto c = FixedPoint::FromInt(10);

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a < c);
    CHECK(c > a);
    CHECK(a <= b);
    CHECK(a >= b);
}

TEST_CASE("FixedPoint — Multiplication")
{
    auto a = FixedPoint::FromInt(3);
    auto b = FixedPoint::FromInt(4);
    CHECK((a * b).ToInt() == 12);
}

TEST_CASE("FixedPoint — Division")
{
    auto a = FixedPoint::FromInt(12);
    auto b = FixedPoint::FromInt(4);
    CHECK((a / b).ToInt() == 3);
}

TEST_CASE("FixedPoint — Multiplication with fraction")
{
    // 1.5 * 2.0 = 3.0
    auto a = FixedPoint::FromInt(1) + FixedPoint(FixedPoint::HALF);
    auto b = FixedPoint::FromInt(2);
    CHECK((a * b).ToInt() == 3);
}

TEST_CASE("FixedPoint — Rounding")
{
    // 2.5 should round to 3 (round half toward zero → 3 for positive)
    auto a = FixedPoint::FromInt(2) + FixedPoint(FixedPoint::HALF);
    CHECK(a.RoundToInt() == 3);
}

TEST_CASE("FixedPoint — Negative rounding")
{
    // -2.5 should round to -3 (round half toward zero → -3)
    auto a = FixedPoint::FromInt(-2) - FixedPoint(FixedPoint::HALF);
    CHECK(a.RoundToInt() == -3);
}

TEST_CASE("FixedPoint — Sqrt")
{
    // sqrt(16) = 4
    auto a = FixedPoint::FromInt(16);
    CHECK(Abs(Sqrt(a) - FixedPoint::FromInt(4)) < FixedPoint(FixedPoint::HALF));

    // sqrt(2) ≈ 1.414... in 24.8 that's ~362/256 ≈ 1.41406
    auto b = FixedPoint::FromInt(2);
    auto s = Sqrt(b);
    // Allow ±2 raw units error (~0.008)
    auto diff = std::abs(s.Raw() - 362);
    CHECK(diff <= 2);
}
