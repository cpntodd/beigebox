// engine/src/render/isometric.cpp
// ─────────────────────────────────────────────────────────────
// Isometric coordinate conversion implementations.
// ─────────────────────────────────────────────────────────────

#include "beigebox/render/isometric.h"

namespace beigebox {

void WorldToScreen(int tileX, int tileY, int& screenX, int& screenY)
{
    // Standard 2:1 dimetric projection:
    //   screen_x = (tile_x - tile_y) * half_width
    //   screen_y = (tile_x + tile_y) * half_height
    screenX = (tileX - tileY) * TILE_HW;
    screenY = (tileX + tileY) * TILE_HH;
}

void ScreenToWorld(int screenX, int screenY, int& tileX, int& tileY)
{
    // Inverse of the above:
    //   tile_x = (screen_x / half_width  + screen_y / half_height) / 2
    //   tile_y = (screen_y / half_height - screen_x / half_width ) / 2
    //
    // Using integer division (truncates toward zero — fine for tile picking).
    int rx = screenX / TILE_HW;
    int ry = screenY / TILE_HH;

    tileX = (rx + ry) / 2;
    tileY = (ry - rx) / 2;
}

void VisibleTiles(int screenW, int screenH, int& tilesX, int& tilesY)
{
    // Each tile diamond spans TILE_W × TILE_H pixels.
    // Horizontally: tiles are spaced TILE_HW (32px) apart.
    // Vertically: tiles are spaced TILE_HH (16px) apart.
    // Add 2 extra tiles for overscan margin.
    tilesX = (screenW / TILE_HW) + 2;
    tilesY = (screenH / TILE_HH) + 2;
}

} // namespace beigebox
