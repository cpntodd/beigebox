// tests/test_map.cpp
// ─────────────────────────────────────────────────────────────
// Unit tests for the procedural map generator and .ogm format.
// ─────────────────────────────────────────────────────────────

#include <doctest/doctest.h>

#include "beigebox/world/map_generator.h"

#include <cstdio>
#include <vector>

using namespace beigebox;

TEST_CASE("MapGenerator — Generate")
{
    MapGenerator gen;
    MapGenerator::Params params;
    params.width = 10;
    params.height = 10;
    params.seed = 42;
    params.salvageDensity = 0.2f;
    params.geothermalFreq = 0.1f;

    std::vector<MapTile> tiles(params.width * params.height);
    gen.Generate(tiles.data(), params);

    // Count tile types
    int frozen = 0, thawed = 0, salvage = 0, geo = 0, impassable = 0;
    for (auto& t : tiles)
    {
        switch (t.terrain)
        {
        case TileTerrain::FrozenGround: frozen++; break;
        case TileTerrain::ThawedGround: thawed++; break;
        case TileTerrain::SalvageField: salvage++; break;
        case TileTerrain::Geothermal:   geo++; break;
        case TileTerrain::Impassable:   impassable++; break;
        }
    }

    // Should have a mix — not all frozen
    CHECK(thawed > 0);
    CHECK(frozen > 0);
    // Total should match
    CHECK(frozen + thawed + salvage + geo + impassable == 100);
}

TEST_CASE("MapGenerator — Determinism")
{
    MapGenerator gen;
    MapGenerator::Params params;
    params.width = 16;
    params.height = 16;
    params.seed = 12345;

    std::vector<MapTile> tilesA(params.width * params.height);
    std::vector<MapTile> tilesB(params.width * params.height);

    gen.Generate(tilesA.data(), params);
    gen.Generate(tilesB.data(), params);

    // Same seed → identical output (deterministic)
    for (size_t i = 0; i < tilesA.size(); ++i)
    {
        CHECK(tilesA[i].terrain == tilesB[i].terrain);
        CHECK(tilesA[i].resourceAmount == tilesB[i].resourceAmount);
    }
}

TEST_CASE("MapGenerator — OGM save/load roundtrip")
{
    MapGenerator gen;
    MapGenerator::Params params;
    params.width = 8;
    params.height = 8;
    params.seed = 99;

    std::vector<MapTile> tiles(params.width * params.height);
    gen.Generate(tiles.data(), params);

    // Save to temp file
    const char* tmpPath = "/tmp/beigebox_test.ogm";
    bool saved = MapGenerator::SaveToFile(tmpPath, tiles.data(), params.width, params.height);
    CHECK(saved);

    // Load back
    std::vector<MapTile> loaded;
    int lw = 0, lh = 0;
    bool loadedOk = MapGenerator::LoadFromFile(tmpPath, loaded, lw, lh);
    CHECK(loadedOk);
    CHECK(lw == params.width);
    CHECK(lh == params.height);

    // Compare all tiles
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        CHECK(loaded[i].terrain == tiles[i].terrain);
        CHECK(loaded[i].resourceAmount == tiles[i].resourceAmount);
        CHECK(loaded[i].flags == tiles[i].flags);
    }

    std::remove(tmpPath);
}
