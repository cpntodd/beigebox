// engine/src/ai/pathfinding.cpp
// ─────────────────────────────────────────────────────────────
// A* Pathfinding implementation.
// ─────────────────────────────────────────────────────────────

#include "beigebox/ai/pathfinding.h"
#include <cmath>
#include <algorithm>

namespace beigebox {

int Pathfinder::Heuristic(int x1, int y1, int x2, int y2)
{
    // Octile distance for isometric grids (diagonal movement allowed)
    int dx = std::abs(x1 - x2);
    int dy = std::abs(y1 - y2);
    // Cost: straight = 10, diagonal = 14 (approx sqrt(2)*10)
    return 10 * std::max(dx, dy) + 4 * std::min(dx, dy);
}

std::vector<std::pair<int,int>> Pathfinder::ReconstructPath(
    const std::vector<PathNode>& nodes, int goalIdx)
{
    std::vector<std::pair<int,int>> path;
    int idx = goalIdx;
    while (idx >= 0)
    {
        path.push_back({nodes[idx].x, nodes[idx].y});
        idx = nodes[idx].parentIdx;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

PathResult Pathfinder::FindPath(
    int sx, int sy, int gx, int gy,
    const SpatialGrid& grid,
    TerrainCostFn terrainCost)
{
    PathResult result;

    // Validate start and goal
    if (sx < 0 || sx >= grid.MapWidth() || sy < 0 || sy >= grid.MapHeight())
        return result;
    if (gx < 0 || gx >= grid.MapWidth() || gy < 0 || gy >= grid.MapHeight())
        return result;

    // Check passability
    int startCost = terrainCost(sx, sy);
    int goalCost = terrainCost(gx, gy);
    if (startCost < 0 || goalCost < 0) return result; // impassable

    // Priority queue: lowest fCost first
    std::priority_queue<PathNode, std::vector<PathNode>, std::greater<PathNode>> openSet;
    std::unordered_map<std::pair<int,int>, int, PairHash> closedSet;

    std::vector<PathNode> allNodes;
    allNodes.reserve(1024);

    // Start node
    PathNode start;
    start.x = sx; start.y = sy;
    start.gCost = 0;
    start.hCost = Heuristic(sx, sy, gx, gy);
    start.parentIdx = -1;
    openSet.push(start);
    allNodes.push_back(start);

    // 8-directional movement (including diagonals)
    const int dirs[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };
    const int moveCost[8] = {14, 10, 14, 10, 10, 14, 10, 14}; // diagonal costs more

    const int maxNodes = 4096; // safety limit for potato PCs
    int nodesExplored = 0;

    while (!openSet.empty() && nodesExplored < maxNodes)
    {
        PathNode current = openSet.top();
        openSet.pop();
        nodesExplored++;

        // Check if reached goal
        if (current.x == gx && current.y == gy)
        {
            result.found = true;
            result.waypoints = ReconstructPath(allNodes, static_cast<int>(allNodes.size()) - 1);
            result.nodesExplored = nodesExplored;
            return result;
        }

        // Skip if already processed
        auto key = std::make_pair(current.x, current.y);
        if (closedSet.count(key)) continue;
        closedSet[key] = current.gCost;

        // Explore neighbors
        for (int d = 0; d < 8; ++d)
        {
            int nx = current.x + dirs[d][0];
            int ny = current.y + dirs[d][1];

            if (nx < 0 || nx >= grid.MapWidth() || ny < 0 || ny >= grid.MapHeight())
                continue;

            int tCost = terrainCost(nx, ny);
            if (tCost < 0) continue; // impassable

            auto nkey = std::make_pair(nx, ny);
            if (closedSet.count(nkey)) continue;

            int newGCost = current.gCost + (moveCost[d] * tCost) / 256;

            PathNode neighbor;
            neighbor.x = nx; neighbor.y = ny;
            neighbor.gCost = newGCost;
            neighbor.hCost = Heuristic(nx, ny, gx, gy);
            neighbor.parentIdx = static_cast<int>(allNodes.size()) - 1; // index of current

            openSet.push(neighbor);
            allNodes.push_back(neighbor);
        }
    }

    result.nodesExplored = nodesExplored;
    return result; // no path found
}

} // namespace beigebox
