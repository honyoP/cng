#include "world/WorldGen.h"

#include <algorithm>
#include <cmath>

namespace {
std::uint64_t mix(std::uint64_t value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

float lattice_value(std::uint64_t seed, int x, int y) {
    const auto ux = static_cast<std::uint64_t>(static_cast<std::int64_t>(x));
    const auto uy = static_cast<std::uint64_t>(static_cast<std::int64_t>(y));
    const std::uint64_t hash = mix(seed ^ mix(ux) ^ (mix(uy) << 1U));
    return static_cast<float>(hash >> 40U) / static_cast<float>(0xFFFFFFU);
}

float smooth(float value) {
    return value * value * (3.0F - 2.0F * value);
}

float lerp(float from, float to, float amount) {
    return from + (to - from) * amount;
}

float smoother_step(float value) {
    value = std::clamp(value, 0.0F, 1.0F);
    return value * value * value * (value * (value * 6.0F - 15.0F) + 10.0F);
}
} // namespace

float worldgen::value_noise(std::uint64_t seed, float x, float y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = smooth(x - static_cast<float>(x0));
    const float ty = smooth(y - static_cast<float>(y0));

    const float top = lerp(lattice_value(seed, x0, y0), lattice_value(seed, x0 + 1, y0), tx);
    const float bottom = lerp(lattice_value(seed, x0, y0 + 1), lattice_value(seed, x0 + 1, y0 + 1), tx);
    return lerp(top, bottom, ty);
}

float worldgen::fbm(std::uint64_t seed, float x, float y, int octaves) {
    float value = 0.0F;
    float amplitude = 0.5F;
    float amplitude_sum = 0.0F;
    for (int octave = 0; octave < octaves; ++octave) {
        value += value_noise(seed + static_cast<std::uint64_t>(octave) * 7919ULL, x, y) * amplitude;
        amplitude_sum += amplitude;
        x *= 2.0F;
        y *= 2.0F;
        amplitude *= 0.5F;
    }
    return value / amplitude_sum;
}

Tile worldgen::generate_tile(std::uint64_t seed, Int2 world_position) {
    // TODO: Replace hard-coded thresholds with biome data before adding rivers, roads, or settlements.
    const float x = static_cast<float>(world_position.x);
    const float y = static_cast<float>(world_position.y);

    const float continental = fbm(seed ^ 0xA341316CULL, x / 260.0F, y / 260.0F, 5);
    const float detail = fbm(seed ^ 0xC8013EA4ULL, x / 72.0F, y / 72.0F, 4);
    const float moisture = fbm(seed ^ 0xAD90777DULL, x / 95.0F, y / 95.0F, 4);
    const float raw_elevation = std::clamp(continental * 0.86F + detail * 0.14F, 0.0F, 1.0F);
    const float shaped_elevation = smoother_step(raw_elevation);
    const int elevation_range = MAX_TERRAIN_ELEVATION - MIN_TERRAIN_ELEVATION;
    const int elevation = MIN_TERRAIN_ELEVATION
        + static_cast<int>(std::round(shaped_elevation * static_cast<float>(elevation_range)));

    Tile tile;
    tile.elevation = elevation;
    if (elevation <= -1) {
        tile.terrain = TerrainType::deep_water;
        tile.walkable = false;
        tile.movement_cost = 0.0F;
        tile.blocking = true;
    } else if (elevation == 0) {
        tile.terrain = TerrainType::shallow_water;
        tile.movement_cost = 2.5F;
    } else if (elevation == 1) {
        tile.terrain = TerrainType::sand;
        tile.movement_cost = 1.25F;
    } else if (elevation >= 7) {
        tile.terrain = TerrainType::mountain;
        tile.walkable = false;
        tile.movement_cost = 0.0F;
        tile.blocking = true;
    } else if (elevation >= 5) {
        tile.terrain = TerrainType::rock;
        tile.movement_cost = 1.6F;
    } else if (moisture > 0.56F && elevation >= 3) {
        tile.terrain = TerrainType::forest;
        tile.movement_cost = 1.8F;
    } else {
        tile.terrain = TerrainType::grass;
    }
    return tile;
}
