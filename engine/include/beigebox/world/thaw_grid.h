// engine/include/beigebox/world/thaw_grid.h
// ─────────────────────────────────────────────────────────────
// Thaw Grid — 2D heat map for the permafrost mechanic.
//
// Tiles have a heat value (0–255). Heat propagates from sources
// outward each tick. When a tile reaches a thaw threshold, it
// transitions from Frozen → Thawing → Thawed.
//
// All math is integer-based for determinism and performance.
// No floating-point in the heat propagation logic.
//
// Memory: for a 256×256 map, the grid is 256×256×2 bytes
// (current heat + tile state) ≈ 128 KB — fits in L2 cache.
// ─────────────────────────────────────────────────────────────
#pragma once

#include "beigebox/core/fixed_point.h"

#include <cstdint>
#include <vector>

namespace beigebox {

// ── Tile State ───────────────────────────────────────────────
enum class TileState : uint8_t
{
    Frozen  = 0,  // impassable, blocks construction
    Thawing = 1,  // transitioning — countdown in progress
    Thawed  = 2   // passable, buildable
};

static constexpr uint8_t kMaxHeat      = 255;
static constexpr uint8_t kThawThreshold = 200;  // heat level to trigger thawing
static constexpr int     kThawDuration  = 60;    // ticks to complete thaw (2 sec @ 30 Hz)

// ── ThawGrid ─────────────────────────────────────────────────
class ThawGrid
{
public:
    ThawGrid() = default;

    // Initialize a grid of the given tile dimensions.
    // All tiles start as Frozen with 0 heat.
    void Init(int width, int height);

    // ── Heat Propagation ─────────────────────────────────────
    //
    // Call once per logic tick. Spreads heat from all active
    // sources outward using a simple convolution kernel.
    // Also updates tile state transitions.
    void Tick();

    // Add a heat source at a tile position.
    // Returns a source ID for later removal.
    uint32_t AddHeatSource(int tileX, int tileY, FixedPoint radius, FixedPoint intensity);

    // Remove a previously added heat source.
    void RemoveHeatSource(uint32_t sourceId);

    // ── Tile Queries ─────────────────────────────────────────
    uint8_t   GetHeat(int tileX, int tileY) const;
    TileState GetState(int tileX, int tileY) const;
    bool      IsFrozen(int tileX, int tileY) const;
    bool      IsBuildable(int tileX, int tileY) const;

    // ── Dimensions ───────────────────────────────────────────
    int Width()  const { return width_; }
    int Height() const { return height_; }

private:
    struct HeatSourceData {
        int         tileX, tileY;
        FixedPoint  radius;     // Q24.8 tile units
        FixedPoint  intensity;  // heat per tick at center
        uint32_t    id;
    };

    // Heat buffer (double-buffered for propagation)
    std::vector<uint8_t> heat_[2];    // current, next
    int                  activeBuffer_ = 0;

    // Tile state + thaw countdown
    std::vector<uint8_t> state_;      // TileState per tile
    std::vector<int16_t> thawTimer_;  // countdown ticks remaining

    std::vector<HeatSourceData> sources_;
    uint32_t nextSourceId_ = 1;

    int width_  = 0;
    int height_ = 0;

    // Helpers
    int  Index(int x, int y) const { return y * width_ + x; }
    bool InBounds(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }
};

} // namespace beigebox
