#pragma once

#include "world/World.h"

#include <cstdint>
#include <vector>

struct TerrainVertex {
    Vec3 position{};
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
};

struct ChunkMeshData {
    std::vector<TerrainVertex> vertices;
    std::uint64_t source_signature = 0;
};

namespace chunk_mesh {

[[nodiscard]] std::uint64_t source_signature(const World& world, Int2 chunk_coordinate);
[[nodiscard]] ChunkMeshData build(const World& world, Int2 chunk_coordinate);

} // namespace chunk_mesh
