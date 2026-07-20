// engine/src/world/thaw_grid.cpp
// ─────────────────────────────────────────────────────────────
// Thaw Grid implementation — heat propagation and tile state.
// ─────────────────────────────────────────────────────────────

#include "beigebox/world/thaw_grid.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace beigebox {

// ── Lifecycle ────────────────────────────────────────────────

void ThawGrid::Init(int width, int height)
{
    width_  = width;
    height_ = height;
    int size = width_ * height_;

    heat_[0].assign(size, 0);
    heat_[1].assign(size, 0);
    state_.assign(size, static_cast<uint8_t>(TileState::Frozen));
    thawTimer_.assign(size, 0);
    sources_.clear();
    activeBuffer_ = 0;
    nextSourceId_ = 1;
}

// ── Heat Propagation ─────────────────────────────────────────
//
// Each tick:
//   1. Apply heat sources to the grid
//   2. Diffuse heat to neighbors (simple 4-way kernel)
//   3. Decay heat slightly (simulates cooling)
//   4. Update tile state transitions
//   5. Swap buffers
//
// The propagation uses a simple box-blur-style diffusion:
//   new_heat[tile] = 0.5 * old_heat[tile] + 0.125 * sum(neighbors)
//
// All math is integer with bit-shifts for performance and determinism.

void ThawGrid::Tick()
{
    auto& current = heat_[activeBuffer_];
    auto& next    = heat_[1 - activeBuffer_];

    // ── Step 1: Apply heat sources ───────────────────────────
    for (const auto& src : sources_)
    {
        int radiusTiles = src.radius.ToInt();
        if (radiusTiles < 1) radiusTiles = 1;

        // Simple falloff: intensity at center, 0 at radius edge
        int intensityAtCenter = src.intensity.ToInt();
        if (intensityAtCenter <= 0) continue;

        for (int dy = -radiusTiles; dy <= radiusTiles; ++dy)
        {
            for (int dx = -radiusTiles; dx <= radiusTiles; ++dx)
            {
                int tx = src.tileX + dx;
                int ty = src.tileY + dy;
                if (!InBounds(tx, ty)) continue;

                // Linear falloff based on Chebyshev distance
                int dist = std::max(std::abs(dx), std::abs(dy));
                int heatAdd = intensityAtCenter * (radiusTiles - dist) / radiusTiles;
                if (heatAdd > 0)
                {
                    int idx = Index(tx, ty);
                    int newVal = current[idx] + heatAdd;
                    current[idx] = static_cast<uint8_t>(std::min(newVal, static_cast<int>(kMaxHeat)));
                }
            }
        }
    }

    // ── Step 2: Diffuse heat (4-way kernel, integer math) ────
    for (int y = 0; y < height_; ++y)
    {
        for (int x = 0; x < width_; ++x)
        {
            int idx = Index(x, y);
            int self = current[idx];

            // Sum neighbors
            int neighborSum = 0;
            int neighborCount = 0;
            auto addNeighbor = [&](int nx, int ny) {
                if (InBounds(nx, ny)) {
                    neighborSum += current[Index(nx, ny)];
                    ++neighborCount;
                }
            };
            addNeighbor(x + 1, y);
            addNeighbor(x - 1, y);
            addNeighbor(x, y + 1);
            addNeighbor(x, y - 1);

            // Diffusion: self * 4 + neighborSum, then /8
            // Equivalent to 0.5 * self + 0.5 * avg(neighbors)
            int newHeat = (self * 4 + neighborSum) / (4 + neighborCount);

            // ── Step 3: Decay ────────────────────────────────
            // Lose 1 heat per tick (simulates ambient cooling)
            newHeat = std::max(0, newHeat - 1);

            next[idx] = static_cast<uint8_t>(std::min(newHeat, static_cast<int>(kMaxHeat)));
        }
    }

    // ── Step 4: Update tile state transitions ────────────────
    for (int i = 0; i < width_ * height_; ++i)
    {
        auto state = static_cast<TileState>(state_[i]);

        switch (state)
        {
        case TileState::Frozen:
            if (next[i] >= kThawThreshold)
            {
                state_[i] = static_cast<uint8_t>(TileState::Thawing);
                thawTimer_[i] = kThawDuration;
            }
            break;

        case TileState::Thawing:
            thawTimer_[i]--;
            if (thawTimer_[i] <= 0)
            {
                state_[i] = static_cast<uint8_t>(TileState::Thawed);
                next[i] = kMaxHeat; // fully thawed = max heat
            }
            break;

        case TileState::Thawed:
            // Once thawed, heat stays high (geothermal equilibrium)
            next[i] = std::max(next[i], kThawThreshold);
            break;
        }
    }

    // ── Step 5: Swap buffers ─────────────────────────────────
    activeBuffer_ = 1 - activeBuffer_;
}

// ── Source Management ────────────────────────────────────────

uint32_t ThawGrid::AddHeatSource(int tileX, int tileY,
                                  FixedPoint radius, FixedPoint intensity)
{
    uint32_t id = nextSourceId_++;
    sources_.push_back({tileX, tileY, radius, intensity, id});
    return id;
}

void ThawGrid::RemoveHeatSource(uint32_t sourceId)
{
    sources_.erase(
        std::remove_if(sources_.begin(), sources_.end(),
            [sourceId](const HeatSourceData& s) { return s.id == sourceId; }),
        sources_.end());
}

// ── Tile Queries ─────────────────────────────────────────────

uint8_t ThawGrid::GetHeat(int tileX, int tileY) const
{
    if (!InBounds(tileX, tileY)) return 0;
    return heat_[activeBuffer_][Index(tileX, tileY)];
}

TileState ThawGrid::GetState(int tileX, int tileY) const
{
    if (!InBounds(tileX, tileY)) return TileState::Frozen;
    return static_cast<TileState>(state_[Index(tileX, tileY)]);
}

bool ThawGrid::IsFrozen(int tileX, int tileY) const
{
    return GetState(tileX, tileY) == TileState::Frozen;
}

bool ThawGrid::IsBuildable(int tileX, int tileY) const
{
    return GetState(tileX, tileY) == TileState::Thawed;
}

} // namespace beigebox
