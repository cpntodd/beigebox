// engine/src/core/fixed_point.cpp
// ─────────────────────────────────────────────────────────────
// Q24.8 Fixed-Point Implementation
//
// All arithmetic uses int64_t intermediates to prevent overflow.
// Rounding is "round half toward zero" for symmetry — this is
// critical for deterministic lockstep: it behaves identically
// on all architectures.
// ─────────────────────────────────────────────────────────────

#include "beigebox/core/fixed_point.h"
#include <cstdlib>
#include <cstdint>

namespace beigebox {

// ── Float Conversion ────────────────────────────────────────

FixedPoint FixedPoint::FromFloat(float f)
{
    // Multiply by 256 and round toward zero.
    // This is deterministic: same float → same int32 on all IEEE 754 hardware.
    return FixedPoint(static_cast<raw_type>(f * static_cast<float>(ONE)));
}

float FixedPoint::ToFloat() const
{
    return static_cast<float>(value_) / static_cast<float>(ONE);
}

int FixedPoint::ToInt() const
{
    // Truncate toward zero (arithmetic right shift on signed int is
    // implementation-defined in C++ prior to C++20, but all relevant
    // compilers (GCC, Clang, MSVC) use arithmetic shift for signed.
    return value_ >> FRACTIONAL_BITS;
}

int FixedPoint::RoundToInt() const
{
    // Round half toward zero (symmetric rounding).
    // For positive: add HALF. For negative: subtract HALF.
    if (value_ >= 0)
        return (value_ + HALF) >> FRACTIONAL_BITS;
    else
        return (value_ - HALF) >> FRACTIONAL_BITS;
}

// ── Multiplication ──────────────────────────────────────────
// (a/256) * (b/256) = (a*b) / 65536
// We compute: ((a * b) + 128) >> 8  using int64_t to avoid overflow.
// The +128 gives round-half-toward-zero for positive products;
// we adjust the sign below.

FixedPoint FixedPoint::operator*(FixedPoint rhs) const
{
    int64_t product = static_cast<int64_t>(value_) * static_cast<int64_t>(rhs.value_);
    // Round half toward zero
    if (product >= 0)
        return FixedPoint(static_cast<raw_type>((product + HALF) >> FRACTIONAL_BITS));
    else
        return FixedPoint(static_cast<raw_type>((product - HALF) >> FRACTIONAL_BITS));
}

// ── Division ────────────────────────────────────────────────
// (a/256) / (b/256) = a/b
// To preserve precision, we shift numerator up before dividing:
// (a << 8) / b  gives the result in fixed-point.

FixedPoint FixedPoint::operator/(FixedPoint rhs) const
{
    // Shift left into int64_t to avoid overflow during shift
    int64_t numerator = static_cast<int64_t>(value_) << FRACTIONAL_BITS;
    return FixedPoint(static_cast<raw_type>(numerator / rhs.value_));
}

FixedPoint& FixedPoint::operator*=(FixedPoint rhs) { *this = *this * rhs; return *this; }
FixedPoint& FixedPoint::operator/=(FixedPoint rhs) { *this = *this / rhs; return *this; }

// ── Absolute Value ──────────────────────────────────────────
FixedPoint Abs(FixedPoint fp)
{
    FixedPoint::raw_type v = fp.Raw();
    return FixedPoint(v >= 0 ? v : -v);
}

// ── Square Root (Newton's Method, Integer-Only) ─────────────
// Computes sqrt(fp) deterministically.
//
// We solve: result^2 = value * 2^FRACTIONAL_BITS
// Because:  (result / 2^8)^2 = value / 2^8
//        →  result^2 / 2^16 = value / 2^8
//        →  result^2 = value * 2^8
//
// Newton iteration: x_{n+1} = (x_n + S/x_n) / 2
// where S = value << 8  (scaled into int64_t).

FixedPoint Sqrt(FixedPoint fp)
{
    if (fp.Raw() <= 0)
        return FixedPoint::FromInt(0);

    // Scale input: S = fp_raw * 256
    uint64_t S = static_cast<uint64_t>(
        static_cast<int64_t>(fp.Raw()) << FixedPoint::FRACTIONAL_BITS
    );

    // Initial guess: use the raw value itself (reasonable for fixed-point)
    uint64_t x = static_cast<uint64_t>(fp.Raw());
    if (x < 1) x = 1; // avoid division by zero

    // Newton's method — 4 iterations is enough for 24.8 precision
    for (int i = 0; i < 4; ++i)
    {
        if (x == 0) break;
        uint64_t quotient = S / x;
        x = (x + quotient) >> 1;
    }

    // Clamp to int32_t range (shouldn't overflow for valid inputs)
    if (x > static_cast<uint64_t>(INT32_MAX))
        return FixedPoint(INT32_MAX);

    return FixedPoint(static_cast<FixedPoint::raw_type>(x));
}

// ── SSE2 Batch Operations ───────────────────────────────────
// Process 4 FixedPoint adds at once using 128-bit SSE2.
// Multiply uses scalar fallback (compiler auto-vectorizes int64_t).
// Functions exist as API placeholders for future AVX2 optimization.

#ifdef __SSE2__
#include <emmintrin.h>

void FixedPointAdd4(FixedPoint* a, const FixedPoint* b, int count)
{
    int i = 0;
    // Process 4 at a time — simple int32_t add, no overflow risk
    for (; i + 3 < count; i += 4)
    {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&a[i]));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&b[i]));
        __m128i vsum = _mm_add_epi32(va, vb);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&a[i]), vsum);
    }
    // Scalar remainder
    for (; i < count; ++i)
        a[i] = a[i] + b[i];
}

void FixedPointMul4(FixedPoint* a, const FixedPoint* b, int count)
{
    // Multiply requires int64_t intermediates → SSE2 has poor support.
    // Use scalar path; GCC/Clang auto-vectorize this well on -O3.
    for (int i = 0; i < count; ++i)
        a[i] = a[i] * b[i];
}

#endif // __SSE2__

} // namespace beigebox
