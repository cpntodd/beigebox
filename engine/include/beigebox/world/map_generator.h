// engine/include/beigebox/world/map_generator.h
// ─────────────────────────────────────────────────────────────
// Procedural Tile Map Generator
//
// Generates isometric tile grids with terrain types, resource
// deposits, and geothermal features. Outputs .ogm (Open Game
// Map) binary format.
//
// OpenGS Maptool (https://github.com/Thomas-Holtvedt/opengs-maptool)
// can be used for grand-strategy province maps. This generator
// handles RTS-style tile grids.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace beigebox {

enum class TileTerrain : uint8_t
{
    FrozenGround  = 0,  // Default — frozen, impassable until thawed
    ThawedGround  = 1,  // Passable, buildable
    SalvageField  = 2,  // Contains salvage resources
    Geothermal    = 3,  // Natural heat vent
    Impassable    = 4   // Permanent obstacle (cliff, crater)
};

struct MapTile
{
    TileTerrain terrain = TileTerrain::FrozenGround;
    uint8_t     resourceAmount = 0;   // 0-255 for salvage/heat
    uint8_t     flags = 0;            // bitfield for future use
};

class MapGenerator
{
public:
    // ── Generation Parameters ────────────────────────────────
    struct Params {
        int width        = 64;
        int height       = 64;
        int seed         = 42;
        float salvageDensity  = 0.15f;  // 0-1 fraction of tiles with salvage
        float geothermalFreq  = 0.05f;  // 0-1 fraction of tiles with geothermal
    };

    // Generate a tile map into the given buffer (must be width*height).
    void Generate(MapTile* tiles, const Params& params);

    // ── OGM File Format ──────────────────────────────────────
    // Binary format:
    //   Header:  "OGM\0" (4 bytes)
    //            version  (2 bytes, little-endian) = 1
    //            width    (2 bytes, little-endian)
    //            height   (2 bytes, little-endian)
    //   Body:    width*height bytes of TileTerrain values
    //            width*height bytes of resourceAmount values
    //            width*height bytes of flags

    static bool SaveToFile(const std::string& path, const MapTile* tiles, int w, int h);
    static bool LoadFromFile(const std::string& path, std::vector<MapTile>& outTiles, int& outW, int& outH);
};

} // namespace beigebox
