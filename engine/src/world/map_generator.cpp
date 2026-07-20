// engine/src/world/map_generator.cpp
// ─────────────────────────────────────────────────────────────
// Procedural tile map generator + .ogm file I/O.
// ─────────────────────────────────────────────────────────────

#include "beigebox/world/map_generator.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace beigebox {

// ── Simple deterministic PRNG (xorshift32) ───────────────────

static uint32_t g_randState = 42;

static void SeedRand(uint32_t seed) { g_randState = seed ? seed : 1; }

static uint32_t Rand()
{
    uint32_t x = g_randState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_randState = x;
    return x;
}

static float RandFloat() { return static_cast<float>(Rand()) / static_cast<float>(UINT32_MAX); }

// ── Generation ────────────────────────────────────────────────

void MapGenerator::Generate(MapTile* tiles, const Params& params)
{
    SeedRand(static_cast<uint32_t>(params.seed));

    int totalTiles = params.width * params.height;

    // Step 1: All tiles start as FrozenGround
    for (int i = 0; i < totalTiles; ++i)
    {
        tiles[i].terrain = TileTerrain::FrozenGround;
        tiles[i].resourceAmount = 0;
        tiles[i].flags = 0;
    }

    // Step 2: Place some ThawedGround patches (clearings)
    int numClearings = std::max(1, totalTiles / 50);
    for (int i = 0; i < numClearings; ++i)
    {
        int cx = static_cast<int>(Rand() % params.width);
        int cy = static_cast<int>(Rand() % params.height);
        int radius = 2 + static_cast<int>(Rand() % 4);

        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                int tx = cx + dx;
                int ty = cy + dy;
                if (tx >= 0 && tx < params.width && ty >= 0 && ty < params.height)
                {
                    int dist = std::max(std::abs(dx), std::abs(dy));
                    if (dist <= radius)
                        tiles[ty * params.width + tx].terrain = TileTerrain::ThawedGround;
                }
            }
        }
    }

    // Step 3: Place salvage fields on some thawed tiles
    for (int i = 0; i < totalTiles; ++i)
    {
        if (tiles[i].terrain == TileTerrain::ThawedGround && RandFloat() < params.salvageDensity)
        {
            tiles[i].terrain = TileTerrain::SalvageField;
            tiles[i].resourceAmount = static_cast<uint8_t>(50 + Rand() % 200);
        }
    }

    // Step 4: Place geothermal vents
    for (int i = 0; i < totalTiles; ++i)
    {
        if ((tiles[i].terrain == TileTerrain::FrozenGround ||
             tiles[i].terrain == TileTerrain::ThawedGround) &&
            RandFloat() < params.geothermalFreq)
        {
            tiles[i].terrain = TileTerrain::Geothermal;
            tiles[i].resourceAmount = static_cast<uint8_t>(100 + Rand() % 155);
        }
    }

    // Step 5: Place some impassable obstacles (cliffs/craters) on frozen tiles
    int numObstacles = totalTiles / 100;
    for (int i = 0; i < numObstacles; ++i)
    {
        int idx = static_cast<int>(Rand() % totalTiles);
        if (tiles[idx].terrain == TileTerrain::FrozenGround)
            tiles[idx].terrain = TileTerrain::Impassable;
    }
}

// ── OGM File I/O ─────────────────────────────────────────────

bool MapGenerator::SaveToFile(const std::string& path, const MapTile* tiles, int w, int h)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    // Header
    const char magic[4] = {'O', 'G', 'M', 0};
    fwrite(magic, 1, 4, f);

    uint16_t version = 1;
    uint16_t width   = static_cast<uint16_t>(w);
    uint16_t height  = static_cast<uint16_t>(h);
    fwrite(&version, 2, 1, f);
    fwrite(&width,  2, 1, f);
    fwrite(&height, 2, 1, f);

    // Body
    int count = w * h;
    for (int i = 0; i < count; ++i) fwrite(&tiles[i].terrain, 1, 1, f);
    for (int i = 0; i < count; ++i) fwrite(&tiles[i].resourceAmount, 1, 1, f);
    for (int i = 0; i < count; ++i) fwrite(&tiles[i].flags, 1, 1, f);

    fclose(f);
    return true;
}

bool MapGenerator::LoadFromFile(const std::string& path, std::vector<MapTile>& outTiles, int& outW, int& outH)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "OGM", 3) != 0)
    {
        fclose(f);
        return false;
    }

    uint16_t version, width, height;
    fread(&version, 2, 1, f);
    fread(&width,  2, 1, f);
    fread(&height, 2, 1, f);

    outW = width;
    outH = height;
    int count = outW * outH;
    outTiles.resize(count);

    for (int i = 0; i < count; ++i) {
        uint8_t t; fread(&t, 1, 1, f); outTiles[i].terrain = static_cast<TileTerrain>(t);
    }
    for (int i = 0; i < count; ++i) fread(&outTiles[i].resourceAmount, 1, 1, f);
    for (int i = 0; i < count; ++i) fread(&outTiles[i].flags, 1, 1, f);

    fclose(f);
    return true;
}

// ── JSON Map Format (.ogm.json) ─────────────────────────────

bool MapGenerator::SaveToJson(const std::string& path, const MapTile* tiles, int w, int h, int seed)
{
    // Simple JSON writing without nlohmann dependency in engine
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"format\": \"ogm.json\",\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"width\": %d,\n", w);
    fprintf(f, "  \"height\": %d,\n", h);
    fprintf(f, "  \"seed\": %d,\n", seed);

    // Terrain array
    fprintf(f, "  \"terrain\": [");
    int count = w * h;
    for (int i = 0; i < count; ++i)
    {
        fprintf(f, "%d", static_cast<int>(tiles[i].terrain));
        if (i < count - 1) fprintf(f, ",");
    }
    fprintf(f, "],\n");

    // Resources array
    fprintf(f, "  \"resources\": [");
    for (int i = 0; i < count; ++i)
    {
        fprintf(f, "%d", tiles[i].resourceAmount);
        if (i < count - 1) fprintf(f, ",");
    }
    fprintf(f, "],\n");

    // Overlay (flags)
    fprintf(f, "  \"overlay\": [");
    for (int i = 0; i < count; ++i)
    {
        fprintf(f, "%d", tiles[i].flags);
        if (i < count - 1) fprintf(f, ",");
    }
    fprintf(f, "]\n");

    fprintf(f, "}\n");
    fclose(f);
    return true;
}

bool MapGenerator::LoadFromJson(const std::string& path, std::vector<MapTile>& outTiles, int& outW, int& outH, int& outSeed)
{
    // Use a simple line-by-line parser for the JSON format
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;

    // Read entire file
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }

    std::string content(sz, '\0');
    fread(&content[0], 1, sz, f);
    fclose(f);

    // Simple parser — extract width, height, seed, terrain[], resources[], overlay[]
    auto findInt = [&](const std::string& key, int defaultVal) -> int {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return defaultVal;
        pos++;
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n')) pos++;
        return atoi(&content[pos]);
    };

    auto parseArray = [&](const std::string& key, std::vector<int>& out) {
        auto pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return;
        pos = content.find('[', pos);
        if (pos == std::string::npos) return;
        pos++;
        while (pos < content.size() && content[pos] != ']')
        {
            if (content[pos] >= '0' && content[pos] <= '9')
            {
                out.push_back(atoi(&content[pos]));
                while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') pos++;
            }
            else pos++;
        }
    };

    outW = findInt("width", 32);
    outH = findInt("height", 32);
    outSeed = findInt("seed", 42);

    std::vector<int> terrain, resources, overlay;
    parseArray("terrain", terrain);
    parseArray("resources", resources);
    parseArray("overlay", overlay);

    int count = outW * outH;
    outTiles.resize(count);
    for (int i = 0; i < count && i < static_cast<int>(terrain.size()); ++i)
    {
        outTiles[i].terrain = static_cast<TileTerrain>(terrain[i]);
        if (i < static_cast<int>(resources.size()))
            outTiles[i].resourceAmount = static_cast<uint8_t>(resources[i]);
        if (i < static_cast<int>(overlay.size()))
            outTiles[i].flags = static_cast<uint8_t>(overlay[i]);
    }

    return true;
}

} // namespace beigebox
