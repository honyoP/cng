#pragma once

#include "core/Types.h"
#include "world/World.h"

#include <cstdint>

namespace worldgen {

[[nodiscard]] float value_noise(std::uint64_t seed, float x, float y);
[[nodiscard]] float fbm(std::uint64_t seed, float x, float y, int octaves);
[[nodiscard]] Tile generate_tile(std::uint64_t seed, Int2 world_position);

} // namespace worldgen
