// engine/include/beigebox/ai/pathfinding.h
// ─────────────────────────────────────────────────────────────
// A* Pathfinding on the spatial grid.
//
// Uses the SpatialGrid for fast neighbor lookups. FixedPoint
// distances for deterministic results. Operates on tile
// coordinates with terrain cost weights.
//
// Thread-safe: can be called from any thread with a const
// reference to the spatial grid.
// ─────────────────────────────────────────────────────────────
#pragma once

#include "beigebox/world/spatial_grid.h"
#include "beigebox/core/fixed_point.h"

#include <vector>
#include <unordered_map>
#include <queue>
#include <functional>

namespace beigebox {

// ── Path node ────────────────────────────────────────────────
struct PathNode
{
    int x, y;
    int gCost = 0; // cost from start
    int hCost = 0; // heuristic to goal
    int fCost() const { return gCost + hCost; }
    int parentIdx = -1;

    bool operator>(const PathNode& other) const {
        return fCost() > other.fCost();
    }
};

// ── Path result ─────────────────────────────────────────────
struct PathResult
{
    bool found = false;
    std::vector<std::pair<int, int>> waypoints; // tile coords from start to goal
    int nodesExplored = 0;
};

class Pathfinder
{
public:
    // ── Terrain cost callback ────────────────────────────────
    // Returns cost multiplier for a tile (256 = 1.0x in FixedPoint).
    // Return -1 to mark as impassable.
    using TerrainCostFn = std::function<int(int tileX, int tileY)>;

    Pathfinder() = default;

    // Find a path from (sx,sy) to (gx,gy) on the given grid.
    // terrainCost: callback to query passability and movement cost.
    // Returns waypoints including start and goal.
    static PathResult FindPath(
        int sx, int sy, int gx, int gy,
        const SpatialGrid& grid,
        TerrainCostFn terrainCost);

private:
    // Hash pair for closed set
    struct PairHash {
        size_t operator()(const std::pair<int,int>& p) const {
            return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 1);
        }
    };

    static int Heuristic(int x1, int y1, int x2, int y2);
    static std::vector<std::pair<int,int>> ReconstructPath(
        const std::vector<PathNode>& nodes, int goalIdx);
};

} // namespace beigebox
