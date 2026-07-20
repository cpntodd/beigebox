// engine/include/beigebox/render/isometric.h
// ─────────────────────────────────────────────────────────────
// Isometric coordinate conversion utilities.
//
// Converts between world-space (tile grid coordinates) and
// screen-space (pixel coordinates) using standard 2:1 dimetric
// projection.
//
// World space:
//   +X → right-down (southeast on screen)
//   +Y → left-down  (southwest on screen)
//
// Tile dimensions: 64px wide × 32px tall (diamond)
// ─────────────────────────────────────────────────────────────
#pragma once

namespace beigebox {

// Tile diamond half-extents in pixels (2:1 ratio)
constexpr int TILE_W = 64;   // full width of diamond base
constexpr int TILE_H = 32;   // full height of diamond
constexpr int TILE_HW = 32;  // half-width
constexpr int TILE_HH = 16;  // half-height

// Convert tile grid coordinates to screen pixel coordinates.
// Returns the top-corner of the diamond.
void WorldToScreen(int tileX, int tileY, int& screenX, int& screenY);

// Convert screen pixel coordinates to tile grid coordinates.
// Returns the tile under the cursor (integer truncation).
void ScreenToWorld(int screenX, int screenY, int& tileX, int& tileY);

} // namespace beigebox
