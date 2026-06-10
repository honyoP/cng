#include "renderer/ChunkMesh.h"

#include <algorithm>
#include <bit>

namespace {
struct Color {
    float red;
    float green;
    float blue;
};

Color terrain_color(TerrainType terrain, float shade = 1.0F) {
    switch (terrain) {
    case TerrainType::deep_water: return {0.04F * shade, 0.16F * shade, 0.38F * shade};
    case TerrainType::shallow_water: return {0.10F * shade, 0.38F * shade, 0.62F * shade};
    case TerrainType::sand: return {0.76F * shade, 0.68F * shade, 0.42F * shade};
    case TerrainType::grass: return {0.26F * shade, 0.50F * shade, 0.22F * shade};
    case TerrainType::forest: return {0.09F * shade, 0.30F * shade, 0.12F * shade};
    case TerrainType::rock: return {0.42F * shade, 0.43F * shade, 0.40F * shade};
    case TerrainType::mountain: return {0.30F * shade, 0.31F * shade, 0.32F * shade};
    case TerrainType::floor: return {0.54F * shade, 0.42F * shade, 0.28F * shade};
    }
    return {1.0F, 0.0F, 1.0F};
}

void add_triangle(std::vector<TerrainVertex>& vertices, Vec3 a, Vec3 b, Vec3 c, Color color) {
    vertices.push_back({a, color.red, color.green, color.blue});
    vertices.push_back({b, color.red, color.green, color.blue});
    vertices.push_back({c, color.red, color.green, color.blue});
}

void add_quad(std::vector<TerrainVertex>& vertices, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color color) {
    add_triangle(vertices, a, b, c, color);
    add_triangle(vertices, a, c, d, color);
}

float neighbor_surface_or(const World& world, Int2 position, float fallback) {
    const Tile* tile = world.get_tile(position);
    if (tile) {
        return get_surface_world_y(world, position.x, position.y);
    }
    // Draw a temporary outer edge. Loading the neighbor changes the signature and rebuilds this mesh.
    return std::min(fallback, static_cast<float>(MIN_TERRAIN_ELEVATION - 1) * TILE_HEIGHT);
}

void hash_combine(std::uint64_t& hash, std::uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
}

void hash_tile(std::uint64_t& hash, const Tile* tile) {
    if (!tile) {
        hash_combine(hash, 0xFFFFFFFFFFFFFFFFULL);
        return;
    }
    hash_combine(hash, static_cast<std::uint64_t>(tile->terrain));
    hash_combine(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(tile->elevation)));
    hash_combine(hash, std::bit_cast<std::uint32_t>(tile->movement_cost));
    hash_combine(hash, static_cast<std::uint64_t>(tile->walkable));
    hash_combine(hash, static_cast<std::uint64_t>(tile->blocking));
}
} // namespace

std::uint64_t chunk_mesh::source_signature(const World& world, Int2 chunk_coordinate) {
    std::uint64_t signature = 1469598103934665603ULL;
    const int start_x = chunk_coordinate.x * chunk_size;
    const int start_y = chunk_coordinate.y * chunk_size;

    for (int local_y = 0; local_y < chunk_size; ++local_y) {
        for (int local_x = 0; local_x < chunk_size; ++local_x) {
            hash_tile(signature, world.get_tile({start_x + local_x, start_y + local_y}));
        }
    }
    // Border neighbor heights affect side-face visibility, so include them in cache invalidation.
    for (int offset = 0; offset < chunk_size; ++offset) {
        hash_tile(signature, world.get_tile({start_x + offset, start_y - 1}));
        hash_tile(signature, world.get_tile({start_x + offset, start_y + chunk_size}));
        hash_tile(signature, world.get_tile({start_x - 1, start_y + offset}));
        hash_tile(signature, world.get_tile({start_x + chunk_size, start_y + offset}));
    }
    return signature;
}

ChunkMeshData chunk_mesh::build(const World& world, Int2 chunk_coordinate) {
    ChunkMeshData mesh;
    mesh.source_signature = source_signature(world, chunk_coordinate);
    mesh.vertices.reserve(static_cast<std::size_t>(chunk_size * chunk_size * 12));

    const int start_x = chunk_coordinate.x * chunk_size;
    const int start_y = chunk_coordinate.y * chunk_size;
    const float half_tile = TILE_SIZE * 0.5F;

    for (int local_y = 0; local_y < chunk_size; ++local_y) {
        for (int local_x = 0; local_x < chunk_size; ++local_x) {
            const Int2 tile_position{start_x + local_x, start_y + local_y};
            const Tile* tile = world.get_tile(tile_position);
            if (!tile) {
                continue;
            }

            const Vec3 center = tile_to_world_position(tile_position.x, tile_position.y, tile->elevation);
            const float left = center.x - half_tile;
            const float right = center.x + half_tile;
            const float front = center.z - half_tile;
            const float back = center.z + half_tile;
            const float top = center.y;

            add_quad(mesh.vertices,
                {left, top, front}, {left, top, back}, {right, top, back}, {right, top, front},
                terrain_color(tile->terrain));

            const auto add_side_if_visible = [&](float neighbor_y, float shade, Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
                if (neighbor_y < top) {
                    a.y = neighbor_y;
                    b.y = neighbor_y;
                    add_quad(mesh.vertices, a, b, c, d, terrain_color(tile->terrain, shade));
                }
            };

            add_side_if_visible(neighbor_surface_or(world, {tile_position.x, tile_position.y - 1}, top), 0.72F,
                {right, 0.0F, front}, {left, 0.0F, front}, {left, top, front}, {right, top, front});
            add_side_if_visible(neighbor_surface_or(world, {tile_position.x, tile_position.y + 1}, top), 0.62F,
                {left, 0.0F, back}, {right, 0.0F, back}, {right, top, back}, {left, top, back});
            add_side_if_visible(neighbor_surface_or(world, {tile_position.x - 1, tile_position.y}, top), 0.66F,
                {left, 0.0F, front}, {left, 0.0F, back}, {left, top, back}, {left, top, front});
            add_side_if_visible(neighbor_surface_or(world, {tile_position.x + 1, tile_position.y}, top), 0.78F,
                {right, 0.0F, back}, {right, 0.0F, front}, {right, top, front}, {right, top, back});
        }
    }
    return mesh;
}
