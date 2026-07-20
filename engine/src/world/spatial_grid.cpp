// engine/src/world/spatial_grid.cpp
// ─────────────────────────────────────────────────────────────
// Spatial grid implementation.
// ─────────────────────────────────────────────────────────────

#include "beigebox/world/spatial_grid.h"
#include <algorithm>

namespace beigebox {

void SpatialGrid::Init(int mapWidth, int mapHeight)
{
    mapW_ = mapWidth;
    mapH_ = mapHeight;
    chunksX_ = (mapW_ + kChunkSize - 1) / kChunkSize;
    chunksY_ = (mapH_ + kChunkSize - 1) / kChunkSize;

    chunks_.resize(chunksX_ * chunksY_);

    for (int cy = 0; cy < chunksY_; ++cy)
    {
        for (int cx = 0; cx < chunksX_; ++cx)
        {
            auto& chunk = chunks_[ChunkIndex(cx, cy)];
            chunk.tileX = cx * kChunkSize;
            chunk.tileY = cy * kChunkSize;
            chunk.indices.reserve(kChunkSize * kChunkSize);
        }
    }
}

const SpatialGrid::Chunk* SpatialGrid::GetChunk(int tileX, int tileY) const
{
    if (tileX < 0 || tileX >= mapW_ || tileY < 0 || tileY >= mapH_)
        return nullptr;

    int cx = tileX / kChunkSize;
    int cy = tileY / kChunkSize;
    return &chunks_[ChunkIndex(cx, cy)];
}

void SpatialGrid::GetChunksInRadius(int centerX, int centerY, int radius,
    std::vector<const Chunk*>& outChunks) const
{
    outChunks.clear();

    int minCx = std::max(0, (centerX - radius) / kChunkSize);
    int maxCx = std::min(chunksX_ - 1, (centerX + radius) / kChunkSize);
    int minCy = std::max(0, (centerY - radius) / kChunkSize);
    int maxCy = std::min(chunksY_ - 1, (centerY + radius) / kChunkSize);

    for (int cy = minCy; cy <= maxCy; ++cy)
        for (int cx = minCx; cx <= maxCx; ++cx)
            outChunks.push_back(&chunks_[ChunkIndex(cx, cy)]);
}

void SpatialGrid::UpdateTile(int tileX, int tileY, int tileIndex)
{
    const auto* chunk = GetChunk(tileX, tileY);
    if (!chunk) return;

    // Const-cast: UpdateTile is the intended mutation path
    auto* mutChunk = const_cast<Chunk*>(chunk);
    // Avoid duplicates
    auto it = std::find(mutChunk->indices.begin(), mutChunk->indices.end(), tileIndex);
    if (it == mutChunk->indices.end())
        mutChunk->indices.push_back(tileIndex);
}

} // namespace beigebox
