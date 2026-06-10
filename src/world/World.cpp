#include "world/World.h"

#include "world/WorldGen.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
int floor_divide(int value, int divisor) {
    int result = value / divisor;
    if (value % divisor < 0) {
        --result;
    }
    return result;
}

int positive_modulo(int value, int divisor) {
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

std::size_t tile_index(Int2 local) {
    return static_cast<std::size_t>(local.y * chunk_size + local.x);
}

std::size_t edge_index(Direction direction) {
    return static_cast<std::size_t>(direction);
}
} // namespace

World::World(std::uint64_t seed, int loading_radius)
    : seed_(seed), loading_radius_(std::max(0, loading_radius)) {
}

const Tile* World::get_tile(Int2 position) const {
    const auto chunk = chunks_.find(world_to_chunk(position));
    if (chunk == chunks_.end()) {
        return nullptr;
    }
    return &chunk->second.tiles[tile_index(world_to_local(position))];
}

Tile* World::get_tile(Int2 position) {
    const auto chunk = chunks_.find(world_to_chunk(position));
    if (chunk == chunks_.end()) {
        return nullptr;
    }
    return &chunk->second.tiles[tile_index(world_to_local(position))];
}

bool World::is_walkable(Int2 position) const {
    const Tile* tile = get_tile(position);
    return tile != nullptr && tile->walkable && !tile->blocking;
}

void World::set_tile(Int2 position, Tile tile) {
    ensure_tile_loaded(position);
    chunks_.at(world_to_chunk(position)).tiles[tile_index(world_to_local(position))] = tile;
    tile_overrides_.insert_or_assign(position, std::move(tile));
}

void World::restore_generated_terrain(Int2 position) {
    ensure_tile_loaded(position);
    const Tile existing = *get_tile(position);
    Tile generated = worldgen::generate_tile(seed_, position);
    generated.edges = existing.edges;
    set_tile(position, std::move(generated));
}

void World::set_edge(Int2 position, Direction direction, EdgeObject edge) {
    const Int2 neighbor_position{
        position.x + direction_offset(direction).x,
        position.y + direction_offset(direction).y,
    };
    ensure_tile_loaded(position);
    ensure_tile_loaded(neighbor_position);

    Tile current = *get_tile(position);
    Tile neighbor = *get_tile(neighbor_position);
    current.edges[edge_index(direction)] = edge;
    neighbor.edges[edge_index(opposite_direction(direction))] = edge;
    set_tile(position, std::move(current));
    set_tile(neighbor_position, std::move(neighbor));
}

EdgeObject World::get_edge(Int2 position, Direction direction) const {
    const Tile* tile = get_tile(position);
    return tile ? tile->edges[edge_index(direction)] : EdgeObject{};
}

void World::update_loaded_chunks(Int2 player_position) {
    const Int2 center = world_to_chunk(player_position);

    for (int y = center.y - loading_radius_; y <= center.y + loading_radius_; ++y) {
        for (int x = center.x - loading_radius_; x <= center.x + loading_radius_; ++x) {
            load_chunk({x, y});
        }
    }

    std::erase_if(chunks_, [&](const auto& entry) {
        const Int2 coordinate = entry.first;
        return std::abs(coordinate.x - center.x) > loading_radius_
            || std::abs(coordinate.y - center.y) > loading_radius_;
    });
}

void World::regenerate(std::uint64_t seed, Int2 player_position) {
    seed_ = seed;
    chunks_.clear();
    tile_overrides_.clear();
    update_loaded_chunks(player_position);
}

void World::set_loading_radius(int loading_radius) {
    loading_radius_ = std::max(0, loading_radius);
}

Int2 World::find_walkable_spawn() {
    constexpr int max_spawn_search_radius = 512;
    for (int radius = 0; radius <= max_spawn_search_radius; ++radius) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (radius > 0 && std::abs(x) != radius && std::abs(y) != radius) {
                    continue;
                }
                const Int2 position{x, y};
                update_loaded_chunks(position);
                if (is_walkable(position)) {
                    return position;
                }
            }
        }
    }
    throw std::runtime_error("Could not find a walkable spawn near the world origin.");
}

void World::load_chunk(Int2 chunk_coordinate) {
    if (chunks_.contains(chunk_coordinate)) {
        return;
    }

    Chunk chunk{.coordinate = chunk_coordinate};
    for (int local_y = 0; local_y < chunk_size; ++local_y) {
        for (int local_x = 0; local_x < chunk_size; ++local_x) {
            const Int2 world_position{
                chunk_coordinate.x * chunk_size + local_x,
                chunk_coordinate.y * chunk_size + local_y,
            };
            const auto override = tile_overrides_.find(world_position);
            chunk.tiles[tile_index({local_x, local_y})] = override == tile_overrides_.end()
                ? worldgen::generate_tile(seed_, world_position)
                : override->second;
        }
    }
    chunks_.emplace(chunk_coordinate, std::move(chunk));
}

void World::ensure_tile_loaded(Int2 position) {
    load_chunk(world_to_chunk(position));
}

Int2 world_to_chunk(Int2 world_position) {
    return {
        floor_divide(world_position.x, chunk_size),
        floor_divide(world_position.y, chunk_size),
    };
}

Int2 world_to_local(Int2 world_position) {
    return {
        positive_modulo(world_position.x, chunk_size),
        positive_modulo(world_position.y, chunk_size),
    };
}

Vec3 tile_to_world_position(int tile_x, int tile_y, int elevation) {
    return {
        static_cast<float>(tile_x) * TILE_SIZE,
        static_cast<float>(elevation) * TILE_HEIGHT,
        static_cast<float>(tile_y) * TILE_SIZE,
    };
}

Vec3 tile_to_world_position(const World& world, Int2 tile_position) {
    const Tile* tile = world.get_tile(tile_position);
    return tile_to_world_position(tile_position.x, tile_position.y, tile ? tile->elevation : 0);
}

float get_surface_world_y(const World& world, int tile_x, int tile_y) {
    const Tile* tile = world.get_tile({tile_x, tile_y});
    return tile ? static_cast<float>(tile->elevation) * TILE_HEIGHT : 0.0F;
}

bool can_move_between_tiles(const World& world, Int2 from, Int2 to, int max_step_height) {
    const Tile* current = world.get_tile(from);
    const Tile* target = world.get_tile(to);
    Direction direction;
    if (to == Int2{from.x, from.y - 1}) {
        direction = Direction::north;
    } else if (to == Int2{from.x + 1, from.y}) {
        direction = Direction::east;
    } else if (to == Int2{from.x, from.y + 1}) {
        direction = Direction::south;
    } else if (to == Int2{from.x - 1, from.y}) {
        direction = Direction::west;
    } else {
        return false;
    }
    const EdgeObject edge = world.get_edge(from, direction);
    const bool edge_blocks = edge.type == EdgeType::wall
        || edge.type == EdgeType::window
        || (edge.type == EdgeType::door && !edge.door_open);
    return current != nullptr
        && target != nullptr
        && target->walkable
        && !target->blocking
        && !edge_blocks
        && std::abs(target->elevation - current->elevation) <= max_step_height;
}

const char* terrain_name(TerrainType terrain) {
    switch (terrain) {
    case TerrainType::deep_water: return "deep water";
    case TerrainType::shallow_water: return "shallow water";
    case TerrainType::sand: return "sand";
    case TerrainType::grass: return "grass";
    case TerrainType::forest: return "forest";
    case TerrainType::rock: return "rock";
    case TerrainType::mountain: return "mountain";
    case TerrainType::floor: return "floor";
    }
    return "unknown";
}

const char* edge_type_name(EdgeType edge) {
    switch (edge) {
    case EdgeType::empty: return "empty";
    case EdgeType::wall: return "wall";
    case EdgeType::door: return "door";
    case EdgeType::window: return "window";
    }
    return "unknown";
}

Int2 direction_offset(Direction direction) {
    switch (direction) {
    case Direction::north: return {0, -1};
    case Direction::east: return {1, 0};
    case Direction::south: return {0, 1};
    case Direction::west: return {-1, 0};
    }
    return {};
}

Direction opposite_direction(Direction direction) {
    switch (direction) {
    case Direction::north: return Direction::south;
    case Direction::east: return Direction::west;
    case Direction::south: return Direction::north;
    case Direction::west: return Direction::east;
    }
    return Direction::north;
}
