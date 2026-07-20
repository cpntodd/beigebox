// engine/include/beigebox/core/fixed_point.h
// ─────────────────────────────────────────────────────────────
// Q24.8 Fixed-Point Integer Type
//
// 24 bits integer, 8 bits fractional.
// Stored as int32_t. All operations are deterministic across
// compilers and architectures — safe for lockstep netcode.
//
// Range:  ±8,388,607.996  (approx ±8.3 million tiles)
// Precision: 1/256 ≈ 0.0039
// ─────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <string>

namespace beigebox {

class FixedPoint
{
public:
    using raw_type = int32_t;

    // ── Constants ────────────────────────────────────────────
    static constexpr int FRACTIONAL_BITS = 8;
    static constexpr raw_type ONE   = 1 << FRACTIONAL_BITS;   // 256
    static constexpr raw_type HALF  = 1 << (FRACTIONAL_BITS - 1); // 128
    static constexpr raw_type ZERO  = 0;

    // ── Construction ─────────────────────────────────────────
    constexpr FixedPoint() : value_(0) {}
    explicit constexpr FixedPoint(raw_type raw) : value_(raw) {}

    // ── Factory Methods ──────────────────────────────────────
    static constexpr FixedPoint FromInt(int n) { return FixedPoint(n << FRACTIONAL_BITS); }
    static FixedPoint FromFloat(float f);  // deterministic conversion
    static FixedPoint FromString(const std::string& s);

    // ── Conversion ───────────────────────────────────────────
    constexpr raw_type Raw() const { return value_; }
    float ToFloat() const;               // render only — not for gameplay
    int ToInt() const;                   // truncates toward zero
    int RoundToInt() const;              // rounds to nearest

    // ── Arithmetic Operators ─────────────────────────────────
    constexpr FixedPoint operator+(FixedPoint rhs) const { return FixedPoint(value_ + rhs.value_); }
    constexpr FixedPoint operator-(FixedPoint rhs) const { return FixedPoint(value_ - rhs.value_); }
    constexpr FixedPoint operator-() const { return FixedPoint(-value_); }

    FixedPoint operator*(FixedPoint rhs) const;
    FixedPoint operator/(FixedPoint rhs) const;

    constexpr FixedPoint& operator+=(FixedPoint rhs) { value_ += rhs.value_; return *this; }
    constexpr FixedPoint& operator-=(FixedPoint rhs) { value_ -= rhs.value_; return *this; }
    FixedPoint& operator*=(FixedPoint rhs);
    FixedPoint& operator/=(FixedPoint rhs);

    // ── Comparison Operators ─────────────────────────────────
    constexpr bool operator==(FixedPoint rhs) const { return value_ == rhs.value_; }
    constexpr bool operator!=(FixedPoint rhs) const { return value_ != rhs.value_; }
    constexpr bool operator< (FixedPoint rhs) const { return value_ <  rhs.value_; }
    constexpr bool operator<=(FixedPoint rhs) const { return value_ <= rhs.value_; }
    constexpr bool operator> (FixedPoint rhs) const { return value_ >  rhs.value_; }
    constexpr bool operator>=(FixedPoint rhs) const { return value_ >= rhs.value_; }

private:
    raw_type value_;
};

// ── Free Functions ───────────────────────────────────────────
FixedPoint Abs(FixedPoint fp);
FixedPoint Sqrt(FixedPoint fp);   // integer sqrt (deterministic)

} // namespace beigebox
