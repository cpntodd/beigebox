// tests/test_thaw.cpp
// ─────────────────────────────────────────────────────────────
// Unit tests for the ThawGrid system.
// ─────────────────────────────────────────────────────────────

#include <doctest/doctest.h>

#include "beigebox/world/thaw_grid.h"
#include "beigebox/core/fixed_point.h"

using namespace beigebox;

TEST_CASE("ThawGrid — Init")
{
    ThawGrid grid;
    grid.Init(16, 16);

    CHECK(grid.Width() == 16);
    CHECK(grid.Height() == 16);

    // All tiles should start frozen with 0 heat
    CHECK(grid.IsFrozen(0, 0));
    CHECK(grid.GetHeat(0, 0) == 0);
    CHECK(!grid.IsBuildable(0, 0));
}

TEST_CASE("ThawGrid — Heat source thaws tiles over time")
{
    ThawGrid grid;
    grid.Init(10, 10);

    // Add a strong heat source at center
    grid.AddHeatSource(5, 5, FixedPoint::FromInt(3), FixedPoint::FromInt(100));

    // Run many ticks to thaw the area
    for (int i = 0; i < 200; ++i)
        grid.Tick();

    // Center tile should be thawed
    CHECK(!grid.IsFrozen(5, 5));
    CHECK(grid.IsBuildable(5, 5));
    CHECK(grid.GetHeat(5, 5) >= kThawThreshold);
}

TEST_CASE("ThawGrid — Out of bounds queries are safe")
{
    ThawGrid grid;
    grid.Init(10, 10);

    CHECK(grid.IsFrozen(-1, -1));
    CHECK(grid.IsFrozen(100, 100));
    CHECK(grid.GetHeat(-1, 0) == 0);
}

TEST_CASE("ThawGrid — Heat source removal")
{
    ThawGrid grid;
    grid.Init(10, 10);

    uint32_t id = grid.AddHeatSource(5, 5, FixedPoint::FromInt(2), FixedPoint::FromInt(50));
    CHECK(id > 0);

    grid.RemoveHeatSource(id);

    // Run ticks — nothing should heat up significantly
    for (int i = 0; i < 100; ++i)
        grid.Tick();

    // Should still be mostly frozen (some residual heat diffusion)
    CHECK(grid.IsFrozen(5, 5));
}
