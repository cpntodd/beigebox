// engine/include/beigebox/world/spatial_grid.h
// ─────────────────────────────────────────────────────────────
// Spatial partitioning grid for O(1) tile lookups.
//
// Divides the map into fixed-size chunks (default 16×16 tiles).
// Systems that scan all tiles for all units (FogOfWarSystem,
// CombatSystem proximity checks) can query only relevant chunks
// instead of iterating the entire map.
//
// Reduces FogOfWarSystem from O(units × tiles) to
// O(units × chunks_in_range).
// ─────────────────────────────────────────────────────────────
#pragma once

#include <vector>
#include <cstdint>

namespace beigebox {

class SpatialGrid
{
public:
    static constexpr int kChunkSize = 16; // tiles per chunk edge

    struct Chunk
    {
        int tileX, tileY;          // top-left tile of this chunk
        std::vector<int> indices;  // tile indices within this chunk
    };

    SpatialGrid() = default;

    // Initialize for a map of the given dimensions.
    void Init(int mapWidth, int mapHeight);

    // Get the chunk containing the given tile.
    const Chunk* GetChunk(int tileX, int tileY) const;

    // Get all chunks within a given radius of a tile.
    void GetChunksInRadius(int centerX, int centerY, int radius,
                           std::vector<const Chunk*>& outChunks) const;

    // Add a tile to its chunk. Call when tiles change.
    void UpdateTile(int tileX, int tileY, int tileIndex);

    int MapWidth()  const { return mapW_; }
    int MapHeight() const { return mapH_; }
    int ChunkCountX() const { return chunksX_; }
    int ChunkCountY() const { return chunksY_; }

private:
    int mapW_ = 0, mapH_ = 0;
    int chunksX_ = 0, chunksY_ = 0;
    std::vector<Chunk> chunks_;

    int ChunkIndex(int cx, int cy) const {
        return cy * chunksX_ + cx;
    }
};

} // namespace beigebox
